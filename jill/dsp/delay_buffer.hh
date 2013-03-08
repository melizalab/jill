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
 */
#ifndef _DELAY_BUFFER_HH
#define _DELAY_BUFFER_HH

#include <boost/noncopyable.hpp>
#include <deque>

namespace jill { namespace dsp {

/**
 * @ingroup buffergroup
 * @brief a FIFO buffer for maintaining a fixed delay period
 *
 * This class is an extremely simple wrapper around a deque of a fixed
 * length. Data are added and removed in equal quantities, maintaining
 * a fixed delay between input and output data. Has the additional
 * feature that when the buffer is not full, the output is padded with
 * zeros.  To maintain consistency, pushing and popping data occur in
 * a single operation.
 *
 * @param T   the type of data stored in the buffer
 */
template <typename T>
class delay_buffer : boost::noncopyable {
public:
	typedef typename std::deque<T>::size_type size_type;

	/**
	 * Initialize the delay buffer
	 *
	 * @param delay    the size of the delay (in samples)
	 */
	explicit delay_buffer(size_type delay=0)
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
	 *
	 * @return the number of samples added as padding
	 */
	size_type push_pop(const T *in, T *out, size_type size, const T &def=0) {
		size_type i;
		// push data into the buffer

		for (i = 0; i < size; ++i,++in)
			_buf.push_front(*in);

		// pull data from the buffer, padding as necessary
		int Npad = _delay - _buf.size();
		for (i = 0; i < size && Npad > 0; ++i, --Npad)
			out[i] = def;
		for (; i < size; ++i) {
			out[i] = _buf.back();
			_buf.pop_back();
		}
		return _delay - _buf.size();
	}

	/**
	 * Resize the buffer. If the new size is smaller than the
	 * current size, samples at the end of the buffer (i.e. older
	 * ones) are discarded.
	 *
	 * @param delay     the new size of the buffer
	 */
	void resize(size_type delay) {
		if (delay < _buf.size())
			_buf.resize(delay);
		_delay = delay;
	}

	/** @return a const reference to the underlying deque */
	const std::deque<T> &buffer() const { return _buf; }

private:
	std::deque<T> _buf;
	size_type _delay;

};

}} // namespace jill::dsp
#endif
