/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _PERIOD_RINGBUFFER_HH
#define _PERIOD_RINGBUFFER_HH

#include "../types.hh"
#include "ringbuffer.hh"

namespace jill { namespace dsp {

/**
 * @ingroup buffergroup
 * @brief a chunking, lockfree ringbuffer
 *
 * This ringbuffer class operates on data in chunks corresponding to a period of
 * data from JACK. Each chunk comprises a header followed by an array of data.
 * The header (@see jill::period_info_t) describes the contents of the data,
 * including its length. Data are added and removed from the queue as chunks.
 *
 * An additional feature of this interface allows it to be efficiently used as a
 * prebuffer. The peek_ahead() function provides read-ahead access, which can
 * used to detect when a trigger event has occurred, while the peek() and
 * release() functions operate on data at the tail of the queue.
 */

class period_ringbuffer : protected ringbuffer<char>
{
public:
        typedef ringbuffer<char> super;
        typedef super::data_type data_type;

        /**
         * Initialize ringbuffer.
         *
         *  @param size  the size of the buffer, in samples
         *
         *  There's no fixed relationship between buffer size and period size,
         *  because period size can be changed without necessiarly needing to
         *  resize the buffer. A good minimum is nframes*nchannels*3
         */
        explicit period_ringbuffer(std::size_t nsamples);
        ~period_ringbuffer();

        /// @see ringbuffer::resize(). units are in frames
        void resize(std::size_t nframes) {
                super::resize(nframes * sizeof(sample_t));
        }

        /// @return the size of the buffer (in frames)
        std::size_t size() const {
                return super::size() / sizeof(sample_t);
        }

	/// @return the number of complete periods that can be written to the ringbuffer
	std::size_t write_space(nframes_t nframes) const {
                return super::write_space() / (nframes * sizeof(sample_t) + sizeof(period_info_t));
        }

        /// @return the number of samples ahead of the read pointer the read-ahead pointer is
        std::size_t read_ahead_space() const {
                return _read_ahead_ptr;
        }

        /**
         * Store data for one channel in a period
         *
         * @param data  the block of data to store
         * @param info  the header for the period
         *
         * @returns the number of samples written, or 0 if there wasn't enough
         *          room for all of them. Will not write partial chunks.
         */
	nframes_t push(void const * src, period_info_t const & info);

        /**
         * Read-ahead access to the buffer. If a period is available, returns a
         * pointer to the header. Successive calls will access successive
         * periods.
         *
         * @return period_info_t* for the next period, or 0 if none is
         * available.
         *
         */
        period_info_t const * peek_ahead();

        /**
         * Read access to the buffer. Returns a pointer to the oldest period in
         * the read queue, or NULL if the read queue is empty.  Successive calls
         * will access the oldest period until it is released.
         *
         * @return period_info_t* for the oldest period, or 0 if none is
         * available.
         */
        period_info_t const * peek() const;

        /**
         * Release the oldest period in the read queue, making it available to
         * the write thread and advancing the read pointer
         */
        void release();

        /** Release all data in the read queue */
        void release_all();

private:
        std::size_t _read_ahead_ptr; // the number of bytes ahead of the _read_ptr

};

}} // namespace

#endif
