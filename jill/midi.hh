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

        const static data_type default_pitch = 60;
        const static data_type default_velocity = 64;

        /**
         * Write a string message to a midi buffer.
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
};

}



// /** A wrapper for jack midi buffers that exposes an iterator interface */
// class midi_buffer : boost::noncopyable {

// public:
//         /**
//          * Initialize object by getting the buffer for a JACK port. Will clear
//          * the buffer if it's an output port
//          */
//         midi_buffer(jack_port_t *, nframes_t);

//         /** Initialize the object from a pointer to a JACK midi buffer */
//         explicit midi_buffer(void *);



// private:
//         void * _buffer;

// };

#endif /* _MIDI_HH */

