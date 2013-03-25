#include "multichannel_data_thread.hh"
#include "period_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;

multichannel_data_thread::multichannel_data_thread(nframes_t buffer_size)
        : _stop(0), _xruns(0), _buffer(new dsp::period_ringbuffer(buffer_size))
{
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
}

multichannel_data_thread::~multichannel_data_thread()
{
        // bad stuff could happen if the thread is not joined
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
}

nframes_t
multichannel_data_thread::push(void const * arg, period_info_t const & info)
{
        if (_stop) return 0;
        nframes_t r = _buffer->push(arg, info);
        // if (r != info.nframes) xrun();
        // signal writer thread
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
        return r;
}

nframes_t
multichannel_data_thread::write_space(nframes_t nframes) const
{
        return _buffer->write_space(nframes);
}

void
multichannel_data_thread::xrun()
{
        __sync_add_and_fetch(&_xruns, 1);
}

void
multichannel_data_thread::stop()
{
        __sync_add_and_fetch(&_stop, 1);
        // have to notify writer thread if it's waiting
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}

void
multichannel_data_thread::start()
{
        int ret = pthread_create(&_thread_id, NULL, multichannel_data_thread::thread, this);
        if (ret != 0)
                throw std::runtime_error("Failed to start writer thread");
}

void
multichannel_data_thread::join()
{
        pthread_join(_thread_id, NULL);
}

nframes_t
multichannel_data_thread::resize_buffer(nframes_t nframes, nframes_t nchannels)
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
multichannel_data_thread::thread(void * arg)
{
        multichannel_data_thread * self = static_cast<multichannel_data_thread *>(arg);
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
                        break;
                }
        }
        pthread_mutex_unlock(&self->_lock);
        return 0;
}


void
multichannel_data_thread::write(period_info_t const * info)
{
        std::cout << "\rgot period: time=" << info->time << ", nframes=" << info->nframes << std::flush;
        _buffer->release();
}
