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
#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>


namespace jill {

/**
 * This class is a special output-only JillClient, which is designed
 * to play a soundfile to its input port whenever run() is called, and
 * to output zeros otherwise.  It can be used for testing purposes, or
 * to control an output in response to some other process.
 */
class PlayerJillClient : public JillClient {

public:
	PlayerJillClient(const char * client_name, const char * audiofile);
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
	static boost::shared_ptr<PlayerJillClient> from_port_list(Iterable &ports) {
		using std::string;
		boost::shared_ptr<PlayerJillClient> ret;
		typename Iterable::iterator it;
		for (it = ports.begin(); it != ports.end(); ++it) {
			size_t p = it->find("sndfile:");
			if (p!=string::npos) {
				string path = it->substr(8,string::npos);
				it->assign(path + ":out");
				ret.reset(new PlayerJillClient(path.c_str(), path.c_str()));
				return ret;
			}
		}
		return ret;
	}

	/**
	 *  Load data from a file into the buffer. The data are
	 *  resampled to match the server's sampling rate, so this can
	 *  be a computationally expensive call.
	 *
	 *  @return number of samples loaded (adjusted for resampling)
	 */
	nframes_t load_file(const char * audiofile);

	friend std::ostream & operator<< (std::ostream &os, const PlayerJillClient &client);

private:

	virtual void _stop(const char *reason);
	virtual bool _is_running() const { return (_buf_pos < _buf_size); }
	static int process_callback_(nframes_t, void *);

	jack_port_t *_output_port;
	boost::scoped_array<sample_t> _buf;
	volatile nframes_t _buf_pos;
	nframes_t _buf_size;

	// these should never get called:
	virtual void _connect_input(const char * port, const char * input=0) {}
	virtual void _connect_output(const char * port, const char * output=0) {}
	virtual void _disconnect_all() {}

};

} // namespace jill

#endif
