/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <pthread.h>
#include <algorithm>
#include "stimset.hh"

using namespace jill::util;

stimset::stimset(nframes_t samplerate)
        : _samplerate(samplerate)
{}

void
stimset::add(jill::stimulus_t * stim, size_t nreps)
{
        _stimuli.push_back(stim);
        // pthread_mutex_lock (&stimlist_lock);
        for (size_t i = 0; i < nreps; ++i) {
                _stimlist.push_back(stim);
        }
        _it = _stimlist.begin();
	// pthread_mutex_unlock (&stimlist_lock);

}

void
stimset::shuffle()
{
        // pthread_mutex_lock (&stimlist_lock);
        std::random_shuffle(_stimlist.begin(), _stimlist.end());
        _it = _stimlist.begin();
	// pthread_mutex_unlock (&stimlist_lock);
}

jill::stimulus_t const *
stimset::next()
{
        jill::stimulus_t *ret;
        // pthread_mutex_lock (&stimlist_lock);
        if (_it == _stimlist.end()) {
                _it = _stimlist.begin();
                ret = 0;
        }
        else {
                ret  = *_it;
                ret->load_samples();
                // if (!ret->buffer())
                //         pthread_cond_wait (&data_ready, &stimlist_lock);
                ++_it;
        }
        // pthread_mutex_unlock (&stimlist_lock);
        return ret;
}

/*
 * Threading notes. A separate thread loads data from disk and resamples as
 * needed. Disk thread needs signal when new stims are added (more work) or the
 * list is reshuffled (invalid iterator). next() may need to wait on the disk
 * thread if the data for a requested stim is not loaded.
 */

/* protects stimlist iterator */
// static pthread_mutex_t stimlist_lock = PTHREAD_MUTEX_INITIALIZER;
// static pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;
// static pthread_cond_t  iter_ready = PTHREAD_COND_INITIALIZER;


// void *
// stimset::load_samples(void * arg)
// {
//         jill::stimulus_t *current, *next;

//         // allow cancellation at any point
// 	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

//         while (1) {
//                 // operating on current stimulus
//                 pthread_mutex_lock (&stimlist_lock);
//                 current = *_it;
//                 next = *(_it + 1)

//                 stim->load_samples(self->_samplerate);
//                 pthread_cond_signal(&data_ready);

//                 pthread_mutex_unlock (&stimlist_lock);
//         }

//         iterator it = _it;
//         it->load_samples(self->_samplerate);
//         return 0;
// }
