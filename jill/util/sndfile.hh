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
template <typename T>
class sndfile : boost::noncopyable {

public:
	typedef T sample_type;
	typedef sf_count_t size_type;

	sndfile(const char *filename, size_type samplerate) 
		: _nframes(0) { open(filename, samplerate); }

	size_type write(const sample_type *in, size_type nsamples) {
		size_type n = _writef(in, nsamples);
		_nframes += n;
		return n;
	}

	/// Return the total number of frames written
	size_type nframes() const { return _nframes; }

protected:

	/// Protected default constructor for subclasses
	sndfile() : _nframes(0) {}

	/// Open a new file for output. Old files are truncated.
	void open(const char *filename, size_type samplerate);


private:
	size_type _nframes;
	SF_INFO _sfinfo;
	boost::shared_ptr<SNDFILE> _sndfile;
	size_type _writef(const sample_type *buf, size_type nframes);

};

template <typename T>
void sndfile<T>::open(const char *filename, size_type samplerate)
{
	std::memset(&_sfinfo, 0, sizeof(_sfinfo));

	_sfinfo.samplerate = samplerate;
	_sfinfo.channels = 1;

	// detect desired file format based on filename extension
	std::string ext = get_filename_extension(filename);
	if (ext == "wav") {
		_sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	} else if (ext == "aiff" || ext == "aif") {
		_sfinfo.format = SF_FORMAT_AIFF | SF_FORMAT_PCM_16;
	} else if (ext == "flac") {
		_sfinfo.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
#ifdef HAVE_SNDFILE_OGG
	} else if (ext == "ogg" || ext == "oga") {
		_sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
#endif
	} else if (ext == "raw" || ext == "pcm") {
		_sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
	} else {
		throw FileError(make_string() << "failed to recognize file extension '" << ext << "'");
	}

	// open output file for writing
	SNDFILE *f = sf_open(filename, SFM_WRITE, &_sfinfo);
	if (!f) {
		throw FileError(make_string() << "couldn't open '" << filename << "' for output");
	}
	_sndfile.reset(f, sf_close);
}	

template<> inline
sndfile<float>::size_type
sndfile<float>::_writef(const float *buf, size_type nframes) {
	return sf_writef_float(_sndfile.get(), buf, nframes);
}

template<> inline
sndfile<double>::size_type 
sndfile<double>::_writef(const double *buf, size_type nframes) {
	return sf_writef_double(_sndfile.get(), buf, nframes);	
}

template<> inline
sndfile<short>::size_type 
sndfile<short>::_writef(const short *buf, size_type nframes) {
	return sf_writef_short(_sndfile.get(), buf, nframes);	
}

template<> inline
sndfile<int>::size_type 
sndfile<int>::_writef(const int *buf, size_type nframes) {
	return sf_writef_int(_sndfile.get(), buf, nframes);	
}

/**
 * This class is an extension of the sndfile to support splitting data
 * across multiple files. It adds a next() function, which closes the
 * current file and opens another one, named sequentially.
 */
template <typename T>
class multisndfile : public sndfile<T> {

public:
	typedef typename sndfile<T>::sample_type sample_type;
	typedef typename sndfile<T>::size_type size_type;
	/**
	 * Instantiate a sndfile family. The filename argument is
	 * replaced by a template.
	 *
	 * @param sprintf-style template with one numeric argument, e.g. "myfile_%04d.wav"
	 */
	multisndfile(const char *templ, size_type samplerate)
		: _fn_templ(templ), _samplerate(samplerate), _file_idx(0) {  next(); }

	/**
	 * Close the current file and open a new one.
	 *
	 * @returns   The name of the new file
	 */
	const std::string &next() {
		try {
			_current_file = (boost::format(_fn_templ) % ++_file_idx). str();
		}
		catch (const boost::io::format_error &e) {
			throw FileError(make_string() << "invalid filename template: " << _fn_templ);
		}
		open(_current_file.c_str(), _samplerate);
		return _current_file;
	}
		

private:
	std::string _fn_templ;
	std::string _current_file;
	size_type _samplerate;
	int _file_idx;
};

}} // namespace jill::util

#endif
