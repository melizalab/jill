/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _COUNTER_HH
#define _COUNTER_HH

#include <iosfwd>
#include <boost/noncopyable.hpp>
#include <boost/circular_buffer.hpp>

namespace jill { namespace dsp {

/**
 * @ingroup miscgroup
 * @brief calculate running sum
 *
 * This class is a simple queue-based running counter. The queue has a
 * fixed size and data are pushed onto the queue one by one. The
 * running sum of the values in the queue is maintained by adding the
 * new value and subtracting the last value in the queue, which is
 * then dropped. A comparison is made between the running total and a
 * threshold.
 */
template <class T>
class running_counter : boost::noncopyable {

        /// the storage type
        typedef boost::circular_buffer<T> storage_type;

public:
	/** the data type stored in the counter */
	typedef T data_type;
	/** the data type for size information */
        typedef typename storage_type::size_type size_type;

	/** Initialize the counter. @param size  the size of the running sum window */
	explicit running_counter(size_type size)
		: _counts(size), _running_count(0) {}

	/**
	 * Add a value to the queue.  If the queue is full, the value at the end
	 * of the queue is dropped.
	 *
	 * @param count          the value to add
	 */
	void push(data_type count) {
                if (_counts.full()) {
                        _running_count -= _counts.front();
                }
                _counts.push_back(count);
		_running_count += count;
	}

	/** Whether the queue is full or not */
	bool full() const { return _counts.full(); }

	/** @return the running total */
	data_type running_count() const { return _running_count; }

	/** reset the counter */
	void reset() {
		_counts.clear();
		_running_count = 0;
	}

        /** output the state of the queue to a stream */
	friend std::ostream& operator<< (std::ostream &os, const running_counter<T> &o) {
                std::ostream_iterator<data_type> out(os," ");
		os << o._running_count << " [" << o._counts.size() << '/' << o._counts.capacity() << "] (";
                std::copy(o._counts.begin(), o._counts.end(), out);
		return os << ')';
	}

private:
	/// count of samples in complete blocks
	storage_type _counts;
	/// a running count
	data_type _running_count;
};

}}

#endif
