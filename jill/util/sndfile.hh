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
#include <sndfile.hh>

namespace jill { namespace util {

class SndfileWriter : boost::noncopyable {

public:
	struct FileError : public std::runtime_error {
		FileError(const std::string & w) : std::runtime_error(w) { }
	};
	SndfileWriter(const std::string &filename, size_t samplerate);

	void writef(const float *buf, size_t nframes);
	void writef(const double *buf, size_t nframes);
	void writef(const short *buf, size_t nframes);
	void writef(const int *buf, size_t nframes);

private:
	size_t _samplerate;
	boost::shared_ptr<SNDFILE> _sndfile;

};

}} // namespace jill::util

#endif
