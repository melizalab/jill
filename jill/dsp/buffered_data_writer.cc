#include <iostream>
#include "buffered_data_writer.hh"
#include "period_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;

buffered_data_writer::buffered_data_writer(boost::shared_ptr<data_writer> writer, nframes_t buffer_size)
        : _stop(0), _xruns(0), _entry_close(0),
          _writer(writer),
          _buffer(new dsp::period_ringbuffer(buffer_size))
{
        // redirect the data_writer's logstream to this class
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
}

buffered_data_writer::~buffered_data_writer()
{
        // bad stuff could happen if the thread is not joined
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
}

nframes_t
buffered_data_writer::push(void const * arg, period_info_t const & info)
{
        if (_stop) return 0;
        nframes_t r = _buffer->push(arg, info);
        signal_writer();
        return r;
}

nframes_t
buffered_data_writer::write_space(nframes_t nframes) const
{
        return _buffer->write_space(nframes);
}

void
buffered_data_writer::xrun()
{
        __sync_add_and_fetch(&_xruns, 1);
        signal_writer();
}

void
buffered_data_writer::close_entry(nframes_t)
{
        __sync_add_and_fetch(&_entry_close,1);
}

void
buffered_data_writer::stop()
{
        __sync_add_and_fetch(&_stop, 1);
        signal_writer();
}

void
buffered_data_writer::start()
{
        int ret = pthread_create(&_thread_id, NULL, buffered_data_writer::thread, this);
        if (ret != 0)
                throw std::runtime_error("Failed to start writer thread");
}

void
buffered_data_writer::join()
{
        pthread_join(_thread_id, NULL);
}

nframes_t
buffered_data_writer::resize_buffer(nframes_t nframes, nframes_t nchannels)
{
        nframes_t nsamples = nframes * nchannels;
        if (nsamples <= _buffer->size()) return _buffer->size();
        // the write thread will keep this locked until the buffer is empty
        pthread_mutex_lock(&_lock);
        _buffer->resize(nsamples);
        pthread_mutex_unlock(&_lock);
        return _buffer->size();
}

void *
buffered_data_writer::thread(void * arg)
{
        buffered_data_writer * self = static_cast<buffered_data_writer *>(arg);
        period_info_t const * period;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&self->_lock);

        while (1) {
                period = self->_buffer->peek_ahead();
                if (period) {
                        self->write(period);
                }
                else if (!self->_stop) {
                        // wait for more data
                        pthread_cond_wait (&self->_ready, &self->_lock);
                }
                else {
                        self->_writer->close_entry();
                        break;
                }
        }
        pthread_mutex_unlock(&self->_lock);
        return 0;
}

void
buffered_data_writer::signal_writer()
{
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}

void
buffered_data_writer::write(period_info_t const * info)
{
        /* handle xruns and overflows in first period of the channel */
        if (_writer->aligned()) {
                if (_xruns) {
                        _writer->xrun();
                        _writer->close_entry(); // new entry
                        __sync_add_and_fetch(&_xruns, -1);
                }
                else if (_entry_close) {
                        _writer->close_entry();
                        __sync_add_and_fetch(&_entry_close, -1);
                }
                else if  (info->time < info->nframes) {
                        _writer->close_entry();
                }
        }

        _writer->write(info);

        /* release data */
        _buffer->release();
}

