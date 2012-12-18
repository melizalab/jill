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

event_t::event_t(nframes_t offset, char type, char channel, size_t data_size, void * data)
{
        size = data_size+1;
        buffer = new jack_midi_data_t[size];
        set(offset, type, channel, data_size, data);
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
	jack_midi_event_get(&evt, port_buffer, idx);
        *this = evt;
        return ((size > evt.size) ? evt.size : size) * sizeof(jack_midi_data_t);
}


event_t &
event_t::operator=(jack_midi_event_t const & evt)
{
        std::cout << "assignment" << std::endl;
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
	jack_midi_event_write(port_buffer, time, buffer, size);
}

bool
event_t::operator< (event_t const & other)
{
	return time < other.time;
}
