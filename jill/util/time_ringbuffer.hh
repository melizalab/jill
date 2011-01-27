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
#ifndef _TIME_RINGBUFFER_HH
#define _TIME_RINGBUFFER_HH

#include "ringbuffer.hh"
#include <boost/thread/mutex.hpp>

namespace jill { namespace util {

/**
 * @ingroup buffergroup
 * @brief ringbuffer that also stores time information
 *
 * The TimeRingbuffer is a specialization of Ringbuffer that stores
 * timing information about the position of the read and write
 * pointers.  The interface is the same as Ringbuffer, with
 * an extended @ref push and a new function,  @ref get_time.
 *
 * The TimeRingbuffer has to lock on the time variable to avoid race
 * conditions.
 *
 * @param T1  The type of data stored in the ringbuffer
 *
 * @param T2 The time type. Any type can be used to represent time,
 * but it is assumed that it is either in units of samples, or that
 * the T2::operator- and T2::operator+ functions have been overloaded
 * appropriately.
 */
template <typename T1, typename T2>
class TimeRingbuffer : public Ringbuffer<T1> {

public:
	typedef T1 data_type;
	typedef T2 time_type;
	typedef typename Ringbuffer<T1>::size_type size_type;

	/**
	 * Construct a ringbuffer with enough room to hold @a size
	 * objects of type @a data_type.
	 *
	 * @param size the size of the ringbuffer (in objects)
	 * @start_time the time value to return before any data has been added
	 */
	TimeRingbuffer(size_type size, const time_type &start_time) : super(size), _wp_time(start_time) {}
	explicit TimeRingbuffer(size_type size) : super(size) {}

	/**
	 * Write data to the ringbuffer. Updates the timing information.
	 *
	 * @param src      pointer to source buffer
	 * @param nframes  the number of frames in the source buffer
	 * @param time     the time corresponding to the start of the data
	 *
	 * @return the number of frames actually written
	 */
	size_type push(const data_type *src, size_type nframes, const time_type &time) {
		boost::mutex::scoped_lock lock(_wp_time_mutex);
		size_type nf = super::push(src, nframes);
		_wp_time = time + nf;
		return nf;
	}

	/** @return the time associated with the read pointer */
	time_type get_time() {
		boost::mutex::scoped_lock lock(_wp_time_mutex);
		return _wp_time - super::read_space();
	}


private:
	typedef Ringbuffer<T1> super;

	/// The time of the write pointer
	time_type _wp_time;
	/// A mutex for _wp_time accessors
	boost::mutex _wp_time_mutex;

};

}}

#endif
