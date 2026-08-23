/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _COUNTER_HH
#define _COUNTER_HH

#include <ostream>
#include <stdexcept>
#include <vector>

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
 *
 * The storage is a plain vector used as a ring. That is the whole of what a
 * circular buffer offered here, and owning it keeps the property this class
 * actually depends on visible: it allocates once, at construction, and never
 * again. push() runs in jdetect's process callback, so an allocation on that
 * path would be a realtime fault.
 *
 * @note the running total is maintained incrementally, by adding the value
 * that enters the window and subtracting the one that leaves. For an integer
 * T, which is what jdetect uses, that is exact indefinitely. For a
 * floating-point T it accumulates rounding error without bound, so a
 * long-running instance would drift away from the true sum. Prefer an integer
 * type unless you have a reason not to.
 */
template <class T>
class running_counter {

        /// the storage type
        using storage_type = std::vector<T>;

public:
	/** the data type stored in the counter */
	using data_type = T;
	/** the data type for size information */
        using size_type = typename storage_type::size_type;

	/**
	 * Initialize the counter.
	 *
	 * @param size  the size of the running sum window; must be at least 1
	 * @throws std::invalid_argument if size is zero
	 *
	 * A window of zero has no meaningful sum, and used to be accepted:
	 * full() reported true immediately and the first push() then read the
	 * front of an empty buffer. jdetect derives this from
	 * --open-period / --period-size, so asking for an integration window
	 * shorter than the analysis period segfaulted it in the callback.
	 */
	explicit running_counter(size_type size)
		: _counts(size), _head(0), _size(0), _running_count(0) {
		if (size == 0) {
			throw std::invalid_argument(
				"running_counter window must hold at least one value");
		}
	}

	/**
	 * Add a value to the queue.  If the queue is full, the value at the end
	 * of the queue is dropped.
	 *
	 * @param count          the value to add
	 */
	void push(data_type count) {
                // _head is the next slot to write, which once the window has
                // filled is also the oldest value in it.
                if (full()) {
                        _running_count -= _counts[_head];
                }
                else {
                        _size += 1;
                }
                _counts[_head] = count;
                _head = (_head + 1) % _counts.size();
		_running_count += count;
	}

	/** Whether the queue is full or not */
	bool full() const { return _size == _counts.size(); }

	/** @return the running total */
	data_type running_count() const { return _running_count; }

	/** reset the counter */
	void reset() {
		_head = 0;
		_size = 0;
		_running_count = 0;
	}

        /** output the state of the queue to a stream */
	friend std::ostream& operator<< (std::ostream &os, const running_counter<T> &o) {
		const size_type capacity = o._counts.size();
		os << o._running_count << " [" << o._size << '/' << capacity << "] (";
                // Oldest first, which is insertion order rather than storage
                // order: the oldest value sits _size slots behind the head.
                for (size_type i = 0; i < o._size; ++i) {
                        os << o._counts[(o._head + capacity - o._size + i) % capacity] << ' ';
                }
		return os << ')';
	}

private:
	/// count of samples in complete blocks; allocated once, used as a ring
	storage_type _counts;
	/// next slot to write, and the oldest value once the window is full
	size_type _head;
	/// how much of the window is occupied, up to _counts.size()
	size_type _size;
	/// a running count
	data_type _running_count;
};

}}

#endif
