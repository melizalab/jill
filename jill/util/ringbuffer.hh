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
#include <boost/type_traits.hpp>
#include <boost/static_assert.hpp>
#include <jack/ringbuffer.h>


namespace jill { namespace util {

/**
 *  Many JILL applications will access an audio stream in both the
 *  real-time thread and a lower-priority main thread.  This class,
 *  which is a thin wrapper around the JACK ringbuffer interface,
 *  allows simultaneous access by read and write threads.
 * 
 *  Client applications can derive from this class or encapsulate it
 *  to provide a wide variety of data handling functionality; note,
 *  however, that due to the performance demands of running in the
 *  real-time thread, none of the member functions are virtual.
 *
 *  @param T the type of object to store in the ringbuffer. Should be POD.
 */
template<typename T>
class Ringbuffer : boost::noncopyable
{
public:
	typedef T data_type;
	typedef std::size_t size_type;
	/** 
	 * Construct a ringbuffer with enough room to hold @a size
	 * objects of type T.
	 * 
	 * @param size The size of the ringbuffer (in objects)
	 */
	Ringbuffer(size_type size) {
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
	inline size_type push(const T *src, size_type nframes) {
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
	inline size_type pop(T *dest, size_type nframes=0) {
		if (nframes==0) 
			nframes = read_space();
		return jack_ringbuffer_read(_rb, reinterpret_cast<char *>(dest), 
					    sizeof(T) * nframes);
	}

	/** 
	 * Read data from the ringbuffer. This version sets the input
	 * argument to the address of an array with the next block of
	 * data.  To free space after using the data, call advance()
	 * 
	 * @param buf   Will point to data in ringbuffer after read
	 * 
	 * @return  The number of available samples in buf
	 */
	inline size_type peek(T **buf) {
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
	inline void advance(size_type nframes) {
		jack_ringbuffer_read_advance(_rb, nframes);
	}

	/// Returns the number of items that can be written to the ringbuffer
	inline size_type write_space() const {
		return jack_ringbuffer_write_space(_rb) / sizeof(T);
	}

	/// Returns the number of items that can be read from the ringbuffer
	inline size_type read_space() const {
		return jack_ringbuffer_read_space(_rb) / sizeof(T);
	}

private:
	jack_ringbuffer_t * _rb;
};

template<typename T>
std::ostream &operator<< (std::ostream &os, const Ringbuffer<T> &o)
{
	return os << "read space " << o.read_space() << "; write space " << o.write_space();
}

/**
 * The RingbufferAdapter is a generic class that uses a Ringbuffer to
 * buffer writing to a backend.  The backend class must have exposed
 * sample_type and size_type types, and a write(const sample_type *in,
 * size_type n) function.
 *
 * The class provides threadsafe buffering.  One thread can call the
 * push() function to add data to the buffer, and the other thread can
 * call flush() to write data to disk.
 *
 */
template <typename Sink>
class RingbufferAdapter : boost::noncopyable {

public:
	typedef typename Sink::sample_type sample_type;
	typedef typename Sink::size_type size_type;
	BOOST_STATIC_ASSERT((boost::is_convertible<size_type,
			     typename Ringbuffer<sample_type>::size_type>::value));

	/// Initialize the buffer with room for buffer_size samples
	RingbufferAdapter(size_type buffer_size, Sink *S=0)
		: _ringbuffer(buffer_size), _sink(S) {}

	void set_sink(Sink *S) { _sink = S; }
	const Sink *get_sink() const { return _sink; }

	/// Store data in the buffer
	inline size_type push(const sample_type *in, size_type n) {
		return _ringbuffer.push(in, n);
	}

        /// write data from the ringbuffer to sink. Returns 0 if successful, -1 otherwise
	inline int flush() {
		sample_type *buf;
		size_type cnt, frames = _ringbuffer.peek(&buf);	
		if (frames && _sink) {
			cnt = _sink->write(buf, frames);
			_ringbuffer.advance(frames);
			return cnt;
		}
		return 0;
	}

private:
	Ringbuffer<sample_type> _ringbuffer;
	Sink *_sink;
};
	

}} // namespace jill::util


#endif // _DAS_RINGBUFFER_HH
