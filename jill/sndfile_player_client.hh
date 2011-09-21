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
#ifndef _SNDFILE_PLAYER_CLIENT_HH
#define _SNDFILE_PLAYER_CLIENT_HH

#include "simple_client.hh"
#include "sndfile_player.hh"
#include <boost/shared_ptr.hpp>
#include <string>

namespace jill {

namespace util{ class logger;}

/**
 * @ingroup clientgroup
 * @brief play soundfiles to output port
 *
 * An implementation of @a SimpleClient that plays back sound files.
 * Reading, resampling, and buffering sound files is handled by @a
 * SndfilePlayer. Has a single output port.
 *
 * To load a new waveform:
 * load_file(key, filename);
 *
 * To select a loaded file for playback:
 * select(key);
 *
 * To start playback of the last loaded item:
 * run();
 * In nonblocking mode:
 * oneshot();
 *
 */
class SndfilePlayerClient : public SimpleClient {

public:
	SndfilePlayerClient(std::string const & client_name, util::logstream * logger=0);
	virtual ~SndfilePlayerClient();

	/**
	 * Parse a list of input ports; if one input is of the form
	 * sndfile:path_to_sndfile, it will create a SndfilePlayerClient
	 * for the sndfile and replace the item in the list with the
	 * correct port name. Only works with one or zero inputs of
	 * this type
	 *
	 * @param ports   an iterable, modifiable sequence of port names
	 * @return a shared pointer to the SndfilePlayerClient created, if necessary
	 */
	template <class Iterable>
	static boost::shared_ptr<SndfilePlayerClient> from_port_list(Iterable &ports, util::logstream *os) {
		using std::string;
		boost::shared_ptr<SndfilePlayerClient> ret;
		typename Iterable::iterator it;
		for (it = ports.begin(); it != ports.end(); ++it) {
			size_t p = it->find("sndfile:");
			if (p!=string::npos) {
				string path = it->substr(8,string::npos);
				it->assign(path + ":out");
				ret.reset(new SndfilePlayerClient(path));
				ret->_sounds.set_logger(os);
				ret->_sounds.load_file(path, path);
				return ret;
			}
		}
		return ret;
	}

	nframes_t load_file(const SndfilePlayer::key_type & key, std::string const & audiofile) {
		return _sounds.load_file(key, audiofile);
	}

	nframes_t load_data(const SndfilePlayer::key_type & key, SndfilePlayer::data_array buf,
			    nframes_t size, nframes_t rate) {
		return _sounds.load_data(key, buf, size, rate);
	}

	nframes_t select(const SndfilePlayer::key_type & key) {
		return _sounds.select(key);
	}

private:

	virtual void _stop(std::string const & reason);
	virtual void _reset() {
		_sounds.reset();
		_status_msg = "Completed playback";
	}
	virtual bool _is_running() const { return (_sounds.position() > 0); }

	SndfilePlayer _sounds;

};

} // namespace jill

#endif
