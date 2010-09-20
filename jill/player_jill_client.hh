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
#ifndef _PLAYER_JILL_CLIENT_HH
#define _PLAYER_JILL_CLIENT_HH

#include "jill_client.hh"
#include "util/buffer_adapter.hh"
#include "util/ringbuffer.hh"
#include "util/sndfile.hh"
#include <boost/shared_ptr.hpp>


namespace jill {

class PlayerJillClient : public JillClient {

public:
	PlayerJillClient(const std::string &client_name, const std::string &audiofile,
			 nframes_t buffer_size);
	virtual ~PlayerJillClient();

        /**
	 * Parse a list of input ports; if one input is of the form
	 * sndfile:path_to_sndfile, it will create a PlayerJillClient
	 * for the sndfile and replace the item in the list with the
	 * correct port name. Only works with one or zero inputs of
	 * this type
	 *
	 * @param ports   an iterable, modifiable sequence of port names
	 * @returns a shared pointer to the PlayerJillClient created, if necessary
	 */
	template <class Iterable>
	static boost::shared_ptr<PlayerJillClient> from_port_list(Iterable &ports, nframes_t buffer_size) {
		using std::string;
		boost::shared_ptr<PlayerJillClient> ret;
		typename Iterable::iterator it;
		for (it = ports.begin(); it != ports.end(); ++it) {
			size_t p = it->find("sndfile:");
			if (p!=string::npos) {
				string path = it->substr(8,string::npos);
				it->assign(path + ":out");
				ret.reset(new PlayerJillClient(path, path, buffer_size));
				return ret;
			}
		}
		return ret;
	}

private:

	virtual int _run(unsigned int usec_delay);
	static int process_callback_(nframes_t, void *);

	jack_port_t *_output_port;
	util::sndfilereader _sfin;
	util::BufferedReader<util::Ringbuffer<sample_t>, util::sndfilereader> _sfbuf;

	// these should never get called:
	virtual void _connect_input(const std::string & port, const std::string * input=0);
	virtual void _connect_output(const std::string & port, const std::string * output=0);
	virtual void _disconnect_all();

};

} // namespace jill

#endif
