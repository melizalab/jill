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

#include <iostream>
#include "midi.hh"

using namespace jill;

event_t::event_t(std::size_t sz)
{
        time = 0;
        size = sz;
        buffer = new jack_midi_data_t[sz];
}

event_t::event_t(jack_midi_event_t const & evt)
{
        time = evt.time;
        size = evt.size;
        buffer = new jack_midi_data_t[evt.size];
        std::copy(evt.buffer, evt.buffer + evt.size, buffer);
}

std::size_t
event_t::set(nframes_t offset, char type, char channel, short data)
{
        return set(offset, type, channel, sizeof(short), &data);
}

std::size_t
event_t::set(nframes_t offset, char type, char channel, size_t data_size, void * data)
{
        std::size_t nb = ((size-1 > data_size) ? data_size : size-1) * sizeof(jack_midi_data_t);
        time = offset;
        if (size)
                buffer[0] = (type & 0xf0) | (channel & 0x0f);
        memcpy(buffer+1, data, nb);
        return nb;
}

std::size_t
event_t::set(void *port_buffer, uint32_t idx)
{
        event_t evt;
	int r = jack_midi_event_get(&evt, port_buffer, idx);
        if (r == 0) {
                *this = evt;
                return ((size > evt.size) ? evt.size : size) * sizeof(jack_midi_data_t);
        }
        else
                return 0;
}


event_t &
event_t::operator=(event_t const & evt)
{
        if (this != &evt) {
                std::size_t nb = ((size > evt.size) ? evt.size : size) * sizeof(jack_midi_data_t);
                time = evt.time;
                size = evt.size;
                memcpy(buffer, evt.buffer, nb);
        }
        return *this;
}

event_t::~event_t()
{
        delete[](buffer);
}

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

void
event_t::write(void *port_buffer) const
{
        jack_midi_data_t *buf = jack_midi_event_reserve(port_buffer, time, size);
        if (buf == 0)
                throw JackError("event buffer is full");  // FIXME: handle more gracefully
        memcpy(buf, buffer, size * sizeof(jack_midi_data_t));
}

bool
event_t::operator< (event_t const & other)
{
	return time < other.time;
}

std::ostream&
operator<< (std::ostream &os, jill::event_t const &evt)
{
        os << evt.time << ", size " << evt.size << ", ";
        switch(evt.type()) {
        case event_t::note_on:
                os << "note on, channel " << evt.channel();
                break;
        case event_t::note_off:
                os << "note off, channel " << evt.channel();
                break;
        case 0x0:
                os << "empty";
                break;
        default:
                os << "unknown type";
                break;
        }
        return os;
}
