/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstring>

#include "block_ringbuffer.hh"
#include "../logging.hh"

using namespace jill::dsp;
using jill::data_block_t;
using std::size_t;

block_ringbuffer::block_ringbuffer(std::size_t size)
        : super(size), _read_ahead_ptr(0)
{}

size_t
block_ringbuffer::push(nframes_t time, dtype_t dtype, char const * id,
                       size_t size, void const * data)
{
        // serialize the data in the buffer such that the header is followed by
        // the two data arrays
        data_block_t header(time, dtype, std::strlen(id), size);
        if (header.size() > write_space()) {
                DBG << "ringbuffer full (req=" << header.size() << "; avail=" << write_space() << ")";
                return 0;
        }
        char * dst = reinterpret_cast<char *>(buffer() + write_offset());
        // store header
        std::memcpy(dst, &header, sizeof(data_block_t));
        dst += sizeof(data_block_t);
        // store id
        std::memcpy(dst, id, header.sz_id);
        dst += header.sz_id;
        // store data
        std::memcpy(dst, data, header.sz_data);
        advance_write_ptr(header.size());
        return header.size();
}

data_block_t const *
block_ringbuffer::peek_ahead()
{
        data_block_t const * ptr = nullptr;
        if (read_space() > _read_ahead_ptr) {
                ptr = reinterpret_cast<data_block_t const *>(buffer() + read_offset() + _read_ahead_ptr);
                _read_ahead_ptr += ptr->size();
        }
        return ptr;
}

data_block_t const *
block_ringbuffer::peek() const
{
        data_block_t const * ptr = nullptr;
        if (read_space())
                ptr = reinterpret_cast<data_block_t const *>(buffer() + read_offset());
        return ptr;
}


void
block_ringbuffer::release()
{
        data_block_t const * ptr = peek();
        if (ptr) {
                if (_read_ahead_ptr > 0)
                        _read_ahead_ptr -= ptr->size();
                super::discard(ptr->size());
        }
}

void
block_ringbuffer::release_all()
{
        super::discard_all();
        _read_ahead_ptr = 0;
}
