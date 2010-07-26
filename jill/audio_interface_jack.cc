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
#include "audio_interface_jack.hh"

#include <jack/jack.h>
#include <jack/transport.h>
#include <iostream>
#include <cerrno>
#include <cstring>

#include "util/debug.hh"
#include "util/string.hh"

using namespace jill;

AudioInterfaceJack::AudioInterfaceJack(const std::string & name, int port_type)
	: _shutdown(false)
{
	int port_flags = 0;

	if ((_client = jack_client_open(name.c_str(), JackNullOption, NULL)) == 0)
		throw AudioError("can't connect to jack server");

	jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
	jack_on_shutdown(_client, &shutdown_callback_, static_cast<void*>(this));

	if (port_type & JackPortIsInput) {
		if (!(port_type & JackPortIsOutput))
			port_flags = JackPortIsTerminal;
		if ((_input_port = jack_port_register(_client, "in", JACK_DEFAULT_AUDIO_TYPE,
						      JackPortIsInput|port_flags, 0))==NULL)
			throw AudioError("can't register input port");
	}
	else
		_input_port = 0;

	if (port_type & JackPortIsOutput) {
		if (!(port_type & JackPortIsInput))
			port_flags = JackPortIsTerminal;
		if ((_output_port = jack_port_register(_client, "out", JACK_DEFAULT_AUDIO_TYPE,
						       JackPortIsOutput|port_flags, 0))==NULL)
			throw AudioError("can't register output port");
	}
	else 
		_output_port = 0;

	if (jack_activate(_client))
		throw AudioError("can't activate client");
}


AudioInterfaceJack::~AudioInterfaceJack()
{
	jack_deactivate(_client);
	jack_client_close(_client);
}


std::string AudioInterfaceJack::client_name() const
{
	return std::string(jack_get_client_name(_client));
}


pthread_t AudioInterfaceJack::client_thread() const
{
	return jack_client_thread_id(_client);
}


nframes_t AudioInterfaceJack::samplerate() const
{
	return jack_get_sample_rate(_client);
}


bool AudioInterfaceJack::is_shutdown() const
{
	return _shutdown;
}


void AudioInterfaceJack::set_timebase_callback(TimebaseCallback cb)
{
	if (cb) {
		if (jack_set_timebase_callback(_client, 0,
					       &timebase_callback_, static_cast<void*>(this)) != 0) {
			throw AudioError("failed to become jack transport master");
		}
	}
	else {
		if (_timebase_cb) {
			jack_release_timebase(_client);
		}
	}
	_timebase_cb = cb;
}


void AudioInterfaceJack::connect_input(const std::string & port)
{
	if (_input_port) {
		int error = jack_connect(_client, port.c_str(), jack_port_name(_input_port));
		if (error && error != EEXIST)
			throw AudioError(util::make_string() << "can't connect "
					 << jack_port_name(_input_port) << " to " << port.c_str());
	}
	else
		throw AudioError("interface does not have an input port");
}


void AudioInterfaceJack::connect_output(const std::string & port)
{
	if (_output_port) {
		int error = jack_connect(_client, jack_port_name(_output_port), port.c_str());
		if (error && error != EEXIST)
			throw AudioError(util::make_string() << "can't connect "
					 << jack_port_name(_output_port) << " to " << port.c_str());
	}
	else
		throw AudioError("interface does not have an output port");
}


void AudioInterfaceJack::disconnect_all()
{
	jack_port_disconnect(_client, _output_port);
	jack_port_disconnect(_client, _input_port);
}


std::vector<std::string> AudioInterfaceJack::available_ports(JackPortFlags port_type)
{
	std::vector<std::string> v;

	const char **ports = jack_get_ports(_client, NULL, JACK_DEFAULT_AUDIO_TYPE, port_type);

	if (ports) {
		const char **p = ports;
		while (*p) {
			v.push_back(*p++);
		}
		std::free(ports);
	}

	return v;
}


bool AudioInterfaceJack::transport_rolling() const
{
	return (jack_transport_query(_client, NULL) == JackTransportRolling);
}


position_t AudioInterfaceJack::position() const
{
	position_t pos;
	jack_transport_query(_client, &pos);
	return pos;
}


nframes_t AudioInterfaceJack::frame() const
{
	return position().frame;
}


bool AudioInterfaceJack::set_position(position_t const & pos)
{
	// jack doesn't modify pos, should have been const anyway, i guess...
	return (jack_transport_reposition(_client, const_cast<position_t*>(&pos)) == 0);
}


bool AudioInterfaceJack::set_frame(nframes_t frame)
{
	return (jack_transport_locate(_client, frame) == 0);
}


int AudioInterfaceJack::process_callback_(nframes_t nframes, void *arg)
{
	AudioInterfaceJack *this_ = static_cast<AudioInterfaceJack*>(arg);

	sample_t *in=NULL, *out=NULL;
	if (this_->_input_port)
		in = (sample_t *)jack_port_get_buffer(this_->_input_port, nframes);
	if (this_->_output_port)
		out = (sample_t *)jack_port_get_buffer(this_->_output_port, nframes);

	if (this_->_process_cb) {
		try {
			this_->_process_cb(in, out, nframes);
		}
		catch (const std::runtime_error &e) {
			this_->_err_msg = e.what();
			this_->_shutdown = true;
		}
	}

	return 0;
}


void AudioInterfaceJack::timebase_callback_(jack_transport_state_t /*state*/, nframes_t /*nframes*/,
					    position_t *pos, int /*new_pos*/, void *arg)
{
	AudioInterfaceJack *this_ = static_cast<AudioInterfaceJack*>(arg);

	if (this_->_timebase_cb) {
		this_->_timebase_cb(pos);
	}
}


void AudioInterfaceJack::shutdown_callback_(void *arg)
{
	AudioInterfaceJack *this_ = static_cast<AudioInterfaceJack*>(arg);
	this_->_err_msg = "shut down by server";
	this_->_shutdown = true;
}
