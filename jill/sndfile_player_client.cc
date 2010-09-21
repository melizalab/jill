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

#include "sndfile_player_client.hh"
#include "util/sndfile.hh"
#include "util/logger.hh"
#include <jack/jack.h>
#include <iostream>
#include <samplerate.h>

using namespace jill;

SndfilePlayerClient::SndfilePlayerClient(const char * client_name)
	: PlayerClient(client_name), _buf_pos(0), _buf_size(0)
{
	if (jack_activate(_client))
		throw AudioError("can't activate client");
}

SndfilePlayerClient::~SndfilePlayerClient()
{
	jack_deactivate(_client);
}

nframes_t
SndfilePlayerClient::load_file(const key_type &key, const char * audiofile)
{
	if (audiofile == 0) return 0;
	util::sndfilereader sf(audiofile);
	nframes_t nframes = sf.frames();
	data_array buf(new sample_t[nframes]);
	sf(buf.get(), nframes);

	return load_data(key, buf, nframes, sf.samplerate());
}

nframes_t
SndfilePlayerClient::load_data(const key_type &key, data_array buf, nframes_t size, nframes_t rate)
{
	if (rate != samplerate()) {
		if (_logger)
			*_logger << util::logstream::allfields << key << ": resampling " 
				 << size << " samples from " 
				 << rate << "->" << samplerate() << " Hz" << std::endl;
		SRC_DATA data;
		data.input_frames = size;
		data.data_in = buf.get();
		data.src_ratio = float(samplerate()) / float(rate);
		size = data.output_frames = (int)(size * data.src_ratio);

		_buf.reset(new sample_t[size]);
		data.data_out = _buf.get();

		int ec = src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
		if (ec != 0)
			throw AudioError(std::string("Resampling error: ") + src_strerror(ec));
	}
	else
		_buf = buf;
	
	_buf_size = _buf_pos = size; // otherwise playback starts immediately
	_buffers[key] = _buf;
	_buffer_sizes[key] = _buf_size;
	return _buf_size;
}

nframes_t
SndfilePlayerClient::select(const key_type &key)
{
	std::map<key_type, data_array>::const_iterator it1 = _buffers.find(key);
	std::map<key_type, nframes_t>::const_iterator it2 = _buffer_sizes.find(key);
	if (it1 == _buffers.end() || it2 == _buffer_sizes.end())
		throw NoSuchKeyError(key);

	_buf_name = it1->first;
	_buf = it1->second;
	_buf_size = _buf_pos = it2->second;
	return _buf_size;
}

void
SndfilePlayerClient::fill_buffer(sample_t *buffer, nframes_t nframes)
{
	int avail = _buf_size - _buf_pos;
	if (avail > 0) {
		if (avail > int(nframes)) avail = nframes;
		memcpy(buffer, _buf.get()+_buf_pos, avail * sizeof(sample_t));
		_buf_pos += avail;
	}

}

void
SndfilePlayerClient::_stop(const char *reason)
{
	_buf_pos = _buf_size;
	_status_msg = reason;
}


namespace jill {

std::ostream &
operator<< (std::ostream &os, const SndfilePlayerClient &client)
{
	os << client._buf_name << ": " << client._buf_size << " @ " << client.samplerate()
	   << "Hz (current position: " << client._buf_pos << ')';
	return os;
}

}
