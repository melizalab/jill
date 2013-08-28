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
 */
template <typename T>
class crossing_trigger : boost::noncopyable {
public:
	typedef T sample_type;
	typedef typename crossing_counter<T>::size_type size_type;

	/**
	 * Instantiate a signal detector.
	 *
	 * @param othresh          The opening threshold (in sample units)
	 * @param ocount_thresh    The mininum number of crossings to open the gate
	 * @param owindow_periods  The number of periods to analyze for opening
	 * @param cthresh          The closing threshold (in sample units)
	 * @param ccount_thresh    Crossings must stay above this rate to keep gate open
	 * @param cwindow_periods  The number of periods to analyze for closing
	 * @param period_size      The size of the analysis period
	 */
	crossing_trigger(const sample_type &othresh, int ocount_thresh, size_type owindow_periods,
			    const sample_type &cthresh, int ccount_thresh, size_type cwindow_periods,
			    size_type period_size)
		: _open(false),
		  _open_counter(othresh, period_size, owindow_periods),
		  _close_counter(cthresh, period_size, cwindow_periods),
		  _open_count_thresh(ocount_thresh),
		  _close_count_thresh(-ccount_thresh) {} // note sign reversal

	/**
	 *  Analyze a block of samples. Depending on the state of the detector,
	 *  they are either pushed to the open-threshold counter or the
	 *  close-threshold counter. If the active counter changes state, the
	 *  gate is opened or closed, and the offset in the supplied data where
	 *  this occurred (to the nearest period) is returned. If no state
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

        /** The state of the detector */
	bool open() const { return _open; }

        /** The threshold for going to an open state */
	sample_type &open_thresh() { return _open_counter.thresh(); }

        /** The threshold for going to a closed state */
	sample_type &close_thresh() { return _close_counter.thresh(); }

private:

	bool _open;
	crossing_counter<sample_type> _open_counter;
	crossing_counter<sample_type> _close_counter;
	int _open_count_thresh;
	int _close_count_thresh;
};

}} // namespace
#endif
