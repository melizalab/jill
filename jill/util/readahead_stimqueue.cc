#include <ostream>
#include <algorithm>

#include "readahead_stimqueue.hh"

using namespace jill::util;

readahead_stimqueue::readahead_stimqueue(nframes_t samplerate,
                                         boost::shared_ptr<jill::event_logger> logger,
                                         bool loop)
        : _log(logger), _samplerate(samplerate), _loop(loop), _it(_stimlist.begin()), _head(0)
{
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
        int ret = pthread_create(&_thread_id, NULL, readahead_stimqueue::thread, this);
        if (ret != 0)
                throw std::runtime_error("Failed to start writer thread");
}

readahead_stimqueue::~readahead_stimqueue()
{
        pthread_cancel(_thread_id);
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
}

void
readahead_stimqueue::add(jill::stimulus_t * stim, size_t nreps)
{
        _stimuli.push_back(stim);

	pthread_mutex_lock (&_lock);
        for (size_t i = 0; i < nreps; ++i) {
                _stimlist.push_back(stim);
        }
        _it = _stimlist.begin(); // because push_back may invalidate
        pthread_cond_signal (&_ready); // signal new data available
        pthread_mutex_unlock (&_lock);
}

void
readahead_stimqueue::shuffle()
{
	pthread_mutex_lock (&_lock);
        std::random_shuffle(_stimlist.begin(), _stimlist.end());
        _it = _stimlist.begin();
        pthread_cond_signal (&_ready); // may not be necessary
        pthread_mutex_unlock (&_lock);
}

void
readahead_stimqueue::stop()
{
        // this may be too messy
        pthread_cancel(_thread_id);
        _head = 0;
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
                if (_head == 0) {
                        if (_it == _stimlist.end() && _stimlist.size() > 0) break;
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                        _log->log() << "next stim: " << ptr->name();
                        _head = ptr;
                        _it += 1;
                }
                if (_loop && _it == _stimlist.end()) {
                        _it = _stimlist.begin();
                }
                if (_it != _stimlist.end() && *_it) {
                        // read ahead
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                }
                // wait for release
                pthread_cond_wait (&_ready, &_lock);
        }
        _log->log() << "end of stimulus list";
        pthread_mutex_unlock (&_lock);
}

void
readahead_stimqueue::release()
{
        _head = 0;
        // signal thread to load next stimulus
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}
