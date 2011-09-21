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
#include "simple_client.hh"
#include <jack/jack.h>

using namespace jill;

SimpleClient::SimpleClient(std::string const & client_name,
			   std::string const & input_name,
			   std::string const & output_name,
			   std::string const & ctrl_name, bool ctrl_input)
	: Client(client_name), _output_port(0), _input_port(0), _ctrl_port(0)
{
	long port_flags;

	jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));

	if (!input_name.empty()) {
		port_flags = JackPortIsInput | ((output_name.empty()) ? JackPortIsTerminal : 0);
		if ((_input_port = jack_port_register(_client, input_name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
						      port_flags, 0))==NULL)
			throw AudioError("can't register input port");
	}

	if (!output_name.empty()) {
		port_flags = JackPortIsOutput | ((input_name.empty()) ? JackPortIsTerminal : 0);
		if ((_output_port = jack_port_register(_client, output_name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
						       port_flags, 0))==NULL)
			throw AudioError("can't register output port");
	}

	if (!ctrl_name.empty()) {
		port_flags = ((ctrl_input) ? JackPortIsInput : JackPortIsOutput) | JackPortIsTerminal;
		if ((_ctrl_port = jack_port_register(_client, ctrl_name.c_str(), JACK_DEFAULT_AUDIO_TYPE,
						       port_flags, 0))==NULL)
			throw AudioError("can't register control port");
	}

	if (jack_activate(_client))
		throw AudioError("can't activate client");

}

SimpleClient::~SimpleClient()
{
	_disconnect_all();
	jack_deactivate(_client);
}

int
SimpleClient::process_callback_(nframes_t nframes, void *arg)
{
	SimpleClient *this_ = static_cast<SimpleClient*>(arg);

	if (this_->_process_cb) {
		try {
			sample_t *in=NULL, *out=NULL, *ctrl=NULL;
			if (this_->_input_port)
				in = (sample_t *)jack_port_get_buffer(this_->_input_port, nframes);
			if (this_->_output_port)
				out = (sample_t *)jack_port_get_buffer(this_->_output_port, nframes);
			if (this_->_ctrl_port)
				ctrl = (sample_t *)jack_port_get_buffer(this_->_ctrl_port, nframes);
			nframes_t time = jack_last_frame_time(this_->_client);

			this_->_process_cb(in, out, ctrl, nframes, time);
		}
		catch (const std::runtime_error &e) {
			this_->stop(e.what());
		}
	}

	return 0;
}

void
SimpleClient::_disconnect_all()
{
	if (_output_port) jack_port_disconnect(_client, _output_port);
	if (_input_port) jack_port_disconnect(_client, _input_port);
	if (_ctrl_port) jack_port_disconnect(_client, _ctrl_port);
}
