/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _CROSSING_COUNTER_HH
#define _CROSSING_COUNTER_HH

#include <atomic>
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
/* Not copyable, because _thresh is a std::atomic: the threshold can be changed
 * from another thread while push() is running. That is a property of the
 * member rather than a rule imposed here. */
class crossing_counter {
public:
	using sample_type = T;
        using count_type = int32_t;
        using size_type = std::size_t;


	crossing_counter(const sample_type &threshold, size_type period_size, size_type period_count)
		:  _counter(period_count), _thresh(threshold), _period_size(period_size),
		   _period_count(period_count), _period_crossings(0), _period_nsamples(0),
		   _last(0), _have_last(false) {
		_max_crossings = _period_count * _period_size / 2;
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
	 * @returns  -1 if no threshold crossing occurred (including calls with
	 *           fewer than two samples), otherwise the offset of the first
	 *           sample *after* the period in which the count crossed the
	 *           threshold. That offset may equal size, when the period
	 *           closed on the last sample of the block.
	 *
	 * @note this used to return the index of the analysis period rather
	 * than a sample offset, and callers multiplied by period_size() to get
	 * back to samples. That was wrong whenever a period spanned a call,
	 * which is the normal case: the first period in a block closes early
	 * by however many samples of it arrived in the previous one, so the
	 * product understated the offset by up to a full period.
	 */
 	int push(const sample_type * samples, size_type size, count_type count_thresh, sample_type * state=0) {
		int ret = -1;
		// a crossing needs two samples to compare; bail out before
		// dereferencing rather than reading past the end
		if (samples == nullptr || size < 2) return -1;
		/* Loaded once per block, not per sample: a change takes effect
		 * at the next block boundary, which is all volatile ever
		 * offered, and an atomic load in the innermost loop would cost
		 * real time on the realtime path. */
		const sample_type threshold = _thresh.load(std::memory_order_relaxed);
		/* Resume from the previous block's last sample. Seeding from
		 * samples[0] and starting at 1, as this did, made a crossing on
		 * the boundary invisible -- while _period_crossings and
		 * _period_nsamples carried across calls regardless, so the
		 * count depended on how the caller happened to divide the
		 * signal and the two effects did not cancel. */
		size_type i = 0;
		sample_type last = _last;
		if (!_have_last) {
			last = samples[0];
			i = 1;
			if (state)
				state[0] = float(_counter.running_count()) / _max_crossings;
		}
		for (; i < size; ++i) {
			// I only check positive crossings because
			// it's faster and there's not much point in
			// counting both for most signals
			if (last < threshold && samples[i] >= threshold)
				_period_crossings += 1;
			last = samples[i];
			_period_nsamples += 1;
			if (_period_nsamples >= _period_size)
			{
				_counter.push(_period_crossings);
				if (_counter.full() && ret < 0) {
					// where the period closed, in samples
					const int offset = static_cast<int>(i + 1);
					if (count_thresh > 0 && _counter.running_count() > count_thresh)
						ret = offset;
					else if (count_thresh < 0 && _counter.running_count() < -count_thresh)
						ret = offset;
				} // if (ret < 0)
				_period_nsamples = 0;
				_period_crossings = 0;
			}
			if (state)
				state[i] = float(_counter.running_count()) / _max_crossings;
		}
		_last = samples[size - 1];
		_have_last = true;
		return ret;
	}

	/** The state of the counter */
	int count() const { return _counter.running_count(); }

	/** reset the queue */
	void reset() {
		_counter.reset();
		_period_crossings = 0;
		_period_nsamples = 0;
		/* Also forget the previous block. A counter resumes after
		 * sitting idle while the other half of a crossing_trigger ran,
		 * and comparing the next sample against one from before that
		 * gap would invent a crossing. */
		_have_last = false;
	}
        /** @return the size of the analysis period (in samples) */
	size_type period_size() const { return _period_size; }
        /** @return current value of the threshold. */
        sample_type thresh() const { return _thresh;}

private:
        /// running count of crossings
        running_counter<count_type> _counter;

	/// sample threshold
	/* May be changed from another thread while push() is running. It was
	 * volatile, which provides neither atomicity nor ordering. Relaxed is
	 * the right ordering: it is a bare value publishing no other data, so
	 * there is nothing for an acquire to order against. */
	std::atomic<sample_type> _thresh;
	/// analysis period size
	size_type _period_size;
	/// number of analysis periods
	size_type _period_count;

	/// count of crossings in the current period (which may span calls to push)
	count_type _period_crossings;
	/// number of samples analyzed in the current period
	size_type _period_nsamples;
	/// last sample of the previous block, so that a crossing straddling a
	/// block boundary is seen. _have_last is false until the first block
	/// and after reset(), when there is nothing to compare against.
	sample_type _last;
	bool _have_last;
        /// max possible crossings in the period (used to normalize)
	count_type _max_crossings;

};


}} //jill::dsp

#endif
