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
 * Represents a stimulus queue that provides wait-free access to a series of
 * stimuli in order. The queue may loop endlessly, and a background thread may
 * be responsible for preparing the data.  The method(s) for adding stimuli to the
 * queue are implementation-specific.
 */
class stimqueue : boost::noncopyable {

public:
        virtual ~stimqueue() = default;

        /**
         * Get the stimulus currently at the head of the queue.
         *
         * @note must be wait-free
         *
         * @return pointer to current stimulus, or nullptr if no stimulus is
         * available
         */
        virtual stimulus_t const * head() = 0;

        /**
         * Release the current stimulus and notify the queue to load a new one.
         *
         * @pre head() != 0
         * @note must be wait-free
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
