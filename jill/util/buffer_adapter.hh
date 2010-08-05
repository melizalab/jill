/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _BUFFER_ADAPTER_HH
#define _BUFFER_ADAPTER_HH

#include <boost/noncopyable.hpp>
#include <boost/type_traits.hpp>
//#include <boost/function_types/parameter_types.hpp>
#include <boost/static_assert.hpp>

namespace jill { namespace util {


/**
 * The BufferAdapter is a generic class that uses a buffer to wrap
 * I/O operations. For example, a real-time thread may need to
 * dump samples to a ringbuffer while a slower thread writes those
 * samples to disk.
 *
 * @param Buffer  A class implementing:
 *                void push(data_type*,size_type),
 *                size_type peek(data_type**),
 *                void advance(size_type)
 * @param Sink    A class implementing write(data_type*, size_type)
 *
 */
template <class Buffer, class Sink>
class BufferAdapter : public Buffer {

public:
	typedef typename Buffer::data_type data_type;
	typedef typename Buffer::size_type size_type;

	// To do: assert that Sink::write can take data_type, size_type
	//typedef boost::mpl::begin<boost::function_types::parameter_types<typename &Sink::write> >::type t;
	BOOST_STATIC_ASSERT((boost::is_convertible<size_type, typename Sink::size_type>::value));

	/// Initialize the buffer with room for buffer_size samples
	explicit BufferAdapter(size_type buffer_size, Sink *S=0)
		: Buffer(buffer_size), _sink(S) {}

	void set_sink(Sink *S) { _sink = S; }
	const Sink *get_sink() const { return _sink; }

        /// write data from the buffer to sink. @return the number of samples written
	size_type flush() {
		data_type *buf;
		size_type rs = Buffer::read_space();
		if (_sink==0) {
			Buffer::advance(rs);
			return rs;
		}
		// peek at the data until it's exhausted, or we've written at least rs samples
		size_type cnt = 0;
		for (size_type frames = 0; cnt < rs;) {
			frames = Buffer::peek(&buf);
			if (frames) {
				size_type n = _sink->write(buf, frames);
				Buffer::advance(n);
				cnt += n;
			}
			else
				break;
		}
		return cnt;		
	}

private:
	Sink *_sink;
};

}}

#endif
