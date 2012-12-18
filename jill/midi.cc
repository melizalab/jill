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
 */

#include "midi.hh"

using namespace jill;

event_t::event_t(jack_midi_event_t const & t)
	: offset(t.time)
{
	std::back_insert_iterator<std::vector<jack_midi_data_t> > ii(data);
	std::copy(t.buffer, t.buffer+t.size, ii);
}

void
event_t::set(nframes_t time, short value)
{
	offset = time;
	jack_midi_data_t *buf = reinterpret_cast<jack_midi_data_t*>(&value);
	data.resize(message_size+1);
	data[0] = 0x80;
	std::copy(buf, buf+message_size, data.begin()+1);
}

void
event_t::read(void *port_buffer, nframes_t idx)
{
	jack_midi_event_t t;
	jack_midi_event_get(&t, port_buffer, idx);
	offset = t.time;
	data.resize(t.size);
	std::copy(t.buffer, t.buffer+t.size, data.begin());
}

short
event_t::message() const
{
	if (data.size() < message_size+1) return 0;
	const short *val = reinterpret_cast<const short*>(&data[1]);
	return *val;
}

void
event_t::write(void *port_buffer) const
{
	jack_midi_event_write(port_buffer, offset, &data[0], data.size());
}

bool
event_t::operator< (event_t const & other)
{
	return offset < other.offset;
}

void
event_list::read(void * port_buffer)
{
	nframes_t nevents = jack_midi_get_event_count(port_buffer);
	resize(nevents);
	iterator it = begin();
	for (nframes_t i = 0; i < nevents; ++i, ++it) {
		it->read(port_buffer, i);
	}
}

void
event_list::write(void * port_buffer) const
{
	for (const_iterator it = begin(); it != end(); ++it)
		it->write(port_buffer);
}
