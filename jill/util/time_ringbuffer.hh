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
 * The TimeRingbuffer is a specialization of Ringbuffer that stores
 * timing information about the position of the read and write
 * pointers.  The interface is the same as Ringbuffer, with the
 * an extended @a push() and a new function,  @a get_time. 
 *
 * @param T1  The type of data stored in the ringbuffer
 *
 * @param T2 The time time. Any type can be used to represent time,
 * but it is assumed that it is either in units of samples, or that
 * the operator-() and operator+() functions have been overloaded
 * appropriately.
 */
template <typename T1, typename T2>
class TimeRingbuffer : public Ringbuffer<T1> {

public:
	typedef T1 data_type;
	typedef T2 time_type;
	typedef typename Ringbuffer<T1>::size_type size_type;

	TimeRingbuffer(size_type size) : super(size) {}
	TimeRingbuffer(size_type size, const time_type &start_time) : super(size), _wp_time(start_time) {}
	
	/** 
	 * Write data to the ringbuffer. Updates the timing information.
	 * 
	 * @param src Pointer to source buffer
	 * @param nframes The number of frames in the source buffer
	 * @param time    The time corresponding to the start of the data
	 * 
	 * @return The number of frames actually written
	 */
	inline size_type push(const data_type *src, size_type nframes, const time_type &time) {
		boost::mutex::scoped_lock lock(_wp_time_mutex);
		size_type nf = super::push(src, nframes);
		_wp_time = time + nf;
		return nf;
	}

	
	inline time_type get_time() {
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
