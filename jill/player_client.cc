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

#include "player_client.hh"
#include <jack/jack.h>

using namespace jill;

PlayerClient::PlayerClient(const char * client_name)
	: Client(client_name), _output_port(0)
{
	jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
	if ((_output_port = jack_port_register(_client, "out", JACK_DEFAULT_AUDIO_TYPE,
					       JackPortIsOutput | JackPortIsTerminal, 0))==NULL)
		throw AudioError("can't register output port");
}

int
PlayerClient::process_callback_(nframes_t nframes, void *arg)
{
	PlayerClient *this_ = static_cast<PlayerClient*>(arg);
	try {
		sample_t *out = reinterpret_cast<sample_t *>(jack_port_get_buffer(this_->_output_port,
										  nframes));
		memset(out, 0, nframes * sizeof(sample_t)); // zero buffer
		this_->fill_buffer(out, nframes);
	}
	catch (const std::runtime_error &e) {
		this_->stop(e.what());
	}
	return 0;
}
