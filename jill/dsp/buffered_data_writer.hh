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
#ifndef _BUFFERED_DATA_WRITER_HH
#define _BUFFERED_DATA_WRITER_HH

#include <iosfwd>
#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include <boost/iostreams/stream.hpp>
#include "../data_thread.hh"
#include "../data_writer.hh"

namespace jill {

namespace dsp {

class period_ringbuffer;

/**
 * An implementation of the data thread that uses a ringbuffer to move data
 * between the push() function and a writer thread.  The logic for actually
 * storing the data (and log messages) is provided through an owned data_writer.
 * This implementation records continuously, starting new entries only when the
 * frame counter overflows or an xrun occurs.
 */
class buffered_data_writer : public data_thread, public event_logger {

public:
        /**
         * Construct new buffered data writer.
         *
         * @param writer       a pointer to a heap-allocated data_writer
         * @param buffer_size  the initial size of the ringbuffer
         *
         * @note best practice is to only access @a writer through this object
         * after initialization, e.g:
         * buffered_data_writer(new concrete_data_writer(...));
         */
        buffered_data_writer(boost::shared_ptr<data_writer> writer, nframes_t buffer_size=4096);
        virtual ~buffered_data_writer();

        nframes_t push(void const * arg, period_info_t const & info);
        void xrun();
        void stop();
        void start();
        void join();

        /* implementations for event_logger */
        std::ostream & log();

	/// @return the number of complete periods that can be stored. wait-free
        nframes_t write_space(nframes_t nframes) const;

        /**
         * Resize the ringbuffer. Only takes effect if the new size is larger
         * than the current size. The actual size may be larger due to
         * constraints on the underlying storage mechanism.
         *
         * A 2 second buffer is good enough for most purposes. Deriving classes
         * may override this method to provide different estimates of the
         * appropriate buffer size.
         *
         * Blocks until the write thread has emptied the buffer. If data is
         * being added to the buffer by a realtime thread this may take an
         * extremely long time.
         *
         * @param nsamples   the requested capacity of the buffer (in frames)
         * @param nchannels  the number of channels in the datastream
         * @return the new capacity of the buffer
         */
        virtual nframes_t resize_buffer(nframes_t nframes, nframes_t nchannels);

protected:
        /**
         * Entry point for deriving classes to handle data pulled off the
         * ringbuffer. Deriving classes *must* release data using
         * _buffer->release() when they are done with the data or the buffer
         * will overrun.  The default implementation simply passes the data to
         * the data_writer::write() function.
         *
         * @param info   the header and data for the period
         */
        virtual void write(period_info_t const * info);

        static void * thread(void * arg);           // the thread entry point

        /* implementation for event_logger */
        std::streamsize log(char const * msg, std::streamsize n);

        pthread_t _thread_id;                      // thread id
        pthread_mutex_t _lock;                     // mutex for disk operations
        pthread_cond_t  _ready;                    // indicates data ready
        int _stop;                                 // stop flag
        int _xruns;                                // xrun counter

        boost::shared_ptr<data_writer> _writer;            // output
        boost::shared_ptr<period_ringbuffer> _buffer;      // ringbuffer
};

}} // jill::file

#endif
