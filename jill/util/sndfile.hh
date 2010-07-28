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
#include <stdexcept>
#include <string>
#include <sndfile.h>
#include "string.hh"

namespace jill { namespace util {

struct FileError : public std::runtime_error {
	FileError(const std::string & w) : std::runtime_error(w) { }
};


/**
 * A sndfile is any object that can act as a sink for audio data.
 * They are intended to be used as back-ends for buffered writing
 * systems.
 */
template <typename T>
class sndfile : boost::noncopyable {

public:
	typedef T sample_type;
	typedef sf_count_t size_type;

	/**
	 * The "sink" function for data. The data may be written
	 * directly to disk, or delayed in a buffer.
	 *
	 * @param in         Samples to write
	 * @param nframes    Number of samples in the input buffer
	 * @return The number of samples actually used
	 */
	virtual size_type write(const sample_type *in, size_type nsamples) = 0;

	/**
	 * For backends that support multiple files or entries, this
	 * advances to the next one.
	 *
	 * @return   The number of frames written to the last entry
	 */
	virtual size_type next() = 0;

	/// Return the number of frames written to the current entry
	virtual size_type nframes() const = 0;
	 
};
	   	
/**
 * This sound file is a single file, opened using libsndfile. Data
 * supplied to write() are written consecutively to the file.
 */
template <typename T>
class single_sndfile : boost::noncopyable {

public:
	typedef T sample_type;
	typedef sf_count_t size_type;

	single_sndfile(const char *filename, size_type samplerate) 
		: _nframes(0) { open(filename, samplerate); }

	size_type write(const sample_type *in, size_type nsamples) {
		size_type n = _writef(in, nsamples);
		_nframes += n;
		return n;
	}

	/// Return the total number of frames written
	size_type nframes() const { return _nframes; }

protected:

	/// Open a new file for output. Old files are truncated.
	void open(const char *filename, size_type samplerate);


private:
	size_type _nframes;
	SF_INFO _sfinfo;
	boost::shared_ptr<SNDFILE> _sndfile;
	size_type _writef(const sample_type *buf, size_type nframes);

};

template <typename T>
void single_sndfile<T>::open(const char *filename, size_type samplerate)
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
single_sndfile<float>::size_type
single_sndfile<float>::_writef(const float *buf, size_type nframes) {
	return sf_writef_float(_sndfile.get(), buf, nframes);
}

template<> inline
single_sndfile<double>::size_type 
single_sndfile<double>::_writef(const double *buf, size_type nframes) {
	return sf_writef_double(_sndfile.get(), buf, nframes);	
}

template<> inline
single_sndfile<short>::size_type 
single_sndfile<short>::_writef(const short *buf, size_type nframes) {
	return sf_writef_short(_sndfile.get(), buf, nframes);	
}

template<> inline
single_sndfile<int>::size_type 
single_sndfile<int>::_writef(const int *buf, size_type nframes) {
	return sf_writef_int(_sndfile.get(), buf, nframes);	
}


}} // namespace jill::util

#endif
