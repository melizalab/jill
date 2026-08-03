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
#ifndef _PULSE_HH
#define _PULSE_HH

#include <algorithm>

#include "../types.hh"

namespace jill { namespace dsp {

/** The shape of a pulse emitted in response to an event. */
enum class pulse_shape {
        positive,       ///< full scale positive for the whole duration
        negative,       ///< full scale negative for the whole duration
        biphasic,       ///< positive for the first half, negative for the second
};

/**
 * Render a pulse into a buffer, filling buf[0, duration).
 *
 * The caller is responsible for the buffer having room: jclicker guarantees
 * this by sizing its ringbuffer to a period plus the largest delay plus
 * duration of any configured pulse.
 *
 * Note that a biphasic pulse of odd duration is not charge balanced: the
 * negative phase runs one sample longer than the positive one, and a pulse of
 * duration 1 is entirely negative. That is longstanding behaviour rather than
 * an intended property, and is pinned by the tests.
 *
 * @param buf       start of the region to fill
 * @param shape     the pulse shape
 * @param duration  number of samples to fill
 */
inline void
render_pulse(sample_t * buf, pulse_shape shape, nframes_t duration)
{
        switch (shape) {
        case pulse_shape::positive:
                std::fill(buf, buf + duration, 1.0f);
                break;
        case pulse_shape::negative:
                std::fill(buf, buf + duration, -1.0f);
                break;
        case pulse_shape::biphasic: {
                const nframes_t half = duration / 2;
                std::fill(buf, buf + half, 1.0f);
                std::fill(buf + half, buf + duration, -1.0f);
                break;
        }
        }
}

}} // namespace jill::dsp

#endif
