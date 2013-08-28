/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "../logging.hh"
#include "readahead_stimqueue.hh"

using namespace jill::util;

readahead_stimqueue::readahead_stimqueue(iterator first, iterator last,
                                         nframes_t samplerate,
                                         bool loop)
        :  _first(first), _last(last), _it(first), _head(0),
           _samplerate(samplerate), _loop(loop)
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

/*
 * Threading notes: background thread locks mutex while it's working, and waits
 * on a condition variable when it's not (releasing the mutex).  RT threads call
 * head() and release(), both of which may need to notify the worker to load the
 * next stimulus. To keep these calls waitfree, a trylock call is used to obtain
 * the mutex prior to signaling the condition variable. If the worker is busy,
 * the trylock will fail.
 *
 * I'm not entirely sure if _head is properly protected, because both release()
 * can modify it even when loop() has the lock. It's probably mostly okay,
 * because release() doesn't modify the underlying object, and loop() only
 * performs a simple assignment. The consumer needs to avoid calling release()
 * when head is null to avoid any weird issues like tearing, but it's unlikely
 * to do so because it should check this condition.
 */
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
                        LOG << "next stim: " << ptr->name() << " (" << ptr->duration() << " s)";
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
        LOG << "end of stimulus list";
        pthread_mutex_unlock (&_lock);
}

jill::stimulus_t const *
readahead_stimqueue::head()
{
        if (_head) return _head;
        /*
         * This function has to try to signal the condition variable to avoid a
         * race condition where the worker is busy resampling the next stimulus
         * when release is called. In this case, _head will be set to zero but
         * the worker won't receive the signal to move the next stimulus into _head
         */
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
        return 0;
}


void
readahead_stimqueue::release()
{
        // FIXME? potential race condition with loop? need CAS?
        _head = 0;
        // signal thread that iterator has advanced
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}
