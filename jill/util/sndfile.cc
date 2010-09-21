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
#include "multisndfile.hh"
#include "string.hh"
using namespace jill::util;

sndfile::sndfile() : _nframes(0) {}

sndfile::sndfile(const char *filename, size_type samplerate)
	: _nframes(0)
{
	open(filename, samplerate);
}

sndfile::sndfile(const std::string &filename, size_type samplerate)
	: _nframes(0)
{
	open(filename.c_str(), samplerate);
}

void
sndfile::open(const char *filename, size_type samplerate)
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

void
sndfile::close()
{
	if(_sndfile) sf_close(_sndfile.get());
	_sndfile.reset();
}

sndfile::operator bool () const
{
	return _sndfile;
}


sndfile::size_type
sndfile::write(const float *buf, size_type nframes)
{
	size_type n = sf_writef_float(_sndfile.get(), buf, nframes);
	_nframes += n;
	return n;
}


sndfile::size_type
sndfile::write(const double *buf, size_type nframes) {
	size_type n = sf_writef_double(_sndfile.get(), buf, nframes);
	_nframes += n;
	return n;
}


sndfile::size_type
sndfile::write(const short *buf, size_type nframes) {
	size_type n = sf_writef_short(_sndfile.get(), buf, nframes);
	_nframes += n;
	return n;
}


sndfile::size_type
sndfile::write(const int *buf, size_type nframes) {
	size_type n = sf_writef_int(_sndfile.get(), buf, nframes);
	_nframes += n;
	return n;
}


/* ------------------------- multisndfile --------------------- */


const std::string&
multisndfile::next()  {
	if (_fn_templ=="") return _fn_templ;
	gettimeofday(_entry_time,0);
	gettimeofday(_entry_time+1,0);
	try {
		_current_file = (boost::format(_fn_templ) % ++_file_idx). str();
	}
	catch (const boost::io::format_error &e) {
		throw FileError(make_string() << "invalid filename template: " << _fn_templ);
	}
	open(_current_file.c_str(), _samplerate);
	return _current_file;
}

void
multisndfile::close()
{
	sndfile::close();
	utimes(_current_file.c_str(), _entry_time);
}

/* ------------------------- sndfilereader --------------------- */

sndfilereader::sndfilereader() {}

sndfilereader::sndfilereader(const char *filename) { open(filename); }

sndfilereader::sndfilereader(const std::string &filename) { open(filename.c_str()); }


void
sndfilereader::open(const char *filename)
{
	SNDFILE *f = sf_open(filename, SFM_READ, &_sfinfo);
	if (!f) {
		throw FileError(make_string() << "couldn't open '" << filename << "' for input");
	}
	_sndfile.reset(f, sf_close);
	_nread = 0;
}
