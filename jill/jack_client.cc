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
#include "jack_client.hh"
#include "util/string.hh"
#include <jack/statistics.h>
#include <cerrno>
#include <algorithm>

using namespace jill;


JackClient::JackClient(std::string const & name)
{
	jack_status_t status;
	_client = jack_client_open(name.c_str(), JackNoStartServer, &status);
        if (_client == NULL) {
                util::make_string err;
                err << "unable to start client (status=" << int(status);
                if (status & JackServerFailed) err << "; couldn't connect to server";
                err << ")";
                throw JackError(err);
        }
        jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
}

JackClient::~JackClient()
{
	jack_client_close(_client);
}

jack_port_t*
JackClient::register_port(std::string const & name, std::string const & type,
                           unsigned long flags, unsigned long buffer_size)
{
        jack_port_t *port = jack_port_register(_client, name.c_str(), type.c_str(), flags, buffer_size);
        if (port == NULL) {
                throw JackError(util::make_string() << "unable to allocate port " << name);
        }
        _ports.push_back(port);
        return port;
}

void
JackClient::unregister_port(std::string const & name)
{
        unregister_port(jack_port_by_name(_client, name.c_str()));
}

void
JackClient::unregister_port(jack_port_t *port)
{
        int ret = jack_port_unregister(_client, port);
        if (ret)
                throw JackError(util::make_string() << "unable to unregister port (err=" << ret << ")");
        _ports.remove(port);
}

void
JackClient::activate()
{
        int ret = jack_activate(_client);
        if (ret)
                throw JackError(util::make_string() << "unable to activate client (err=" << ret << ")");
}

void
JackClient::deactivate()
{
        int ret = jack_deactivate(_client);
        if (ret)
                throw JackError(util::make_string() << "unable to deactivate client (err=" << ret << ")");
}

void
JackClient::connect_port(std::string const & src, std::string const & dest)
{
	// simple name-based lookup
	jack_port_t *p1, *p2;
	p1 = jack_port_by_name(_client, src.c_str());
	if (p1==0) {
		std::string n = util::make_string() << jack_get_client_name(_client) << ":" << src;
		p1 = jack_port_by_name(_client, n.c_str());
		if (p1==0)
			throw JackError(util::make_string() << "the port " << n << " does not exist");
	}

	p2 = jack_port_by_name(_client, dest.c_str());
	if (p2==0) {
		std::string n = util::make_string() << jack_get_client_name(_client) << ":" << dest;
		p2 = jack_port_by_name(_client, n.c_str());
		if (p2==0)
			throw JackError(util::make_string() << "the port " << n << " does not exist");
	}

	int error = jack_connect(_client, jack_port_name(p1), jack_port_name(p2));
	if (error && error != EEXIST)
		// no easy way to trap error message; it gets printed to stdout
		throw JackError(util::make_string() << "can't connect "
				 << src << " to " << dest);
}

void
JackClient::disconnect_all()
{
        std::list<jack_port_t*>::const_iterator it;
        for (it = _ports.begin(); it != _ports.end(); ++it) {
                int ret = jack_port_disconnect(_client, *it);
                if (ret)
                        throw JackError(util::make_string() << "unable to disconnect port (err=" << ret << ")");
        }
}

sample_t*
JackClient::samples(std::string const & name, nframes_t nframes)
{
        return samples(jack_port_by_name(_client, name.c_str()), nframes);
}


sample_t*
JackClient::samples(jack_port_t *port, nframes_t nframes)
{
        if (port)
                return static_cast<sample_t*>(jack_port_get_buffer(port, nframes));
        else
                return 0;
}

void*
JackClient::events(jack_port_t *port, nframes_t nframes)
{
        if (port) {
                void *buf = jack_port_get_buffer(port, nframes);
                if (jack_port_flags(port) & JackPortIsOutput) jack_midi_clear_buffer(buf);
        }
        else
                return 0;
}



nframes_t
JackClient::samplerate() const
{
	return jack_get_sample_rate(_client);
}

std::string
JackClient::name() const
{
	return std::string(jack_get_client_name(_client));
}


bool
JackClient::transport_rolling() const
{
	return (jack_transport_query(_client, NULL) == JackTransportRolling);
}


position_t
JackClient::position() const
{
	position_t pos;
	jack_transport_query(_client, &pos);
	return pos;
}

bool
JackClient::position(position_t const & pos)
{
	// jack doesn't modify pos, should have been const anyway, i guess...
	return (jack_transport_reposition(_client, const_cast<position_t*>(&pos)) == 0);
}

nframes_t
JackClient::frame() const
{
	return jack_get_current_transport_frame(_client);
}

bool
JackClient::frame(nframes_t frame)
{
	return (jack_transport_locate(_client, frame) == 0);
}

int
JackClient::process_callback_(nframes_t nframes, void *arg)
{
	JackClient *self = static_cast<JackClient*>(arg);
        nframes_t time = jack_last_frame_time(self->_client);
	if (self->_process_cb)
                return self->_process_cb(self, nframes, time);
        else
	return 0;
}

