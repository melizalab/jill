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

#include <iostream>
#include "stimqueue.hh"

using namespace jill::util;

stimqueue::stimqueue()
        : current_stim(0), next_stim(0), buffer_pos(0)
{
        pthread_mutex_init(&disk_thread_lock, 0);
        pthread_cond_init(&data_needed, 0);
}

stimqueue::~stimqueue()
{
        pthread_mutex_destroy(&disk_thread_lock);
        pthread_cond_destroy(&data_needed);
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

namespace jill {

std::ostream & operator<< (std::ostream & os, jill::stimulus_t const & stim)
{
        os << "stim: " << stim.name();
        return os;
}

}
