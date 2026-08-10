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
#ifndef _STIMQUEUE_HH
#define _STIMQUEUE_HH

#include <boost/noncopyable.hpp>
#include "../stimulus.hh"

namespace jill { namespace util {

/**
 * One entry in a playlist: a stimulus, plus whether this presentation of it
 * carries the manipulation.
 *
 * The flag lives here and not on stimulus_t because a playlist holds one
 * pointer per repetition, all aliasing the same object. A field on the
 * stimulus would be shared by every repetition -- all of them marked or none
 * -- which is the opposite of what balancing a condition across trials needs.
 */
struct trial {
        /* Not const: the queue's worker calls load_samples() through this to
         * read the file and resample it. head() hands out a `trial const *`,
         * which protects the entry itself -- the consumer cannot retarget it
         * or change its condition -- but not the stimulus behind it. That is
         * the same protection the queue offered before trials existed. */
        stimulus_t * stim;
        /** true if this presentation is in the manipulated condition */
        bool condition;
};

/**
 * Represents a stimulus queue that provides wait-free access to a series of
 * stimuli in order. The queue may loop endlessly, and a background thread may
 * be responsible for preparing the data.  The method(s) for adding stimuli to the
 * queue are implementation-specific.
 */
class stimqueue : boost::noncopyable {

public:
        virtual ~stimqueue() = default;

        /**
         * Get the trial currently at the head of the queue.
         *
         * @note must be wait-free, and is called from the realtime thread
         *
         * @return pointer to the current trial, or nullptr if none is ready
         */
        virtual trial const * head() = 0;

        /**
         * Release the current trial so the next one can be played.
         *
         * @pre head() != nullptr
         * @note must be wait-free, and is called from the realtime thread
         */
        virtual void release() = 0;

        /**
         * Terminate the queue. It may be necessary to call this function if
         * there is a background thread or the queue is set to loop endlessly.
         */
        virtual void stop() = 0;

        /**
         * Blocks until the queue has been exhausted (the last stimulus in the
         * list has been released, or stop() has been called.
         */
        virtual void join() = 0;

};

}} // namespace jill::util

#endif
