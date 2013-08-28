/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _CROSSING_COUNTER_HH
#define _CROSSING_COUNTER_HH

#include <boost/noncopyable.hpp>
#include "counter.hh"

namespace jill { namespace dsp {

/**
 * Counts the number of times a signal crosses a threshold within a time window.
 *
 * Data are passed to the counter in blocks. The counter adds the number of
 * crossings in the block to a queue (@see jill::dsp::running_counter) to obtain
 * a moving sum of the counts in previous blocks.
 */
template<typename T>
class crossing_counter : boost::noncopyable {
public:
	typedef T sample_type;
        typedef int32_t count_type;
        typedef std::size_t size_type;


	crossing_counter(const sample_type &threshold, size_type period_size, size_type period_count)
		:  _counter(period_count), _thresh(threshold), _period_size(period_size),
		   _period_crossings(0), _period_nsamples(0) {
		_max_crossings = period_count * _period_size / 2;
	}

	/**
	 * Analyze a block of samples. The samples are divided into smaller
	 * analysis periods and the number of counts in each period pushed on to
	 * a queue. Each time the queue is updated, the total crossing count in
	 * the analysis window is compared against a count threshold. The return
	 * value of the function indicates the block in which the count crossed
	 * this threshold.
	 *
	 * @param samples      A buffer of samples to analyze
	 * @param size         The number of samples in the buffer. Must be at least 2
	 * @param count_thresh The count threshold. Can be negative or positive,
	 *                     to indicate which crossing direction to look for.
	 * @param state        If this is not null, copies the running count in each sample
	 *                     to this buffer. Needs to be at least same size as samples.
	 *                     Useful for debug.
	 *
	 * @returns  -1 if no crossing occurred (including calls with samples < period_size)
	 *           N=0+ if a crossing occurred in the Nth block of data
	 *
	 */
 	int push(const sample_type * samples, size_type size, count_type count_thresh, sample_type * state=0) {
		int ret = -1, period = 0;
		sample_type last = *samples;
		if (state)
			state[0] = float(_counter.running_count()) / _max_crossings;
		for (size_t i = 1; i < size; ++i) {
			// I only check positive crossings because
			// it's faster and there's not much point in
			// counting both for most signals
			if (last < _thresh && samples[i] >= _thresh)
				_period_crossings += 1;
			last = samples[i];
			_period_nsamples += 1;
			if (_period_nsamples >= _period_size)
			{
				_counter.push(_period_crossings);
				if (_counter.full() && ret < 0) {
					if (count_thresh > 0 && _counter.running_count() > count_thresh)
						ret = period;
					else if (count_thresh < 0 && _counter.running_count() < -count_thresh)
						ret = period;
				} // if (ret < 0)
				period += 1;
				_period_nsamples = 0;
				_period_crossings = 0;
			}
			if (state)
				state[i] = float(_counter.running_count()) / _max_crossings;
			// here, ret should be the period in blocks or -1 if no crossing
		}
		return ret;
	}

	/** The state of the counter */
	int count() const { return _counter.running_count(); }

	/** reset the queue */
	void reset() {
		_counter.reset();
		_period_crossings = 0;
		_period_nsamples = 0;
	}
        /** @return the size of the analysis period (in samples) */
	size_type period_size() const { return _period_size; }
        /** @return current value of the threshold. */
        sample_type thresh() const { return _thresh;}

private:
        /// running count of crossings
        running_counter<count_type> _counter;

	/// sample threshold
	volatile sample_type _thresh;
	/// analysis period size
	size_type _period_size;
	/// number of analysis periods
	size_type _period_count;

	/// count of crossings in the current period (which may span calls to push)
	count_type _period_crossings;
	/// number of samples analyzed in the current period
	size_type _period_nsamples;
        /// max possible crossings in the period (used to normalize)
	count_type _max_crossings;

};


}} //jill::dsp

#endif
