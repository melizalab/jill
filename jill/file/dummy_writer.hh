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
#ifndef _MULTICHANNEL_WRITER_HH
#define _MULTICHANNEL_WRITER_HH

#include <pthread.h>
#include "../data_thread.hh"

/**
 * A trivial implementation of the data thread that just stores data in a
 * ringbuffer and then throws it away. A useful base for more complex tasks.
 */
class multichannel_writer : public data_thread {

public:
        multichannel_writer();
        ~multichannel_writer();

        /**
         * Store data to be processed. Thread-safe, wait-free.
         *
         * @param data     the buffer from a single channel
         * @param nframes  the number of sample frames in the data
         * @param time     the sample offset of the data
         */
        virtual void store(void * arg, nframes_t nframes, nframes_t time);

        /** Increment the overrun/underrun counter. Thread-safe, wait-free. */
        virtual void xrun();

        /** Tell the thread to stop writing and exit. Thread-safe, wait-free */
        virtual void stop();

        /** Start the thread writing samples */
        virtual void start();

        /** Wait for the thread to finish */
        virtual void join();

protected:

        pthread_t _thread_id;                      // thread id
        pthread_mutex_t _lock;                     // mutex for disk operations
        pthread_cond_t  _ready;                    // indicates data ready
        int _stop;                                 // stop flag
        long _xruns;                               // xrun counter



};

#endif
