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
#ifndef _BUFFERED_DATA_WRITER_HH
#define _BUFFERED_DATA_WRITER_HH

#include <iosfwd>
#include <pthread.h>
#include <boost/shared_ptr.hpp>
#include "../data_thread.hh"
#include "../data_writer.hh"

namespace jill {

namespace dsp {

class block_ringbuffer;

/**
 * An implementation of the data thread that uses a ringbuffer to move data
 * between the push() function and a writer thread.  The logic for actually
 * storing the data (and log messages) is provided through an owned data_writer.
 * This implementation records continuously, though other threads may call
 * reset() to split data into separate entries.
 */
class buffered_data_writer : public data_thread {

public:
        /**
         * Initialize buffered writer
         *
         * @param writer       the sink for the data
         * @param buffer_size  the initial size of the ringbuffer (in bytes)
         */
        buffered_data_writer(boost::shared_ptr<data_writer> writer, std::size_t buffer_size=4096);
        virtual ~buffered_data_writer();

        /* implementations of data_thread methods */

        void push(nframes_t time, dtype_t dtype, char const * id,
                  std::size_t size, void const * data);
        void data_ready();
        void xrun();
        void reset();
        void stop();
        void start();
        void join();

        /**
         * Resize the ringbuffer. Only takes effect if the new size is larger
         * than the current size. The actual size may be larger due to
         * constraints on the underlying storage mechanism.
         *
         * Blocks until the write thread has emptied the buffer, so this can
         * only be called when data are no longer being added to the buffer
         * (i.e. in resize_buffer callback)
         */
        virtual std::size_t request_buffer_size(std::size_t bytes);

        /**
         * Bind the logger to a zeromq socket. Messages may be sent to this
         * socket by other programs.
         *
         * @param server_name  the name of the jack server. All the clients of
         *                     the server must log to the same socket.
         *
         * @note once binding is successful, subsequent calls to this function
         *       do nothing.
         */
        void bind_logger(std::string const & server_name);

protected:
        /**
         * Entry point for deriving classes to handle data pulled off the
         * ringbuffer. Deriving classes *must* release data using
         * _buffer->release() when they are done with the data or the buffer
         * will overrun.
         *
         * @param data   the header and data for the period. may be null if
         *               there's no data
         */
        virtual void write(data_block_t const * data);

        /**
         * Collect log messages from the zmq socket and write them. Call this
         * when load is low.
         */
        void write_messages();

        state_t _state;                            // thread state
        bool _reset;                               // flag to reset stream

        boost::shared_ptr<data_writer> _writer;            // output
        boost::shared_ptr<block_ringbuffer> _buffer;      // ringbuffer

private:
        pthread_mutex_t _lock;                     // mutex for condition variable
        pthread_cond_t  _ready;                    // indicates data ready
        static void * thread(void * arg);           // the thread entry point
        pthread_t _thread_id;                      // thread id
        bool _xrun;                                // flag to indicate xrun
        // variables for receiving incoming messages
        void * _context;
        void * _socket;
        bool _logger_bound;

};

}} // jill::file

#endif
