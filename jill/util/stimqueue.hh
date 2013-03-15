/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _STIMQUEUE_HH
#define _STIMQUEUE_HH

#include <pthread.h>
#include <algorithm>
#include "../stimulus.hh"

namespace jill { namespace util {

/**
 * Represents a two-element stimulus queue. One thread is pulling data off the
 * current stimulus, while another is making sure the next stimulus is loaded.
 *
 * Provides some synchronization between threads. The reader thread should check
 * ready(), use the frame buffer (advancing the read pointer as necessary), then
 * call release() when done. The (slow) writer thread should call enqueue()
 * whenever it has a stimulus loaded.  This will block until the reader calls
 * release(), at which point it will swap in the next stimulus.
 */
class stimqueue {

public:
        stimqueue();
        ~stimqueue();

        /** return true if the current stimulus is loaded */
        bool ready() const { return current_stim && current_stim->buffer(); }

        /** true if the queue is filled */
        bool full() const { return current_stim && next_stim; }

        /** true if the stimulus is playing (samples have been read) */
        bool playing() const {
                return current_stim && (buffer_pos > 0);
        }

        /** the current buffer position */
        sample_t const * buffer() const { return current_stim->buffer() + buffer_pos; }

        /** the number of samples left in the stimulus */
        nframes_t nsamples() const { return current_stim->nframes() - buffer_pos; }

        /** the name of the current stimulus */
        char const * name() const { return current_stim->name(); }

        /**
         * Advance the read pointer.
         *
         * @param samples   the numbers of samples to advance. will not advance
         *                  beyond end of buffer
         * @pre ready() is true and the caller is the read thread
         */
        void advance(nframes_t samples) {
                buffer_pos = std::min(nframes_t(buffer_pos + samples),
                                      current_stim->nframes());
        }

        /**
         *  Release the current stimulus. This is a wait-free, thread-safe
         *  function. If the object is locked, it will return immediately, but
         *  will need to be called again to release the current stimulus.
         */
        void release();

        /**
         * Enqueue a new stimulus. This function will block until there is room
         * in the queue for the argument.  If the queue is not full, the new
         * stimulus will be added to the next free position.
         *
         * @param stim   the stimulus to enqueue
         */
        void enqueue(jill::stimulus_t const * stim);

private:
        jill::stimulus_t const * current_stim;     // currently playing stim
        jill::stimulus_t const * next_stim;        // next stim in queue
        size_t buffer_pos;                         // position in current buffer

        pthread_mutex_t disk_thread_lock;
        pthread_cond_t  data_needed;

};

}} // namespace jill::util

#endif
