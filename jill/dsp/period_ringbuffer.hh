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
 * For multichannel data, the channels all need to be written to the same
 * ringbuffer in order to maintain synchrony with each other. In order for the
 * reader thread to know how much data to pull out of the ringbuffer, the chunk
 * size either needs to be fixed, or there needs to be a header that describes
 * the chunk size.  The latter is more flexible, as it allows chunk size to
 * change, and also the embedding of timestamp information.
 *
 * The interface is slightly different from the standard ringbuffer, in that the
 * writer first reserves space for the period and then fills it channel by
 * channel.
 *
 * Inherits size and space member functions from dsp::ringbuffer, but these do
 * NOT correspond to periods and should not be used except to check if the
 * buffer is empty or full.
 */

class period_ringbuffer : public ringbuffer<char>
{
public:
        typedef ringbuffer<char> super;
        typedef super::data_type data_type;

	typedef boost::function<void (void const * src, period_info_t & info)> visitor_type;

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
         * Look at the next period in the buffer. If a period is available,
         * returns a pointer to the header. You can then copy out just the
         * payload or the whole chunk. Be sure to call release() when done.
         *
         * @return period_info_t* for the next period, or 0 if none is
         * available.
         *
         */
        period_info_t const * peek();

        /**
         * Release the next period in the queue after peeking at it.
         */
        void release();

};

}} // namespace

#endif
