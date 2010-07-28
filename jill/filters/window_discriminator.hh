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
#ifndef _DELAY_BUFFER_HH
#define _DELAY_BUFFER_HH

#include <boost/noncopyable.hpp>
#include "../util/counter.hh"
#include "../util/debug.hh"


namespace jill { namespace filters {

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
	inline std::size_t push_pop(const T *in, T *out, size_type size, const T &def=0) {
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

	const std::deque<T> &buffer() const { return _buf; }

private:
	std::deque<T> _buf;
	size_type _delay;

};



/**
 * The ThresholdCounter is a simple window discriminator that counts
 * the number of times a signal crosses a threshold within a
 * window. 
 */
template<typename T>
class ThresholdCounter : public util::Counter {
public:

	ThresholdCounter(const T &threshold, size_type period_size, size_type period_count)
		:  util::Counter(period_count), _thresh(threshold), _period_size(period_size),
		  _period_crossings(0), _period_nsamples(0) {}

	/**
	 * Analyze a block of samples. The samples are blocked into
	 * smaller analysis periods and the number of counts in each
	 * period pushed on to a queue.  Each time the queue is
	 * updated, the total crossing count in the analysis window is
	 * compared against a count threshold.  The return value of
	 * the function indicates the block in which the count crossed
	 * this threshold.
	 *
	 * @param samples      A buffer of samples to analyze
	 * @param size         The number of samples in the buffer. Must be at least 2
	 * @param count_thresh The count threshold. Can be negative or positive,
	 *                     to indicate which crossing direction to look for.
	 *
	 * @returns  -1 if no crossing occurred (including calls with samples < period_size)
	 *           N=0+ if a crossing occurred in the Nth block of data
	 */
 	int push(const T *samples, size_type size, int count_thresh) {
		int ret = -1, period = 0;
		ASSERT(size > 1);
		T last = *samples;
		for (size_t i = 1; i < size; ++i) {
			// I only check positive crossings because
			// it's faster and there's not much point in
			// counting both for most signals
			if (last < _thresh && samples[i] >= _thresh)
				_period_crossings += 1;
			last = samples[i];
			_period_nsamples += 1;
			if (_period_nsamples >= _period_size) {
				if (util::Counter::push(_period_crossings, count_thresh) && ret < 0)
					ret = period;
				period += 1;
				_period_nsamples = 0;
				_period_crossings = 0;
			}
		}
		return ret;
	}

	/// reset the queue
	void reset() { 
		util::Counter::reset();
		_period_crossings = 0;
		_period_nsamples = 0;
	}

private:
	/// sample threshold
	T _thresh;
	/// analysis period size
	size_type _period_size;
	/// number of analysis periods
	size_type _period_count;

	/// count of crossings in the current period
	int _period_crossings;
	/// number of samples analyzed in the current period
	size_type _period_nsamples;
};

/**
 * A simple window discriminator. The filter is essentially a gate,
 * which opens when the signal crosses the threshold a certain number
 * of times in a given time window.  The gate closes when the signal
 * fails to cross the threshold more than a certain number of times in
 * a time window.
 */
template <typename T>
class WindowDiscriminator : boost::noncopyable {
public:
	typedef typename std::deque<T>::size_type size_type;

	WindowDiscriminator();

	size_type push(const T *samples, size_type size);
	size_type pop(T *buffer, size_type size);

protected:
	

private:
	volatile bool _open;

	std::deque<T> _buf;
};

}} // namespace jill::filters
#endif
