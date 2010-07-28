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
#ifndef _COUNTER_HH
#define _COUNTER_HH

#include <boost/noncopyable.hpp>
#include <deque>

namespace jill { namespace util {

class Counter : boost::noncopyable {
	friend std::ostream& operator<< (std::ostream &os, const Counter &o);
public:
	typedef std::deque<int>::size_type size_type;

	Counter(size_t size);

	/// add a value to a count queue. Return true if it crosses the threshold.
	bool push(int count, int count_thresh);

	/// reset the counter
	void reset();

private:
	/// the analysis window
	size_type _size;
	/// count of samples in complete blocks
	std::deque<int> _counts;
	/// a running count
	int _running_count;

};

}}

#endif
