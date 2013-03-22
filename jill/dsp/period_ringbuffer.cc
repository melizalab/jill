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

#include "period_ringbuffer.hh"

using jill::period_info_t;
using namespace jill::dsp;
using std::size_t;

period_ringbuffer::period_ringbuffer(std::size_t nsamples)
        : super(nsamples * sizeof(sample_t))
{}

period_ringbuffer::~period_ringbuffer()
{}

jill::nframes_t
period_ringbuffer::push(void const * src, period_info_t const & info)
{
        std::size_t size = sizeof(period_info_t) + info.bytes();
        if (size > super::write_space()) return 0;
        char * dst = buffer() + write_offset();
        // store header
        memcpy(dst, &info, sizeof(period_info_t));
        // store data
        memcpy(dst + sizeof(period_info_t), src, info.bytes());
        // advance write pointer
        super::push(0, size);
        return info.nframes;
}

period_info_t const *
period_ringbuffer::peek()
{
        if (!read_space())
                return 0;
        return reinterpret_cast<period_info_t*>(buffer() + read_offset());
}


void
period_ringbuffer::release(size_t nperiods)
{
        if (nperiods==0) {
                super::pop(0,0);
        }
        else {
                period_info_t const * ptr = peek();
                while (ptr && nperiods) {
                        super::pop(0, sizeof(period_info_t) + ptr->bytes());
                        ptr = peek();
                        nperiods--;
                }
        }
}
