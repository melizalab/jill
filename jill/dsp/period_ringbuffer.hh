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
	typedef boost::function<void (void const * src,
                                      nframes_t nframes,
                                      nframes_t channel_idx)> read_visitor_type;

        /**
         * Define the header for the period. Contains information about
         * timestamp, frame count, and channel count. TODO: do we need an offset
         * field that ensures audio data are aligned properly for optimized
         * copies?
         */
        struct period_info_t {
                nframes_t time;
                nframes_t nbytes; // NB: converted from frame count
                nframes_t nchannels;
                nframes_t offset(nframes_t chan=0) const { return sizeof(period_info_t) + nbytes * chan;}
                nframes_t size() const { return nbytes * nchannels + sizeof(period_info_t);}
        };

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
         * Reserve space for a period
         *
         * Checks to see if there is enough room for the period, and if so,
         * writes the header. Subsequent calls to push are used to write a
         * single channel to the period.  Only once all the period is fully
         * written is the write pointer advanced.
         *
         * @param time      the timestamp for the period
         * @param size      the number of frames per channel in the period
         * @param nchannels the number of channels in the period
         *
         * @return the number of periods that could fit in the buffer. If the
         *         a complete period cannot fit, the header is NOT written and
         *         the return value is zero.
         *
         * @throws std::logic_error if the previous chunk is not completely written
         */
        std::size_t reserve(nframes_t time, nframes_t nframes, nframes_t nchannels);

        /**
         * Return the number of channels in the current write chunk, or zero if a
         * chunk has not been requested.
         */
        nframes_t chans_to_write() const;

        /**
         * Write the data for one channel in a period
         *
         * Writing per-channel is largely a matter of convenience because that's
         * how the data is accessed in JACK. The object keeps track of how many
         * channels have been written.  Attempting to write a channel before the
         * header is written for a new chunk raises an error.
         *
         * @param data the block of data to be written. At least
         *             nframes*sizeof(sample_t) bytes
         *
         * @ throws std::logic_error if the number of calls exceeds the number of channels
         *          or the header has not been written
         */
	void push(void const * src);

        /**
         * Request a period from the buffer
         *
         * If a period is available, returns a pointer to the header. After
         * acquiring the header, the client should read each channel in the
         * period using pop(). After all the channels have been read the read
         * pointer will be advanced.
         *
         * @return period_info_t* for the next period, or 0 if none is
         * available.
         *
         * @throws std::logic_error if the method is called before all channels
         * have been read
         */
        period_info_t const * request();

        /**
         * Return the number of remaining channels in the current read chunk, or
         * zero if a chunk has not been requested.
         */
        nframes_t chans_to_read() const;

        /**
         * Copy data for a channel in the current period.
         *
         * @param dest the destination buffer. Must have as many bytes as there
         * are per channel (period_info_t.nframes * sizeof(sample_t))
         *
         * @throws std::logic_error if called before request() or after all
         * channels have been read
         */
        void pop(void * dest);

        /**
         * Access channel data for current period
         *
         * @param data_fun   see @read_visitor_type
         *
         * @throws std::logic_error if called before request() or after all
         * channels have been read
         */
        void pop(read_visitor_type data_fun);

        /**
         * @brief Copy all data from a period
         *
         * Copies the entire period, including the header, into a block of
         * memory.  Call request() first and allocate sizeof(period_info_t) +
         * sizeof(sample_t) * nframes * nchannels
         */
        void pop_all(void * dest);

private:
        period_info_t *_read_hdr;
        period_info_t *_write_hdr;
        nframes_t _chans_to_read;
        nframes_t _chans_to_write;

};

}} // namespace

#endif
