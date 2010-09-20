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
 *
 */

#include "player_jill_client.hh"
#include "util/string.hh"
#include <jack/jack.h>

using namespace jill;

PlayerJillClient::PlayerJillClient(const std::string &client_name, const std::string &audiofile,
				   nframes_t buffer_size)
	: JillClient(client_name), _output_port(0), _sfin(audiofile), _sfbuf(buffer_size, &_sfin)
{
	_sfbuf.fill();

	jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
	if ((_output_port = jack_port_register(_client, "out", JACK_DEFAULT_AUDIO_TYPE,
					       JackPortIsOutput | JackPortIsTerminal, 0))==NULL)
		throw AudioError("can't register output port");

	if (jack_activate(_client))
		throw AudioError("can't activate client");
}

PlayerJillClient::~PlayerJillClient()
{
	jack_deactivate(_client);
}

int 
PlayerJillClient::process_callback_(nframes_t nframes, void *arg)
{
	PlayerJillClient *this_ = static_cast<PlayerJillClient*>(arg);
	try {
		sample_t *out = (sample_t *)jack_port_get_buffer(this_->_output_port, nframes);
		nframes_t nf = this_->_sfbuf.pop(out, nframes);
		// underrun could indicate end of file, or failure to fill buffer
		if (nf < nframes)
			this_->_quit = true;
	}
	catch (const std::runtime_error &e) {
		this_->_err_msg = e.what();
		this_->_quit = true;
	}
	return 0;
}	


int
PlayerJillClient::_run(unsigned int usec_delay)
{
	for (;;) {
		::usleep(usec_delay);
		if (_quit) {
			if (!_err_msg.empty())
				throw std::runtime_error(_err_msg);
			else if (_sfin.nread() < _sfin.frames())
				throw std::runtime_error("buffer underrun");
			else
				return 0;
		}
		_sfbuf.fill();
	}
}

void 
PlayerJillClient::_connect_input(const std::string & port, const std::string *)
{
	throw AudioError("unsupported operation for PlayerJillClient");
}

void 
PlayerJillClient::_connect_output(const std::string & port, const std::string *)
{
	throw AudioError("unsupported operation for PlayerJillClient");
}

void 
PlayerJillClient::_disconnect_all()
{
	throw AudioError("unsupported operation for PlayerJillClient");
}

