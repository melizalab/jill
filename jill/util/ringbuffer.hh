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

	/** 
	 * Write data to the ringbuffer. 
	 * 
	 * @param src Pointer to source buffer
	 * @param nframes The number of frames in the source buffer
	 * 
	 * @return The number of frames actually written
	 */
	inline std::size_t write(const T *src, std::size_t nframes) {
		return jack_ringbuffer_write(_rb, reinterpret_cast<char const *>(src), 
					     sizeof(T) * nframes);
	}

	/** 
	 * Read data from the ringbuffer. This version of the function
	 * copies data to a destination buffer.
	 * 
	 * @param dest The destination buffer. Needs to be pre-allocated
	 * @param nframes The number of frames to read
	 * 
	 * @return The number of frames actually read
	 */
	inline std::size_t read(T *dest, std::size_t nframes=0) {
		if (nframes==0) 
			nframes = read_space();
		return jack_ringbuffer_read(_rb, reinterpret_cast<char *>(dest), 
					    sizeof(T) * nframes);
	}

	/** 
	 * Read data from the ringbuffer. This version sets in the
	 * input argument to the address of an array with the next
	 * block of data.  To free space after using the data, call advance()
	 * 
	 * @param buf Output, will point to data in ringbuffer after read
	 * 
	 * @return The number of available samples
	 */
	inline std::size_t peek(T **buf) {
		jack_ringbuffer_data_t vec[2];
		jack_ringbuffer_get_read_vector(_rb, vec);
		for (int i = 0; i < 2; ++i) {
			if (vec[i].len > 0) {
				*buf = reinterpret_cast<T *>(vec[i].buf);
				return vec[i].len;
			}
		}
		return 0;
	}

	/// Advance the read pointer by nframes
	inline void advance(std::size_t nframes) {
		jack_ringbuffer_read_advance(_rb, nframes);
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
