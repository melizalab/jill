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
#include <boost/shared_ptr.hpp>
#include "../data_thread.hh"

namespace jill {

namespace dsp { class period_ringbuffer; }

namespace file {
/**
 * A trivial implementation of the data thread that just stores data in a
 * ringbuffer and then throws it away. A useful base for more complex tasks.
 */
class multichannel_writer : public data_thread {

public:
        multichannel_writer(nframes_t buffer_size=4096);
        ~multichannel_writer();

        virtual nframes_t push(void const * arg, period_info_t const & info);
        virtual void xrun();
        // virtual int  xruns() const;
        virtual void stop();
        virtual void start();
        virtual void join();

        /**
         * Resize the ringbuffer. Blocks until the write thread has emptied the
         * buffer. If data is being added to the buffer by a realtime thread
         * this may take an extremely long time.
         */
        virtual void resize_buffer(nframes_t buffer_size);

protected:
        /**
         * Entry point for deriving classes to actually so something with the
         * data pulled off the ringbuffer.
         *
         * @param info   the header and data for the period
         */
        virtual void write(period_info_t const * info);

        /**
         * Called when the writer thread is exiting, and after all data have
         * been pulled off the queue.  Allows deriving classes to
         * ensure any remaining data is written to disk.
         */
        virtual void flush();

        static void * thread(void * arg);           // the thread entry point

        pthread_t _thread_id;                      // thread id
        pthread_mutex_t _lock;                     // mutex for disk operations
        pthread_cond_t  _ready;                    // indicates data ready
        int _stop;                                 // stop flag
        int _xruns;                                // xrun counter

        boost::shared_ptr<jill::dsp::period_ringbuffer> _buffer;     // ringbuffer
};

}} // jill::file

#endif
