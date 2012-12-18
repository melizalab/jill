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
struct event_t {
	/// The time of the event, relative to the start of the period
	nframes_t offset;
	/// The data of the event; this includes the status byte
	std::vector<jack_midi_data_t> data;
	/// The size of a message containing a short int
	static const std::size_t message_size = sizeof(short) / sizeof(jack_midi_data_t);
	/// The status byte value corresponding to note on
	static const jack_midi_data_t status_on = 0x90;
	/// The status byte value corresponding to note off
	static const jack_midi_data_t status_off = 0x80;

	/// Create an empty event
	event_t() : offset(0) {}
	/// Create an event from a jack_midi_event_t object
	explicit event_t(jack_midi_event_t const & t);

	/**
	 * Set the event message to a status of "note on" and a short
	 * integer value as data.
	 *
	 * @param offset  the time of the event, relative to start of the period
	 * @param data    the content of the event
	 */
	void set(nframes_t offset, short data);

	/**
	 * @return the status byte of the message or 0 if the buffer isn't long enough
	 */
	jack_midi_data_t status() const { return (data.empty()) ? 0 : data[0]; }

	/** 
	 * Read the event message as a short 
	 * @return the message, or 0 if the data buffer isn't long enough 
	 */
	short message() const;

	/**
	 * Load values for this event from a port buffer.
	 *
	 * @param port_buffer  the port buffer to read (@see jack_port_get_buffer)
	 * @param idx  the index of the event 
	 */
	void read(void *port_buffer, nframes_t idx);

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

/** 
 * @ingroup clientgroup
 * @brief Storage for sets of events
 *
 * Stores events in a list and provides some functionality for reading
 * and writing to port buffers.
 */
struct event_list : public std::list<event_t> {

	/**
	 * Read all available events from a port buffer
	 *
	 * @param port_buffer the destination buffer
	 */
	void read(void * port_buffer);

	/**
	 * Write all events to a port buffer
	 *
	 * @param port_buffer the destination buffer
	 */
	void write(void * port_buffer) const;
};


}

#endif /* _MIDI_HH */

