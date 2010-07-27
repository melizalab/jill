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
 *! @file
 *! @brief Various filters for processing audio streams
 *! 
 *! This file contains interfaces and templates for filtering audio streams
 */
#ifndef _FILTERS_HH
#define _FILTERS_HH

#include <boost/noncopyable.hpp>
#include <deque>


namespace jill {

/**
 * A LIFO buffer for maintaining a fixed delay between input and
 * output data. Has the additional feature that when the buffer is not
 * full, the output is padded with zeros.  To maintain consistency,
 * pushing and popping data occur in a single operation.
 *
 */
template <typename T>
class DelayBuffer : boost::noncopyable {
public:
	typedef typename std::deque<T>::size_type size_type;

	DelayBuffer(typename std::deque<T>::size_type delay)
		: _delay(delay) {}
	
	/**
	 * Add data to the buffer while taking an equal amount
	 * out. If the buffer is not full enough to supply sufficient
	 * data to the output, it is padded.
	 *
	 * @param in   The data to be added to the buffer
	 * @param out  The destination array for the output
	 * @param size The size of the input/output data
	 * @param def  The value to use when padding output
	 */
	inline std::size_t push_pop(const T *in, T *out, unsigned int size, const T &def=0) {
		unsigned int i;
		int Npad;

		for (i = 0; i < size; ++i,++in)
			_buf.push_front(*in);

 		Npad = _delay - _buf.size();
		for (i = 0; i < size && Npad > 0; ++i, --Npad)
			out[i] = def;
		for (; i < size; ++i) {
			out[i] = _buf.back();
			_buf.pop_back();
		}
		return _delay - _buf.size();
	}

	const std::deque<T> &buffer() const { return _buf; }

private:
	std::deque<T> _buf;
	typename std::deque<T>::size_type _delay;

};

} // namespace jill
#endif
