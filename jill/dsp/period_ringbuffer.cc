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

period_ringbuffer::period_ringbuffer(std::size_t size)
        : super(size), _read_header(0), _write_header(0), _chans_to_read(0), _chans_to_write(0)
{}

period_ringbuffer::~period_ringbuffer()
{}

size_t
period_ringbuffer::reserve(nframes_t time, nframes_t nframes, nframes_t nchannels)
{
        if (_chans_to_write || _write_hdr)
                throw std::logic_error("attempted to reserve period before finishing last one");
        size_t chunk_size = sizeof(period_info_t) + nframes * nchannels * sizeof(sample_t);
        if (chunk_size > write_space())
                return 0;

        char * ptr = buffer() + write_offset();
        _write_hdr = reinterpret_cast<period_info_t*>(ptr);
        _write_hdr->time = time;
        _write_hdr->nframes = nframes;
        _write_hdr->nchannels = _chans_to_write = nchannels;
}

void
period_ringbuffer::push(sample_t const * src)
{
        if (!_chans_to_write || !_write_hdr)
                throw std::logic_error("attempted to write period before reserving header");
        void * ptr = buffer() + write_offset() + sizeof(period_info_t) +
                bytes_per_channel() * (_write_hdr->nchannels - _chans_to_write);
        memcpy(ptr, src, bytes_per_channel());
        _chans_to_write -= 1;
        if (_chans_to_write == 0) {
                // advances pointer
                super::push(0, _write_hdr->nchannels * bytes_per_channel() + sizeof(period_info_t));
                _write_hdr = 0;
        }
}

period_info_t const *
period_ringbuffer::request()
{

        if (_chans_to_read || _read_hdr)
                throw std::logic_error("attempted to request period before finishing last one");
        if (!read_space())
                return 0;
        _read_hdr = buffer() + read_offset();
        _chans_to_read = _read_hdr->nchannels;
        return _read_hdr;
}

void
period_ringbuffer::pop(sample_t * dest)
{
        if (!_chans_to_read || !_read_hdr)
                throw std::logic_error("attempted to read period before requesting header (or out of channels)");
        void const * ptr = buffer() + read_offset() + sizeof(period_info_t) +
                bytes_per_channel() * (_read_hdr->nchannels - _chans_to_read);
        memcpy(dest, ptr, bytes_per_channel());
        _chans_to_read -= 1;
        if (_chans_to_write == 0) {
                // advances pointer
                super::pop(0, _read_hdr->nchannels * bytes_per_channel() + sizeof(period_info_t));
                _write_hdr = 0;
        }

}


void
period_ringbuffer::pop(read_visitor_type data_fun)
{}

