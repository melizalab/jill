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
#ifndef _SNDFILE_HH
#define _SNDFILE_HH

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <stdexcept>
#include <string>
#include <sndfile.h>
#include "string.hh"

namespace jill { namespace util {

template <typename T>
class Sndfile : boost::noncopyable {

public:
	struct FileError : public std::runtime_error {
		FileError(const std::string & w) : std::runtime_error(w) { }
	};
	Sndfile() : _nframes(0) {}

	/// Open a new file for output. Old files are truncated.
	void open(const std::string &filename, size_t samplerate);

	/// Write nframes from buf to disk
	inline sf_count_t writef(const T *buf, sf_count_t nframes) {
		sf_count_t n = _writef(buf, nframes);
		_nframes += n;
		return n;
	}

	/// Return the total number of frames written
	sf_count_t nframes() const { return _nframes; }

private:
	sf_count_t _samplerate;
	sf_count_t _nframes;
	boost::shared_ptr<SNDFILE> _sndfile;
	sf_count_t _writef(const T *buf, sf_count_t nframes);

};

template <typename T>
void Sndfile<T>::open(const std::string &filename, size_t samplerate)
{
	_samplerate = samplerate;

	SF_INFO sfinfo;
	std::memset(&sfinfo, 0, sizeof(sfinfo));

	sfinfo.samplerate = samplerate;
	sfinfo.channels = 1;

	// detect desired file format based on filename extension
	std::string ext = get_filename_extension(filename);
	if (ext == "wav") {
		sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
	} else if (ext == "aiff" || ext == "aif") {
		sfinfo.format = SF_FORMAT_AIFF | SF_FORMAT_PCM_16;
	} else if (ext == "flac") {
		sfinfo.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_16;
#ifdef HAVE_SNDFILE_OGG
	} else if (ext == "ogg" || ext == "oga") {
		sfinfo.format = SF_FORMAT_OGG | SF_FORMAT_VORBIS;
#endif
	} else if (ext == "raw" || ext == "pcm") {
		sfinfo.format = SF_FORMAT_RAW | SF_FORMAT_PCM_16;
	} else {
		throw FileError(make_string() << "failed to recognize file extension '" << ext << "'");
	}

	// open output file for writing
	SNDFILE *f = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);
	if (!f) {
		throw FileError(make_string() << "couldn't open '" << filename << "' for output");
	}
	_sndfile.reset(f, sf_close);
}	

template<> inline
sf_count_t Sndfile<float>::_writef(const float *buf, sf_count_t nframes) {
	return sf_writef_float(_sndfile.get(), buf, nframes);
}

template<> inline
sf_count_t Sndfile<double>::_writef(const double *buf, sf_count_t nframes) {
	return sf_writef_double(_sndfile.get(), buf, nframes);	
}

template<> inline
sf_count_t Sndfile<short>::_writef(const short *buf, sf_count_t nframes) {
	return sf_writef_short(_sndfile.get(), buf, nframes);	
}

template<> inline
sf_count_t Sndfile<int>::_writef(const int *buf, sf_count_t nframes) {
	return sf_writef_int(_sndfile.get(), buf, nframes);	
}


}} // namespace jill::util

#endif
