/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _BLOCK_RINGBUFFER_HH
#define _BLOCK_RINGBUFFER_HH

#include "../types.hh"
#include "ringbuffer.hh"

namespace jill { namespace dsp {


/**
 * @ingroup buffergroup
 * @brief a chunking, lockfree ringbuffer
 *
 * This ringbuffer class operates on data in blocks. Each block comprises a
 * header followed by two arrays of data. The header describes the contents of the
 * data, including its length. The first array gives the id (channel) name of
 * the block, and the second array contains the data. Currently sampled or event
 * data are specified.
 *
 * An additional feature of this interface allows it to be efficiently used as a
 * prebuffer. The peek_ahead() function provides read-ahead access, which can
 * used to detect when a trigger event has occurred, while the peek() and
 * release() functions operate on data at the tail of the queue.
 */
class block_ringbuffer : public ringbuffer<char>
{
public:
        typedef ringbuffer<char> super;
        typedef super::data_type data_type;

        /**
         * Initialize ringbuffer.
         *
         *  @param size  the size of the buffer, in bytes
         *
         *  There's no fixed relationship between buffer size and block size,
         *  because block size can be changed without necessiarly needing to
         *  resize the buffer. Also, event data may take up much less room than
         *  sampled data. A good minimum is nframes*nchannels*12
         */
        explicit block_ringbuffer(std::size_t size);
        ~block_ringbuffer() = default;

        /// @return the number of samples ahead of the read pointer the read-ahead pointer is
        std::size_t read_ahead_space() const {
                return _read_ahead_ptr;
        }

        /// @return true if the buffer contains no data
        bool empty() const {
                return read_space() == 0;
        }

        /// @return true if the peek_ahead pointer is at the end of the read buffer
        bool empty_ahead() const {
                return read_space() == _read_ahead_ptr;
        }

        /**
         * Store a block of data
         *
         * @param time  the time of the block
         * @param dtype the type of data in the block
         * @param id    a string giving the id (channel) of the block
         * @param size  the number of bytes in the data array
         * @param data  an array of data to write
         *
         * @returns the number of bytes written, or 0 if there wasn't enough
         *          room for all of them. Will not write partial blocks.
         */
        std::size_t push(nframes_t time, dtype_t dtype, char const * id,
                         std::size_t size, void const * data);

        /**
         * Read-ahead access to the buffer. If a block is available, returns a
         * pointer to the header. Successive calls will access successive
         * blocks.
         *
         * @return data_block_t* for the next block, or 0 if none is
         * available.
         *
         */
        data_block_t const * peek_ahead();

        /**
         * Read access to the buffer. Returns a pointer to the oldest block in
         * the read queue, or NULL if the read queue is empty.  Successive calls
         * will access the oldest block until it is released.
         *
         * @return data_block_t* for the oldest block, or 0 if none is
         * available.
         */
        data_block_t const * peek() const;

        /**
         * Release the oldest block in the read queue, making the memory
         * available to the write thread and advancing the read pointer
         */
        void release();

        /** Release all data in the read queue */
        void release_all();

private:
        std::size_t _read_ahead_ptr; // the number of bytes ahead of the _read_ptr

};

}} // namespace

#endif
