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

#include "player_client.hh"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <string>
#include <map>

namespace jill {

namespace util { class logstream; }

/**
 * @ingroup clientgroup
 * @brief play soundfiles to output port
 *
 * Implements PlayerClient using sound files as the source of its
 * data. Provides functions to load data from sound files, or from
 * other buffers.  Data are resampled, on loading, to match the
 * samplerate of the JACK server.  Multiple files can be loaded and
 * accessed by key.
 *
 * To load a new waveform:
 * load_file(key, filename);
 *
 * To load pre-existing data, resampling as needed:
 * load_data(key, data, sampling_rate);
 *
 * To select an item:
 * select(key);
 *
 * To start playback of the last loaded item:
 * run();
 * In nonblocking mode:
 * oneshot();
 *
 */
class SndfilePlayerClient : public PlayerClient {

public:
	/** the type of the key used to index sound buffers */
	typedef std::string key_type;

	/** the type of variable to store sound in */
	typedef boost::shared_array<sample_t> data_array;

	/** thrown when the user tries to access a non-existent buffer */
	struct NoSuchKeyError : public std::runtime_error {
		NoSuchKeyError(const key_type &k) : std::runtime_error(k + " does not exist") {}
	};

	SndfilePlayerClient(const char * client_name);
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
				ret.reset(new SndfilePlayerClient(path.c_str()));
				ret->set_logger(os);
				ret->load_file(path, path.c_str());
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
	 *  @param key    an identifying key
	 *  @param path   the location of the file to load
	 *
	 *  @return number of samples loaded (adjusted for resampling)
	 */
	nframes_t load_file(const key_type & key, const char * audiofile);

	/**
	 *  Copy data from another buffer to the object. The data are
	 *  resampled to match the server's sampling rate, so this can
	 *  be a computationally expensive call.
	 *
	 *  @param key    an identifying key
	 *  @param buf    the data buffer
	 *  @param size   the number of samples in the buffer
	 *  @param rate   the sampling rate of the data
	 *
	 *  @return number of samples loaded (adjusted for resampling)
	 */
	nframes_t load_data(const key_type &key, data_array buf, nframes_t size, nframes_t rate);

	/**
	 * Select which item to play.
	 *
	 * @param key      an identifying key
	 *
	 * @throws NoSuchKey  if the key is invalid
	 * @return            the number of frames in the selected dataset
	 */
	nframes_t select(const key_type &key);

	/**
	 * Return true if the key is valid
	 */
	bool has_key(const key_type &key) const { return (_buffers.find(key) != _buffers.end()); }

	/**
	 * Resampling can take quite a while for long signals, so it's
	 * useful to give the user some feedback. If a logger is
	 * supplied, notifications will be sent to it.
	 *
	 * @param os     the @a logstream object
	 */
	void set_logger(util::logstream *os) { _logger = os; }

	friend std::ostream & operator<< (std::ostream &os, const SndfilePlayerClient &client);

protected:
	// for logging resampling events; derivers might enjoy using it too
	util::logstream *_logger;

private:

	void fill_buffer(sample_t *buffer, nframes_t frames);

	virtual void _stop(const char *reason);
	virtual void _reset() { _buf_pos = 0; }
	virtual bool _is_running() const { return (_buf_pos < _buf_size); }

	// the currently selected dataset
	key_type _buf_name;
	data_array _buf;
	volatile nframes_t _buf_pos;
	nframes_t _buf_size;

	// the stored buffers
	std::map<key_type, data_array> _buffers;
	std::map<key_type, nframes_t> _buffer_sizes;

};

} // namespace jill

#endif
