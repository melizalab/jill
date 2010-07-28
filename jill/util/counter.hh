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

/**
 * This class is a simple queue-based running counter. The queue has a
 * fixed size and data are pushed onto the queue one by one, with a
 * comparison made between the running total and a threshold.
 */
class Counter : boost::noncopyable {
	friend std::ostream& operator<< (std::ostream &os, const Counter &o);
public:
	typedef std::deque<int>::size_type size_type;

	/// Initialize the counter with a total size of size
	Counter(size_t size);

	/**
	 * Add a value to the queue. Check running total against
	 * threshold. If threshold is positive, return true if total
	 * is >= threshold. If negative, return true if <= threshold.
	 */
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
