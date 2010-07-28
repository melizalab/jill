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

#include "counter.hh"

using namespace jill::util;

Counter::Counter(size_type size) : _size(size), _running_count(0) {}

bool
Counter::push(int count, int count_thresh) 
{
	_counts.push_front(count);
	if (_counts.size() < _size) return false; // insufficient data

	int old_count = _running_count;
	_running_count += count - _counts.back();
	_counts.pop_back();

	if (count_thresh > 0)
		return (old_count < count_thresh && _running_count >= count_thresh);
	else
		return (old_count > -count_thresh && _running_count <= -count_thresh);
}

void
Counter::reset()
{
	_counts.clear();
	_running_count = 0;
}
