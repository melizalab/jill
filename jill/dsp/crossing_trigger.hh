/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _CROSSING_TRIGGER_HH
#define _CROSSING_TRIGGER_HH

#include <vector>
#include "crossing_counter.hh"

namespace jill { namespace dsp {

/**
 * A simple signal detector based on threshold crossings. The filter is
 * essentially a gate, which is controlled by two counters. If the signal
 * crosses a threshold a certain number of times in a given time window, the
 * gate opens; when the signal fails to cross a threshold (which can be
 * different from the open threshhold) more than a certain number of times in a
 * time window the gate closes.
 *
 * There is no underlying storage for samples. Depending on the application, the
 * storage might need to be re-entrant, or involve prebuffering.
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
class CrossingTrigger : boost::noncopyable {
public:
	typedef T sample_type;
	typedef typename CrossingCounter<T>::size_type size_type;

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
	CrossingTrigger(const sample_type &othresh, int ocount_thresh, size_type owindow_periods,
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
	 *  @param counts     A buffer, at least as large as samples, to store the current state
	 *                    of whatever gate is active.
	 *  @returns          The sample offset where a state change occurred, or
                              -1 if no state change occurred
	 *
	 */
	int push(const sample_type * samples, size_type size, sample_type * counts=0) {
		if (_open) {
			int per = _close_counter.push(samples, size, _close_count_thresh, counts);
			if (per > -1) {
				int offset = per * _close_counter.period_size();
				_open = false;
				_close_counter.reset();
				// push samples after offset to open counter
				_open_counter.push(samples+offset, size-offset, _open_count_thresh,
						   (counts!=0) ? counts+offset : 0);
				return offset;
			}
		}
		else {
			int per = _open_counter.push(samples, size, _open_count_thresh, counts);
			if (per > -1) {
				int offset = per * _open_counter.period_size();
				_open = true;
				_open_counter.reset();
				// push samples after offset to close counter
				_close_counter.push(samples+offset, size-offset, _close_count_thresh,
						    (counts!=0) ? counts+offset : 0);
				return offset;
			}
		}
		return -1;
	}

	bool open() const { return _open; }
	sample_type &open_thresh() { return _open_counter.thresh(); }
	sample_type &close_thresh() { return _close_counter.thresh(); }
        /** Return the current state of the counter. Minus values indicate the close counter  */
        int count() const {
                return (_open) ? _open_counter.count() : -_close_counter.count();
        }

private:

	volatile bool _open;
	CrossingCounter<sample_type> _open_counter;
	CrossingCounter<sample_type> _close_counter;
	int _open_count_thresh;
	int _close_count_thresh;
};

}} // namespace
#endif
