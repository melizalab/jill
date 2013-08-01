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

#include "block_ringbuffer.hh"

using namespace jill::dsp;
using std::size_t;

block_ringbuffer::block_ringbuffer(std::size_t size)
        : super(size), _read_ahead_ptr(0)
{}

block_ringbuffer::~block_ringbuffer()
{}

size_t
block_ringbuffer::push(nframes_t time, dtype_t dtype, char const * id,
                       size_t size, void const * data)
{
        header_t header = { time, dtype, strlen(id), size};
        if (header.size() > write_space()) return 0;
        char * dst = buffer() + write_offset();
        // store header
        memcpy(dst, &header, sizeof(header_t));
        dst += sizeof(header_t);
        // store id
        memcpy(dst, id, header.sz_id);
        dst += header.sz_id;
        // store data
        memcpy(dst, data, header.sz_data);
        // advance write pointer
        return super::push(0, header.size());
}

header_t const *
block_ringbuffer::peek_ahead()
{
        header_t const * ptr = 0;
        if (read_space() > _read_ahead_ptr) {
                ptr = reinterpret_cast<header_t const *>(buffer() + read_offset() + _read_ahead_ptr);
                _read_ahead_ptr += ptr->size();
        }
        return ptr;
}

header_t const *
block_ringbuffer::peek() const
{
        header_t const * ptr = 0;
        if (read_space())
                ptr = reinterpret_cast<header_t const *>(buffer() + read_offset());
        return ptr;
}


void
block_ringbuffer::release()
{
        header_t const * ptr = peek();
        if (ptr) {
                if (_read_ahead_ptr > 0)
                        _read_ahead_ptr -= ptr->size();
                super::pop(0, ptr->size());
        }
}

void
block_ringbuffer::release_all()
{
        super::pop(0,0);
        _read_ahead_ptr = 0;
}
