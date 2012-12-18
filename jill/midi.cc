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

short
event_t::message() const
{
        if (size > 2) {
                const short *val = reinterpret_cast<const short*>(buffer+1);
                return *val;
        }
        else
                return 0;
}


event_t::event_t(nframes_t offset, char type, char channel, short data)
{
        time = offset;
        size = 3;
        buffer = new jack_midi_data_t[3];
        buffer[0] = (type & 0xf0) | (channel & 0x0f);
        memcpy(buffer+1, &data, sizeof(short));
}

event_t::event_t(void *port_buffer, uint32_t idx)
{
	jack_midi_event_get(static_cast<jack_midi_event_t*>(this), port_buffer, idx);
}

event_t::~event_t()
{
        delete[](buffer);
}

void
event_t::write(void *port_buffer) const
{
	jack_midi_event_write(port_buffer, time, buffer, size);
}

bool
event_t::operator< (event_t const & other)
{
	return time < other.time;
}
