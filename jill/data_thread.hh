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
         * Store data to be processed. Thread-safe, wait-free.
         *
         * @param data     the buffer from a single channel
         * @param nframes  the number of sample frames in the data
         * @param time     the sample offset of the data
         */
        virtual void store(void * arg, nframes_t nframes, nframes_t time) = 0;

        /** Increment the overrun/underrun counter. Thread-safe, wait-free. */
        virtual void xrun() {}

        /** Tell the thread to stop writing and exit. Thread-safe, wait-free */
        virtual void stop() {}

        /**
         *  Write a message to the file. Thread-safe, may block.
         *
         * @param msg       the message to write
         */
        virtual void log(std::string const & msg) {}

        /** Start the thread writing samples */
        virtual void start() {}

        /** Wait for the thread to finish */
        virtual void join() {}

};

}

#endif
