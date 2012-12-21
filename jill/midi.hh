/*
 * JILL - C++ framework for JACK
 * Copyright (C) 2012 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MIDI_HH
#define _MIDI_HH 1

#include "types.hh"
#include <jack/midiport.h>

/**
 * @file midi.hh
 * @brief midi data types
 */
namespace jill {

/*
 * @ingroup clientgroup
 * @brief store a control event
 *
 * This class provides storage for MIDI events. These are short
 * messages consisting of the time, a status byte, and a short message. The MIDI
 * standard data message is 2 bytes, but longer messages are possible in the
 * JACK framework.
 *
 * In JACK, MIDI messages are embedded in a buffer and are accessed by index. In
 * some cases it's desirable to move data around outside the JACK system, so
 * this class provides a simple POD representation, along with some useful
 * enumerations for common MIDI types. The message size is set to ensure that at
 * least two events can fit in the shortest reasonable period size (32 frames).
 */
struct event_t {
        enum { MAXIMUM_DATA_SIZE = 40};
        enum midi_type {
                midi_on = 0x90,     // used for onsets and single events
                midi_off = 0x80,    // used for offsets
                midi_ctl = 0xb0,    // control messages, function?
                midi_sys = 0xf0     // system-exclusive messages used for errors, etc
        };

        nframes_t time;            /// time of the event
        std::size_t size;          /// number of used data bytes
        jack_midi_data_t buffer[MAXIMUM_DATA_SIZE];

        jack_midi_data_t& status() { return buffer[0]; }
        jack_midi_data_t const & status() const { return buffer[0]; }

        event_t() : time(0), size(0) {};
        event_t(nframes_t t, jack_midi_data_t status) : time(t), size(1) { buffer[0] = status; }
        explicit event_t(jack_midi_event_t const &event)
                : time(event.time), size((event.size > MAXIMUM_DATA_SIZE) ? MAXIMUM_DATA_SIZE : event.size)
                { memcpy(buffer, event.buffer, size); }

        // type of the event
        int type() const { return status() & 0xf0; }

        // channel of the event
        int channel() const { return status() & 0x0f; }

};

}
#endif /* _MIDI_HH */

