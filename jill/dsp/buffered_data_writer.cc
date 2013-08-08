#include <iostream>
#include "buffered_data_writer.hh"
#include "block_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;
using std::size_t;

buffered_data_writer::buffered_data_writer(boost::shared_ptr<data_writer> writer, nframes_t buffer_size)
        : _state(Stopped),
          _reset(false),
          _writer(writer),
          _buffer(new block_ringbuffer(buffer_size))
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

void
buffered_data_writer::push(nframes_t time, dtype_t dtype, char const * id,
                           size_t size, void const * data)
{
        switch (_state) {
        case Running:
        case Stopped:
                if (_buffer->push(time, dtype, id, size, data) == 0) {
                        xrun();
                }
                break;
        case Xrun:
#if DEBUG > 1
                std::cerr << "in Xrun: frame " << time << " discarded" << std::endl;
                break;
#endif
        case Stopping:
                break;
        }
}

void
buffered_data_writer::data_ready()
{
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}


void
buffered_data_writer::xrun()
{
        __sync_bool_compare_and_swap(&_state, Running, Xrun);
}

void
buffered_data_writer::stop()
{
        (__sync_bool_compare_and_swap(&_state, Running, Stopping) ||
         __sync_bool_compare_and_swap(&_state, Xrun, Stopping));
        // release condition variable to prevent deadlock
        data_ready();
}


void
buffered_data_writer::reset()
{
        if (_state == Running)
                __sync_bool_compare_and_swap(&_reset, false, true);
}


void
buffered_data_writer::start()
{
        if (_state == Stopped) {
                int ret = pthread_create(&_thread_id, NULL, buffered_data_writer::thread, this);
                if (ret != 0)
                        throw std::runtime_error("Failed to start writer thread");
        }
}

void
buffered_data_writer::join()
{
        pthread_join(_thread_id, NULL);
}

size_t
buffered_data_writer::request_buffer_size(size_t bytes)
{
        // the write thread will keep this locked until the buffer is empty
        pthread_mutex_lock(&_lock);
        if (bytes > _buffer->size()) {
                _buffer->resize(bytes);
        }
        pthread_mutex_unlock(&_lock);
        return _buffer->size();
}

void *
buffered_data_writer::thread(void * arg)
{
        buffered_data_writer * self = static_cast<buffered_data_writer *>(arg);
        data_block_t const * hdr;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&self->_lock);
        self->_state = Running;

        while (1) {
                hdr = self->_buffer->peek_ahead();
                self->write(hdr);
                if (hdr == 0) {
                        /* if ringbuffer empty and Stopping, close loop */
                        if (self->_state == Stopping) {
                                break;
                        }
                        /* otherwise flush to disk and wait for more data */
                        else {
                                self->_writer->flush();
                                pthread_cond_wait (&self->_ready, &self->_lock);
                        }
                }
        }
        self->_writer->close_entry();
        pthread_mutex_unlock(&self->_lock);
        self->_state = Stopped;
        return 0;
}

void
buffered_data_writer::write(data_block_t const * data)
{
        if (data == 0) {
                /* buffer is flushed; close entry and clear xrun state */
                if (__sync_bool_compare_and_swap(&_state, Xrun, Running)) {
                        _writer->xrun();
                        _writer->close_entry();

                }
        }
        else {
                if (__sync_bool_compare_and_swap(&_reset, true, false)) {
                        _writer->close_entry();
                }
                _writer->write(data, 0, 0);
                _buffer->release();
        }
}
