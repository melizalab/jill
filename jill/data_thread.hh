/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
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
 * The interface for a threaded data handler. The handler accepts data through the push()
 * member function and then processes it in a separate thread in an
 * implementation-specific manner.
 *
 * The handler may be in one of three states:
 *
 * Stopped: [initial state] push() accepts samples, but data is not being processed
 * Running: [Stopped=>start()] push() accepts samples, data are processed
 * Stopping: [Running=>stop()] push() drops samples while remaining samples in the
 *           buffer are flushed. Once all samples are flushed, returns to Stopped.
 *
 * data_thread objects are safe to use in realtime applications as long as the
 * methods marked as wait-free are implemented using wait-free algorithms.
 */
class data_thread : boost::noncopyable {

public:
        virtual ~data_thread() {}

        /**
         * Process incoming data according to current state. In Stopped and
         * Running, data are stored for further processing. In Stopping, data
         * are silently discarded. Must always be wait-free. The caller must
         * call data_ready() after push() to notify the handler that there is
         * data to process.
         *
         * @param time  the time of the block
         * @param dtype the type of data in the block
         * @param id    a string giving the id (channel) of the block
         * @param size  the number of bytes in the data array
         * @param data  an array of data to write
         */
        virtual void push(nframes_t time, dtype_t dtype, char const * id,
                          std::size_t size, void const * data) = 0;

        /** Signal the handler that data is ready. Must be wait-free. */
        virtual void data_ready() = 0;

        /** Signal an overrun/underrun. Must be wait-free. */
        virtual void xrun() {}

        /** Reset processing of data stream at the end of the next full period. Wait-free. */
        virtual void reset() {}

        /** Tell the thread to finish writing and exit. Must be wait-free. */
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
         * thread until it stops. Users may also call it to check/wait for
         * thread completion.
         */
        virtual void join() {}

        /**
         * Hint how much buffering is needed. May block if memory needs to be
         * acquired.
         *
         * Most implementations will use buffers to provide wait-free push()
         * behavior and thus need to know how much storage is needed between
         * calls to data_ready. The actual amount needed will also depend on how
         * the data are consumed.

         * @param bytes   the requested capacity of the buffer
         * @return the new capacity of the buffer
         */
        virtual std::size_t request_buffer_size(std::size_t bytes) { return bytes; }

protected:
        enum state_t {
                Stopped,
                Running,
                Stopping,
        };

};

}

#endif
