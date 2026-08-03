/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _PLAYBACK_TIMING_HH
#define _PLAYBACK_TIMING_HH

#include <algorithm>
#include <optional>

#include "../types.hh"

namespace jill { namespace dsp {

/**
 * @file playback_timing.hh
 *
 * The arithmetic behind deciding when a stimulus starts, and when the trigger
 * events around it are emitted. Pulled out of jstim's process callback so it
 * can be tested: it is fiddly, it is the part the implementation notes single
 * out as tricky, and it leans on unsigned wraparound in two places.
 *
 * Every offset here is relative to the start of the coming period. An offset
 * of nframes or more means "not during this period", which is how the caller
 * decides to do nothing and wait.
 *
 * All of these work correctly when the frame counter has wrapped, provided the
 * caller derives the elapsed times as `time - last_event`, since that
 * difference is exact under modular arithmetic as long as time >= last_event.
 */

/**
 * Offset at which the next stimulus may start, honouring both minimum
 * spacings.
 *
 * @param since_start   frames since the previous stimulus started
 * @param since_stop    frames since the previous stimulus ended
 * @param min_interval  minimum frames between successive onsets
 * @param min_gap       minimum frames between an offset and the next onset
 * @return              offset into the period, possibly beyond its end
 */
inline nframes_t
next_onset_offset(nframes_t since_start, nframes_t since_stop,
                  nframes_t min_interval, nframes_t min_gap)
{
        const nframes_t until_interval =
                (since_start > min_interval) ? 0 : min_interval - since_start;
        const nframes_t until_gap =
                (since_stop > min_gap) ? 0 : min_gap - since_stop;
        // the stimulus starts once both minimums are satisfied
        return std::max(until_interval, until_gap);
}

/**
 * Offset at which to emit a trigger event that must precede an onset.
 *
 * @param onset_offset  offset at which the stimulus will start
 * @param interval      how far ahead of the onset the event goes
 * @param nframes       length of the period
 * @return              the offset, or nullopt if it is not in this period
 */
inline std::optional<nframes_t>
pretrigger_offset(nframes_t onset_offset, nframes_t interval, nframes_t nframes)
{
        // an onset closer than the interval leaves no room for the warning
        if (onset_offset < interval) return std::nullopt;
        const nframes_t offset = onset_offset - interval;
        if (offset >= nframes) return std::nullopt;
        return offset;
}

/**
 * Offset at which to emit a trigger event following the end of a stimulus.
 *
 * @param since_stop  frames since the stimulus ended
 * @param interval    how far after the offset the event goes
 * @param nframes     length of the period
 * @return            the offset, or nullopt if it is not in this period
 *
 * When the moment has already passed, the subtraction wraps to a very large
 * number and falls outside the period, which is the intended behaviour rather
 * than an accident.
 */
inline std::optional<nframes_t>
posttrigger_offset(nframes_t since_stop, nframes_t interval, nframes_t nframes)
{
        const nframes_t offset = interval - since_stop;
        if (offset >= nframes) return std::nullopt;
        return offset;
}

/**
 * How many samples of a stimulus fit in the remainder of the period.
 *
 * @param stim_nframes  total length of the stimulus
 * @param stim_offset   how much of it has already been played
 * @param nframes       length of the period
 * @param period_offset offset at which copying starts
 */
inline nframes_t
samples_to_copy(nframes_t stim_nframes, nframes_t stim_offset,
                nframes_t nframes, nframes_t period_offset)
{
        if (stim_offset >= stim_nframes || period_offset >= nframes) return 0;
        return std::min(stim_nframes - stim_offset, nframes - period_offset);
}

}} // namespace jill::dsp

#endif
