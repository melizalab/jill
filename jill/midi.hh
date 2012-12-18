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
#include <vector>
#include <list>

/**
 * @file midi.hh
 * @brief midi data types
 */
namespace jill {

/*
 * @ingroup clientgroup
 * @brief store a control event
 *
 * In addition to communicating via realtime sound samples, JACK
 * clients can also send and receive MIDI signals. These consist of a
 * time, a status byte, and up to two data bytes (this is the MIDI
 * standard; longer messages are possible).  The meaning and
 * interpretation of MIDI signals is up to the clients, but a typical
 * use would be to act as a trigger or gate.  In this case, the status
 * byte should be "note on" and the data byte should indicate the state
 * of the gate.  A constructor is supplied for this construct.
 */
struct event_t : public jack_midi_event_t {
        // inherits fields:
        // nframes_t time
        // size_t size (number of bytes in buffer)
        // jack_midi_data_t (char) *buffer

        // define common midi types
        enum midi_type {
                note_on = 0x90,
                note_off = 0x80,
                control = 0xb0
        };

        /** Allocate midi event AND buffer. Not RT safe */
        event_t(size_t size=3);
        /** Copy constructor copies entire data buffer. Not RT safe. */
        event_t(jack_midi_event_t const & evt);
        /** Assigment operator copies as much of data as will fit. Realtime safe */
        event_t & operator=(jack_midi_event_t const & evt);

	event_t(nframes_t offset, char type, char channel, size_t data_size, void * data);

        ~event_t();

        /**
         * Set data in object. Only copies as much data as allocated, so it's RT safe.
         * @param offset  the time of the event
         * @param status  the type of the event (top four bytes only)
         * @param channel the channel of the event (0-16)
         * @param size    the number of bytes in the data
         * @param data    the data of the event
         * @return the number of bytes copied from data buffer
         */
        std::size_t set(nframes_t offset, char type, char channel, short data=0);
        std::size_t set(nframes_t offset, char type, char channel, size_t data_size, void * data);

        /**
         * Read data from a JACK port buffer
         * @param  port_buffer JACK port buffer
         * @param  idx the index of the event to read
         * @return the number of bytes copied from the data buffer
         */
        std::size_t set(void * port_buffer, uint32_t event_index);

        // type of the event
        int type() const { return size && (buffer[0] & 0xf0);}

        // channel of the event
        int channel() const { return size && (buffer[0] & 0x0f);}

	/**
	 * Read the event message as a short
	 * @return the message, or 0 if the data buffer isn't long enough
	 */
	short message() const;

	/**
	 * Write the current event to a JACK port buffer.  Events need
	 * to be written in order.
	 *
	 * @param port_buffer the port buffer to write to
	 */
	void write(void *port_buffer) const;

	/// Less-than operator to make this object sortable
	bool operator< (event_t const & other);

};

template<typename OutputIterator>
int copy(void * port_buffer, OutputIterator it)
{
	nframes_t nevents = jack_midi_get_event_count(port_buffer);
        for (nframes_t i = 0; i < nevents; ++i) {
                int r = jack_midi_event_get(&(*it), port_buffer, i);
                if (r == 0) ++it;
        }
}


}

#endif /* _MIDI_HH */

