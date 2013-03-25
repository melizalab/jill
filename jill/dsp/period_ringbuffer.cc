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
        : super(nsamples * sizeof(sample_t)), _read_ahead_ptr(0)
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
period_ringbuffer::peek_ahead()
{
        period_info_t const * ptr = 0;
        if (read_space() > _read_ahead_ptr) {
                ptr = reinterpret_cast<period_info_t const *>(buffer() + read_offset() + _read_ahead_ptr);
                _read_ahead_ptr += sizeof(period_info_t) + ptr->bytes();
        }
        return ptr;
}

period_info_t const *
period_ringbuffer::peek() const
{
        period_info_t const * ptr = 0;
        if (read_space())
                ptr = reinterpret_cast<period_info_t const *>(buffer() + read_offset());
        return ptr;
}


void
period_ringbuffer::release()
{
        period_info_t const * ptr = peek();
        if (ptr) {
                if (_read_ahead_ptr > 0)
                        _read_ahead_ptr -= sizeof(period_info_t) + ptr->bytes();
                super::pop(0, sizeof(period_info_t) + ptr->bytes());
        }
}

void
period_ringbuffer::release_all()
{
        super::pop(0,0);
        _read_ahead_ptr = 0;
}
