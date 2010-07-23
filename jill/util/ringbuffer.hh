/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _RINGBUFFER_HH
#define _RINGBUFFER_HH

#include <boost/noncopyable.hpp>
#include <jack/ringbuffer.h>


namespace jill { namespace util {


template<typename T>
class Ringbuffer : boost::noncopyable
{
public:
	Ringbuffer(std::size_t size) {
		_rb = jack_ringbuffer_create(size * sizeof(T));
	}

	~Ringbuffer() {
		jack_ringbuffer_free(_rb);
	}

	// should also add calls to get_read_vector and get_write_vector for performance
	inline std::size_t write(const T *buffer, std::size_t nframes) {
		return jack_ringbuffer_write(_rb, reinterpret_cast<char const *>(buffer), 
					     sizeof(T) * nframes);
	}

	inline std::size_t read(T *buffer, std::size_t nframes=0) {
		if (nframes==0) 
			nframes = read_space();
		return jack_ringbuffer_read(_rb, reinterpret_cast<char *>(buffer), 
					    sizeof(T) * nframes);
	}

	inline std::size_t write_space() {
		return jack_ringbuffer_write_space(_rb) / sizeof(T);
	}

	inline std::size_t read_space() {
		return jack_ringbuffer_read_space(_rb) / sizeof(T);
	}

private:
	jack_ringbuffer_t * _rb;
};

}} // namespace jill::util


#endif // _DAS_RINGBUFFER_HH
