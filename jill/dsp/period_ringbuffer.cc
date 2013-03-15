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
        if (info.size() > write_space()) return 0;
        char * dst = buffer() + write_offset();
        // store header
        memcpy(dst, &info, sizeof(period_info_t));
        // store data
        memcpy(dst + sizeof(period_info_t), src, info.bytes());
        // advance write pointer
        super::push(0, info.size());
        return info.nframes;
}

period_ringbuffer::period_info_t const *
period_ringbuffer::peek()
{
        if (!read_space())
                return 0;
        return reinterpret_cast<period_info_t*>(buffer() + read_offset());
}


void
period_ringbuffer::release()
{
        period_info_t const * ptr = peek();
        if (ptr)
                super::pop(0, ptr->size());
}
