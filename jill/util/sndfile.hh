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
 *! @file
 *! @brief Classes for writing data to sound files
 *! 
 */

#ifndef _SNDFILE_HH
#define _SNDFILE_HH

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <stdexcept>
#include <string>
#include <sndfile.h>
#include "string.hh"

namespace jill { namespace util {

struct FileError : public std::runtime_error {
	FileError(const std::string & w) : std::runtime_error(w) { }
};

	   	
/**
 * This sound file is a single file, opened using libsndfile. Data
 * supplied to write() are written consecutively to the file.
 */
class sndfile : boost::noncopyable {

public:
	typedef sf_count_t size_type;

	sndfile();
	sndfile(const std::string &filename, size_type samplerate);
	sndfile(const char *filename, size_type samplerate);

	void open(const char *filename, size_type samplerate);
	void close();

	operator bool () const;

	size_type write(const float *buf, size_type nframes);
	size_type write(const double *buf, size_type nframes);
	size_type write(const int *buf, size_type nframes);
	size_type write(const short *buf, size_type nframes);

	/// Return the total number of frames written
	size_type nframes() const { return _nframes; }

private:
	size_type _nframes;
	SF_INFO _sfinfo;
	boost::shared_ptr<SNDFILE> _sndfile;

};

/**
 * This class is an extension of the sndfile to support splitting data
 * across multiple files. It adds a next() function, which closes the
 * current file and opens another one, named sequentially.
 */
class multisndfile : public sndfile {

public:
	/**
	 * Instantiate a sndfile family. The filename argument is
	 * replaced by a template.  Note: call next() before write()!
	 *
	 * @param sprintf-style template with one numeric argument, e.g. "myfile_%04d.wav"
	 */
	multisndfile(const char *templ, size_type samplerate)
		: _fn_templ(templ), _samplerate(samplerate), _file_idx(0) { }
	multisndfile(const std::string &templ, size_type samplerate)
		: _fn_templ(templ), _samplerate(samplerate), _file_idx(0) { }

	/**
	 * Close the current file and open a new one.
	 *
	 * @returns   The name of the new file
	 */
	const std::string &next();

	/// Return the currently open file
	const std::string &current_file() const { return _current_file; }
		

private:
	std::string _fn_templ;
	std::string _current_file;
	size_type _samplerate;
	int _file_idx;
};


/**
 * A thin wrapper around the libsndfile header for reading sound
 * files. This duplicates to some extent the SndfileHandle class in
 * <sndfile.hh> but with a much simpler interface (and no copy semantics)
 */
class sndfilereader : boost::noncopyable {
public:
	typedef sf_count_t size_type;

	sndfilereader();
	sndfilereader(const std::string &path);
	sndfilereader(const char *path);

	void open(const char *path);

	operator bool () const { return _sndfile; };
	size_type samplerate() const { return (_sndfile) ? _sfinfo.samplerate : 0; }
	size_type frames() const { return (_sndfile) ? _sfinfo.frames : 0; }

	inline size_type seek(size_type frames, int whence) {
		return sf_seek(_sndfile.get(), frames, whence);
	}

	inline size_type read(float *buf, size_type nframes) {
		return sf_readf_float(_sndfile.get(), buf, nframes);
	}
	inline size_type read(double *buf, size_type nframes) {
		return sf_readf_double(_sndfile.get(), buf, nframes);
	}
	inline size_type read(int *buf, size_type nframes) {
		return sf_readf_int(_sndfile.get(), buf, nframes);
	}
	inline size_type read(short *buf, size_type nframes) {
		return sf_readf_short(_sndfile.get(), buf, nframes);
	}

private:
	boost::shared_ptr<SNDFILE> _sndfile;
	SF_INFO _sfinfo;

};

}} // namespace jill::util

#endif
