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
#include "util/sndfile.hh"
#include <jack/jack.h>
#include <ostream>

using namespace jill;

PlayerJillClient::PlayerJillClient(const std::string &client_name, const std::string &audiofile)
	: JillClient(client_name), _output_port(0), _buf_pos(0), _buf_size(0)
{
	jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
	if ((_output_port = jack_port_register(_client, "out", JACK_DEFAULT_AUDIO_TYPE,
					       JackPortIsOutput | JackPortIsTerminal, 0))==NULL)
		throw AudioError("can't register output port");

	load_file(audiofile);
	
	if (jack_activate(_client))
		throw AudioError("can't activate client");
}

PlayerJillClient::~PlayerJillClient()
{
	jack_deactivate(_client);
}

nframes_t
PlayerJillClient::load_file(const std::string &audiofile)
{
	_buf_size = _buf_pos = 0;
	if (audiofile.empty())
		return _buf_size;

	util::sndfilereader sf(audiofile);
	nframes_t nframes = sf.frames();
	_buf.reset(new sample_t[nframes]);
	_buf_size = sf(_buf.get(), nframes);
	return _buf_size;
}

int 
PlayerJillClient::process_callback_(nframes_t nframes, void *arg)
{
	PlayerJillClient *this_ = static_cast<PlayerJillClient*>(arg);
	try {
		sample_t *out = reinterpret_cast<sample_t *>(jack_port_get_buffer(this_->_output_port, 
										  nframes));
		memset(out, 0, nframes * sizeof(sample_t)); // zero buffer

		int avail = this_->_buf_size - this_->_buf_pos;
		if (avail > 0) {
			if (avail > nframes) avail = nframes;
			memcpy(out, this_->_buf.get()+this_->_buf_pos, avail * sizeof(sample_t));
			this_->_buf_pos += avail;
		}
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
	_buf_pos = 0; // reset position
	if (usec_delay==0) return 0;
	for (;;) {
		::usleep(usec_delay);
		if (_quit) {
			if (!_err_msg.empty())
				throw std::runtime_error(_err_msg);
			else
				return 0;
		}
		if (_buf_pos >= _buf_size)
			return 0;
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

namespace jill {

std::ostream &
operator<< (std::ostream &os, const PlayerJillClient &client)
{
	os << client.client_name() << ": " << client._buf_size << " @ " << client.samplerate()
	   << "Hz (current position: " << client._buf_pos << ')';
	return os;
}

}
