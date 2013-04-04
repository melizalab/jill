/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _DATA_THREAD_HH
#define _DATA_THREAD_HH

#include <boost/noncopyable.hpp>
#include "types.hh"

namespace jill {

/**
 * The interface for a data handler. A fast thread sends it samples through
 * push(), and the state of the handler can be manipulated through a set of
 * member functions.  The handler can be in several states:
 *
 * Stopped: [initial state] data is not being written to disk. any background
 *          threads are inactive
 * Running: [Stopped=>start()] push() accepts samples, samples are written
 * Xrun:    [Running=>xrun()] initiate when an overrun occurs. returns to Running
 *          after remaining samples in the buffer are flushed
 * Stopping: [Running=>stop()] remaining samples in the buffer are flushed.
 *          returns to Stopped.
 */
class data_thread : boost::noncopyable {

public:
        virtual ~data_thread() {}

        /**
         * Process incoming data according to current state. In Stopped and
         * Running, data are stored for further processing. In Xrun and
         * Stopping, data are silently discarded. Always thread-safe
         * and lock-free.
         *
         * @param data     the buffer from a single channel. must have at least
         *                 as many elements as info.nframes
         * @param info     a structure with information about the period
         *
         * @return in Stopped and Running, the number of frames stored. This may
         * be less than the number of frames requested if any underlying buffers
         * are full. In Stopping and Xrun, always the number of frames
         * requested.
         */
        virtual nframes_t push(void const * arg, period_info_t const & info) = 0;

        /** Signal an overrun/underrun. Thread-safe, lock-free. */
        virtual void xrun() {}

        /**
         * Tell the thread to finish writing and exit. Thread-safe, lock-free.
         * Also informs push() not to store any additional samples.
         */
        virtual void stop() {}

        /**
         * Start the thread writing samples.
         *
         * @pre the thread is not already running
         */
        virtual void start() {}

        /**
         * Wait for the thread to finish. Deriving classes should call this in
         * the destructor to ensure their resources are available to the disk
         * thread until it stops.  Users must call this before the object is
         * destroyed to avoid losing data.
         */
        virtual void join() {}

};

}

#endif
