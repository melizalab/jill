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
#include <errno.h>
#include <jack/midiport.h>

/**
 * @file midi.hh
 * @brief midi data types
 */
namespace jill {

struct midi {

        typedef jack_midi_data_t data_type;

        // status bytes
        const static data_type stim_on = 0x00;      // non-standard; message is a string
        const static data_type stim_off = 0x10;     // non-standard; message is a string
        const static data_type info = 0x20;         // non-standard; message is a string

        const static data_type note_off = 0x80;     // used for offsets
        const static data_type note_on = 0x90;      // used for onsets and single events
        const static data_type key_pres = 0xa0;     // key pressure
        const static data_type ctl = 0xb0;          // control messages, function?
        const static data_type sysex = 0xf0;        // system-exclusive messages
        const static data_type sysex_end = 0xf7;    // ends sysex
        const static data_type reset = 0xff;

        // masks for splitting status bytes into type and channel
        const static data_type type_nib = 0xf0;
        const static data_type chan_nib = 0x0f;

        const static data_type default_channel = 0;
        const static data_type default_pitch = 60;
        const static data_type default_velocity = 64;

        /**
         * Write a string message to a midi buffer.  This is inlined for speed.
         *
         * @param buffer   the JACK midi buffer
         * @param time     the offset of the message (in samples)
         * @param status   the status byte
         * @param message  the string message, or 0 to send an empty message
         *
         */
        static int write_message(void * buffer, nframes_t time, data_type status, char const * message=0) {
                size_t len = 1;
                if (message)
                        len += strlen(message) + 1; // add the null byte
                data_type *buf = jack_midi_event_reserve(buffer, time, len);
                if (buf) {
                        buf[0] = status;
                        if (message) strcpy(reinterpret_cast<char*>(buf)+1, message);
                        return 0;
                }
                else
                        return ENOBUFS;
        }

        /**
         * Find an onset or offset event in a midi event stream.
         *
         * @param midi_buffer  the JACK midi buffer
         * @param onset        if true, look for onset events; if false, for offsets
         * @return the time of the first event, or -1 if no event was found.
         */
        static int find_trigger(void const * midi_buffer, bool onset=true) {
                void * buf = const_cast<void *>(midi_buffer);
                jack_midi_event_t event;
                nframes_t nevents = jack_midi_get_event_count(buf);
                for (nframes_t i = 0; i < nevents; ++i) {
                        jack_midi_event_get(&event, buf, i);
                        if (event.size < 1) continue;
                        data_type t = event.buffer[0] & midi::type_nib;
                        if (onset) {
                                if (t == midi::stim_on || t == midi::note_on)
                                        return event.time;
                        }
                        else {
                                if (t ==midi::stim_off || t ==midi::note_off)
                                        return event.time;
                        }
                }
                return -1;
        }
};

}

#endif /* _MIDI_HH */

