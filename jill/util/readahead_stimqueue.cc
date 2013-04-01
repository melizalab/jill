#include "../event_logger.hh"
#include "readahead_stimqueue.hh"

using namespace jill::util;

readahead_stimqueue::readahead_stimqueue(iterator first, iterator last,
                                         nframes_t samplerate,
                                         boost::shared_ptr<jill::event_logger> logger,
                                         bool loop)
        :  _first(first), _last(last), _it(first), _head(0),
          _log(logger),_samplerate(samplerate), _loop(loop)
{
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
        int ret = pthread_create(&_thread_id, NULL, readahead_stimqueue::thread, this);
        if (ret != 0)
                throw std::runtime_error("Failed to start writer thread");
}

readahead_stimqueue::~readahead_stimqueue()
{
        stop();
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
}

void
readahead_stimqueue::stop()
{
        // messy?
        pthread_cancel(_thread_id);
}

void
readahead_stimqueue::join()
{
        pthread_join(_thread_id, NULL);
}

void *
readahead_stimqueue::thread(void * arg)
{
	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
        readahead_stimqueue * self = static_cast<readahead_stimqueue *>(arg);
        self->loop();
        return 0;
}

void
readahead_stimqueue::loop()
{
        jill::stimulus_t * ptr;
	pthread_mutex_lock (&_lock);

        while (1) {
                // load data
                if (_head == 0) {
                        if (_it == _last) {
                                if (_loop) _it = _first;
                                else break;
                        }
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                        _head = ptr;
                        if (_log) _log->log() << "next stim: " << ptr->name() << " (" << ptr->duration() << " s)";
                        _it += 1;
                }

                // read ahead
                if (_it != _last) {
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                }

                // wait for iterator to change
                pthread_cond_wait (&_ready, &_lock);
        }
        if (_log) _log->log() << "end of stimulus list";
        pthread_mutex_unlock (&_lock);
}

jill::stimulus_t const *
readahead_stimqueue::head()
{
        if (_head) return _head;
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
        return 0;
}


void
readahead_stimqueue::release()
{
        _head = 0;
        // signal thread that iterator has advanced
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}
