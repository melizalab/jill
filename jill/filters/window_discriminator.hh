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

#include <limits>
#include <cmath>
#include <boost/noncopyable.hpp>
#include "../util/counter.hh"
#include "../util/debug.hh"
#include "../util/ringbuffer.hh"


namespace jill { namespace filters {

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

	inline size_type period_size() const { return _period_size; }

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
 * which is controlled by two counters.  If the signal crosses an open
 * threshold a certain number of times in a given time window, the
 * gate opens; when the signal fails to cross the threshold more than
 * a certain number of times in a time window the gate closes.
 *
 * The underlying storage is a ringbuffer, and the object is intended
 * to be reentrant, so that different threads can call push and pop.
 * The filtering occurs in the push step.
 *
 * The class also keeps track of when the gate opened, so that
 * downstream clients can correctly timestamp the beginning of each
 * episode.
 */
template <typename T1, typename T2>
class WindowDiscriminator : boost::noncopyable {
public:
	typedef T1 sample_type;
	typedef T2 time_type;
	typedef typename util::Ringbuffer<sample_type>::size_type size_type;
	static const bool has_marks = std::numeric_limits<sample_type>::has_quiet_NaN;

	/**
	 * Instantiate a window discriminator with a set of
	 * parameters. Choosing the parameters can be a bit tricky, so
	 * a few pointers:
	 *
	 * The open and close gates are separate processes and should
	 * be tuned independently. The main "user" parameters to
	 * consider are thresholds for samples and for crossing rate.
	 * Crossing rate is the number of crossings per unit time,
	 * i.e. count_thresh / (period_size * window_periods). Larger
	 * period sizes are more efficient; smaller periods give more
	 * fine-grained temporal information.
	 */


	WindowDiscriminator(const sample_type &othresh, int ocount_thresh, size_type owindow_periods,
			    const sample_type &cthresh, int ccount_thresh, size_type cwindow_periods,
			    size_type period_size, size_type buffer_size)
		: _open(false), _sample_buf(buffer_size), 
		  // note: maximum number of open transitions is buffer_size / period_size / 2
		  _time_buf(size_type(ceil(0.5 * buffer_size / period_size))),
		  _open_counter(othresh, period_size, owindow_periods),
		  _close_counter(cthresh, period_size, cwindow_periods),
		  _open_count_thresh(ocount_thresh), 
		  _close_count_thresh(-ccount_thresh) {} // note sign reversal

	void push(const sample_type *samples, size_type size, time_type time) {
		if (_open) {
			int per = _close_counter.push(samples, size, _close_count_thresh);
			if (per > -1) {
				size_type offset = per * _close_counter.period_size();
				if (offset > 0)
					_sample_buf.push(samples, offset);
				_open = false;
				_close_counter.reset();
			}
			else
				_sample_buf.push(samples, size);
		}
		else {
			int per = _open_counter.push(samples, size, _open_count_thresh);
			if (per > -1) {
				size_type offset = per * _open_counter.period_size();
				_sample_buf.push(samples + offset, size - offset);
				_time_buf.push(&time, 1);
				_open = true;
				_open_counter.reset();
			}
		}
	}

	bool open() const { return _open; }

	friend std::ostream& operator<< (std::ostream &os, const WindowDiscriminator<T1,T2> &o) {
		os << "Gate: " << ((o._open) ? "open" : "closed") << std::endl
		   << "Input buffer: " << o._sample_buf << std::endl
		   << "Time buffer: " << o._time_buf << std::endl
		   << "Open counter: " << o._open_counter << std::endl
		   << "Close counter: " << o._close_counter << std::endl;
		return os;
	}

protected:

	template <bool is_float>
	void push_mark() {} // no-op for generic template

private:

	volatile bool _open;
	util::Ringbuffer<sample_type> _sample_buf;
	util::Ringbuffer<time_type> _time_buf;
	ThresholdCounter<sample_type> _open_counter;
	ThresholdCounter<sample_type> _close_counter;
	int _open_count_thresh;
	int _close_count_thresh;
};

// template<typename T>
// void WindowDiscriminator<T>::push_mark<true>() { push(std::numeric_limits<T>::quiet_NaN(),1); }

}} // namespace jill::filters
#endif
