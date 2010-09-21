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
#include <iostream>

#ifdef HAVE_SRC
#include <samplerate.h>
#endif

using namespace jill;

PlayerJillClient::PlayerJillClient(const char * client_name, const char * audiofile)
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
PlayerJillClient::load_file(const char * audiofile)
{
	_buf_size = _buf_pos = 0;
	if (audiofile == 0)
		return _buf_size;

	util::sndfilereader sf(audiofile);

#ifdef HAVE_SRC
	if (sf.samplerate() != samplerate()) {
		SRC_DATA data;
		boost::scoped_array<sample_t> in(new sample_t[sf.frames()]);
		data.input_frames = sf(in.get(), sf.frames());

		data.src_ratio = float(samplerate()) / float(sf.samplerate());
		_buf_size = data.output_frames = (int)(data.input_frames * data.src_ratio);

		_buf.reset(new sample_t[_buf_size]);
		data.data_in = in.get();
		data.data_out = _buf.get();

		int ec = src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
		if (ec != 0)
			throw AudioError(std::string("Resampling error: ") + src_strerror(ec));
	}
	else {
#endif
		nframes_t nframes = sf.frames();
		_buf.reset(new sample_t[nframes]);
		_buf_size = sf(_buf.get(), nframes);
#ifdef HAVE_SRC
	}
#endif
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
			if (avail > int(nframes)) avail = nframes;
			memcpy(out, this_->_buf.get()+this_->_buf_pos, avail * sizeof(sample_t));
			this_->_buf_pos += avail;
		}
	}
	catch (const std::runtime_error &e) {
		this_->stop(e.what());
	}
	return 0;
}

void
PlayerJillClient::_stop(const char *reason)
{
	_buf_pos = _buf_size;
	_status_msg = reason;
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
