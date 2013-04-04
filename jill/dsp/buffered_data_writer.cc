#include <iostream>
#include "buffered_data_writer.hh"
#include "period_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;

#if DEBUG > 1
#include <fstream>
std::ofstream plog("period_log.txt");
#endif

buffered_data_writer::buffered_data_writer(boost::shared_ptr<data_writer> writer, nframes_t buffer_size)
        : _xrun(false),
          _entry_close(false),
          _writer(writer),
          _buffer(new dsp::period_ringbuffer(buffer_size)),
          _stop(false)
{
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
}

buffered_data_writer::~buffered_data_writer()
{
        // need to make sure synchrons are not in use
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
        // pthread_cancel(_thread_id);
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
}

nframes_t
buffered_data_writer::push(void const * arg, period_info_t const & info)
{
        nframes_t r;
        if (_stop || _xrun) r = info.nframes;
        else r = _buffer->push(arg, info);
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
        if (__sync_bool_compare_and_swap(&_xrun, false, true))
                signal_writer();
}

void
buffered_data_writer::close_entry(nframes_t)
{
        __sync_bool_compare_and_swap(&_entry_close, false, true);
}

void
buffered_data_writer::stop()
{
        __sync_bool_compare_and_swap(&_stop, false, true);
        signal_writer();
}

void
buffered_data_writer::start()
{
        _xrun = false;
        _stop = false;
        _entry_close = false;
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
#if DEBUG > 1
                plog << "\nbdw: x=" << self->_xrun << ", s=" << self->_stop << ", ec=" << self->_entry_close;
#endif
                period = self->_buffer->peek_ahead();
                self->write(period);
                /* handle empty ringbuf according to state */
                if (period == 0) {
                        if (self->_stop) {
                                self->_writer->close_entry();
                                break;
                        }
                        else {
#if DEBUG > 1
                                plog << ", flushing";
#endif
                                // use this opportunity to flush
                                self->_writer->flush();
                                // then wait for more data
                                pthread_cond_wait (&self->_ready, &self->_lock);
                        }
                }
        }
        pthread_mutex_unlock(&self->_lock);
        __sync_bool_compare_and_swap(&self->_stop, true, false);
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
buffered_data_writer::write(period_info_t const * period)
{
        /* no data */
        if (period == 0) {
                if (_xrun) {
                        /* buffer is flushed; clear xrun state */
                        _writer->xrun();
                        _writer->close_entry(); // new entry
                        __sync_bool_compare_and_swap(&_xrun, true, false);
                }
                return;
        }
        /* handle entry close requests and overflows in first period of the channel */
        if (_writer->aligned()) {
                if (_entry_close) {
                        _writer->close_entry();
                        __sync_bool_compare_and_swap(&_entry_close, true, false);
                }
                else if  (period->time < period->nframes) {
                        _writer->close_entry();
                }
        }
#if DEBUG > 1
        plog << ", period t=" << period->time;
#endif
        _writer->write(period);

        /* release data */
        _buffer->release();
}
