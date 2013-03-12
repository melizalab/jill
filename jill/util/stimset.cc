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
        for (size_t i = 0; i < nreps; ++i) {
                _stimlist.push_back(stim);
        }
        _it = _stimlist.begin();
}

void
stimset::shuffle()
{
        std::random_shuffle(_stimlist.begin(), _stimlist.end());
        _it = _stimlist.begin();
}

jill::stimulus_t const *
stimset::next()
{
        jill::stimulus_t *ret;
        if (_it == _stimlist.end()) {
                _it = _stimlist.begin();
                ret = 0;
        }
        else {
                ret  = *_it;
                ret->load_samples();
                ++_it;
        }
        return ret;
}

stimqueue::stimqueue()
        : current_stim(0), next_stim(0), buffer_pos(0)
{
        pthread_mutex_init(&disk_thread_lock, 0);
        pthread_cond_init(&data_needed, 0);
}

void
stimqueue::release()
{
        if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
                current_stim = 0;
                pthread_cond_signal (&data_needed);
                pthread_mutex_unlock (&disk_thread_lock);
        }
}

void
stimqueue::enqueue(jill::stimulus_t const * stim)
 {
         if (next_stim) {

                 pthread_mutex_lock (&disk_thread_lock);
                 if (current_stim)
                         pthread_cond_wait (&data_needed, &disk_thread_lock);
                 current_stim = next_stim;
                 buffer_pos = 0;
                 pthread_mutex_unlock (&disk_thread_lock);

                 next_stim = stim;
         }
         else if (current_stim) {
                 next_stim = stim;
         }
         else {
                 pthread_mutex_lock (&disk_thread_lock);
                 current_stim = stim;
                 buffer_pos = 0;
                 pthread_mutex_unlock (&disk_thread_lock);
         }
 }
