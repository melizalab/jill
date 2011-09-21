/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _SNDFILE_PLAYER_HH
#define _SNDFILE_PLAYER_HH

#include "types.hh"
#include <boost/shared_array.hpp>
#include <boost/filesystem.hpp>
#include <map>
#include <string>

namespace jill {

namespace util { class logstream; }

/**
 * @ingroup iogroup
 * @brief   provides playback from a dictionary of sound files
 *
 * Reads a bunch of sound files from disk, resampling as necessary.
 * Functions to retrieve whole files, or to buffer selected files to a
 * JILL client.
 *
 * To load a new waveform:
 * load_file(key, filename);
 *
 * To load pre-existing data, resampling as needed:
 * load_data(key, data, sampling_rate);
 *
 * To access a buffer and/or select that item for playback:
 * select(key);
 *
 * To successively pass chunks of data to a buffer:
 * operator()(*buffer, nsamples);
 */
class SndfilePlayer {

public:
	friend std::ostream & operator<< (std::ostream &os, const SndfilePlayer &client);
	/** the type of the key used to index sound buffers */
	typedef std::string key_type;

	/** the type of variable to store sound in */
	typedef boost::shared_array<sample_t> data_array;

	/** thrown when the user tries to access a non-existent buffer */
	struct NoSuchKeyError : public std::runtime_error {
		NoSuchKeyError(const key_type &k) : std::runtime_error(k + " does not exist") {}
	};

	SndfilePlayer(nframes_t samplerate)
		:  _logger(NULL), _samplerate(samplerate), _buf_pos(0), _buf_size(0) {}
	~SndfilePlayer() {}

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
	nframes_t load_file(const key_type & key, boost::filesystem::path const & audiofile);

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
	nframes_t load_data(const key_type & key, data_array buf, nframes_t size, nframes_t rate);

	/**
	 * Select which item to play.
	 *
	 * @param key      an identifying key
	 *
	 * @throws NoSuchKey  if the key is invalid
	 * @return            the number of frames in the selected dataset
	 */
	nframes_t select(const key_type & key);

	/**
	 * Reset the position of the buffer
	 *
	 * @return            the number of frames in the current buffer
	 */
	nframes_t reset(bool start=true) {
		_buf_pos = (start) ? 0 : _buf_size;
		return _buf_size;
	}

	/**
	 * The current position of the buffer
	 */
	nframes_t position() const { return _buf_size - _buf_pos; }

	/**
	 * Copy data from the currently selected buffer to another
	 * buffer. An expanded form that matches @SimpleClient's
	 * callback signature is provided so that the object can be
	 * used as a callback.
	 *
	 * @param buffer   the target buffer
	 * @param nsamples the number of samples to copy
	 */
	void operator() (sample_t * buffer, nframes_t nsamples);
	void operator() (sample_t * , sample_t *out, sample_t * ,
			 nframes_t nsamples, nframes_t ) {
		operator()(out, nsamples);
	}

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

protected:
	// for logging resampling events
	util::logstream *_logger;

private:
	// playback sample rate
	nframes_t _samplerate;

	// the currently selected buffer
	key_type _buf_name;
	data_array _buf;
	volatile nframes_t _buf_pos;
	nframes_t _buf_size;

	// the stored buffers
	std::map<key_type, data_array> _buffers;
	std::map<key_type, nframes_t> _buffer_sizes;
};

} // ns jill

#endif
