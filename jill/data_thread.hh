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

#include "types.hh"

namespace jill {

/**
 * The interface for a slow data handler. A fast thread sends it samples that
 * this class handles at whatever pace it can. Designed for multiple channels.
 * Deriving classes will have to specify how the data are stored, and how
 * parameters like the number and names of channels, and the buffersize should
 * be configured.
 */
class data_thread : boost::noncopyable {

public:
        virtual ~data_thread() {}

        /**
         * Store data to be processed. Thread-safe, lock-free.
         *
         * @param data     the buffer from a single channel. must have at least
         *                 as many elements as info.nframes
         * @param info     a structure with information about the period
         * @return the number of frames actually written
         */
        virtual nframes_t push(void const * arg, period_info_t const & info) = 0;

        /** Increment the overrun/underrun counter. Thread-safe, lock-free. */
        virtual void xrun() {}
        /** Return the current state of the xrun counter */
        // virtual int  xruns() const;

        /**
         * Tell the thread to finish writing and exit. Thread-safe, lock-free.
         * The thread may not check this condition until it has finished writing
         * all data in the buffer, so calling this while another thread is still
         * calling store() may lead to undefined behavior.
         */
        virtual void stop() {}

        /**
         *  Write a message to the file. Thread-safe, may block.
         *
         * @param msg       the message to write
         */
        virtual void log(std::string const & msg) {}

        /** Start the thread writing samples */
        virtual void start() {}

        /** Wait for the thread to finish. Blocks, potentially for a long time. */
        virtual void join() {}

};

}

#endif
