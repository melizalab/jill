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
#ifndef _WINDOW_DISCRIMINATOR_HH
#define _WINDOW_DISCRIMINATOR_HH

#include <boost/noncopyable.hpp>
#include <vector>
#include <jill/util/counter.hh>


namespace capture {

/**
 * The ThresholdCounter is a simple window discriminator that counts
 * the number of times a signal crosses a threshold within a
 * window. 
 */
template<typename T>
class ThresholdCounter : public jill::util::Counter {
public:
	using jill::util::Counter;
	typedef T sample_type;

	ThresholdCounter(const sample_type &threshold, size_type period_size, size_type period_count)
		:  Counter(period_count), _thresh(threshold), _period_size(period_size),
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
 	int push(const sample_type *samples, size_type size, int count_thresh) {
		int ret = -1, period = 0;
		ASSERT(size > 1);
		sample_type last = *samples;
		for (size_t i = 1; i < size; ++i) {
			// I only check positive crossings because
			// it's faster and there's not much point in
			// counting both for most signals
			if (last < _thresh && samples[i] >= _thresh)
				_period_crossings += 1;
			last = samples[i];
			_period_nsamples += 1;
			if (_period_nsamples >= _period_size) {
				if (Counter::push(_period_crossings, count_thresh) && ret < 0)
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
		Counter::reset();
		_period_crossings = 0;
		_period_nsamples = 0;
	}

	inline size_type period_size() const { return _period_size; }
	sample_type &thresh() { return _thresh; }

private:
	/// sample threshold
	volatile sample_type _thresh;
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
 * There is no underlying storage for samples.  Depending on the
 * application, the storage might need to be re-entrant, or involve
 * prebuffering.
 *
 * Choosing the parameters can be a bit tricky, so a few pointers:
 *
 * The open/close sample thresholds determine the amplitude
 * sensitivity of the gates. They can be adjusted dynamically using
 * the open_thresh() and close_thresh() methods if necessary.
 *
 * The crossing rate, analysis window, and period size all contribute
 * to the threshold for average crossing rate, which is equal to
 * count_thresh / (period_size * window_periods).  Crossing rate is
 * related to the frequency and power of the signal.
 *
 * The integration time is determined by period_size * window_periods.
 * Longer integration times make the gates less sensitive to temporary
 * dips or spikes in power, at some cost in sensitivity and temporal
 * resolution.
 *
 * The analysis granularity is controlled by period_size.  Longer
 * periods are more efficient; smaller periods carry more fine-grained
 * temporal information.
 */
template <typename T>
class WindowDiscriminator : boost::noncopyable {
public:
	typedef T sample_type;
	typedef typename ThresholdCounter<T>::size_type size_type;

	/**
	 * Instantiate a window discriminator.
	 *
	 * @param othresh          The opening threshold (in sample units)
	 * @param ocount_thresh    The mininum number of crossings to open the gate
	 * @param owindow_periods  The number of periods to analyze for opening
	 * @param cthresh          The closing threshold (in sample units)
	 * @param ccount_thresh    Crossings must stay above this rate to keep gate open
	 * @param cwindow_periods  The number of periods to analyze for closing
	 * @param period_size      The size of the analysis period
	 */
	WindowDiscriminator(const sample_type &othresh, int ocount_thresh, size_type owindow_periods,
			    const sample_type &cthresh, int ccount_thresh, size_type cwindow_periods,
			    size_type period_size)
		: _open(false), 
		  _open_counter(othresh, period_size, owindow_periods),
		  _close_counter(cthresh, period_size, cwindow_periods),
		  _open_count_thresh(ocount_thresh), 
		  _close_count_thresh(-ccount_thresh) {} // note sign reversal

	/**
	 *  Analyze a block of samples. Depending on the state of the
	 *  discriminator, they are either pushed to the
	 *  open-threshold counter or the close-threshold counter. If
	 *  the active counter changes state, the gate is opened or
	 *  closed, and the offset in the supplied data where this
	 *  occurred (to the nearest period) is returned.  If no state
	 *  changed, -1 is returned.
	 *
	 *  @param samples    The input samples
	 *  @param size       The number of available samples
	 *  @returns          The sample offset where a state change occurred, or 
                              -1 if no state change occurred
	 *                    
	 */
	int push(const sample_type *samples, size_type size) {
		if (_open) {
			int per = _close_counter.push(samples, size, _close_count_thresh);
			if (per > -1) {
				int offset = per * _close_counter.period_size();
				_open = false;
				_close_counter.reset();
				// push samples after offset to open counter
				_open_counter.push(samples+offset, size-offset, _open_count_thresh);
				return offset;
			}
		}
		else {
			int per = _open_counter.push(samples, size, _open_count_thresh);
			if (per > -1) {
				int offset = per * _open_counter.period_size();
				_open = true;
				_open_counter.reset();
				// push samples after offset to close counter
				_close_counter.push(samples+offset, size-offset, _open_count_thresh);
				return offset;
			}
		}
		return -1;
	}

	bool open() const { return _open; }
	sample_type &open_thresh() { return _open_counter.thresh(); }
	sample_type &close_thresh() { return _close_counter.thresh(); }


	friend std::ostream& operator<< (std::ostream &os, const WindowDiscriminator<T> &o) {
		os << "Gate: " << ((o._open) ? "open" : "closed") << std::endl
		   << "Open counter (" << o._open_count_thresh << "): " << o._open_counter << std::endl
		   << "Close counter (" << o._close_count_thresh << "): " << o._close_counter << std::endl;
		return os;
	}

#ifndef NDEBUG
	void print_counters(std::ostream &os) {
		for (int i = 0; i < _open_counter._count_storage.size() ; ++i)
			os << _open_counter._count_storage[i] << ',';
		os << std::endl;
		for (int i = 0; i < _close_counter._count_storage.size() ; ++i)
			os << _close_counter._count_storage[i] << ',';
		os << std::endl;
	}
#endif

private:

	volatile bool _open;
	ThresholdCounter<sample_type> _open_counter;
	ThresholdCounter<sample_type> _close_counter;
	int _open_count_thresh;
	int _close_count_thresh;
};

} // namespace capture
#endif
