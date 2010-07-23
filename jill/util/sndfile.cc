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

#include "sndfile.hh"
#include "string.hh"
#include <cstring> // for memset
#include <algorithm>

using namespace jill::util;

SndfileWriter::SndfileWriter(const std::string &filename, size_t samplerate)
	: _samplerate(samplerate)
{
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
	} else if (ext == "raw") {
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

void 
SndfileWriter::writef(const float *buf, size_t nframes) {
	sf_writef_float(_sndfile.get(), buf, nframes);	
}

void 
SndfileWriter::writef(const double *buf, size_t nframes) {
	sf_writef_double(_sndfile.get(), buf, nframes);	
}

void 
SndfileWriter::writef(const short *buf, size_t nframes) {
	sf_writef_short(_sndfile.get(), buf, nframes);
}

void 
SndfileWriter::writef(const int *buf, size_t nframes) {
	sf_writef_int(_sndfile.get(), buf, nframes);
}

