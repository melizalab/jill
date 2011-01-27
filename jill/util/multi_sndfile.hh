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

#ifndef _MULTI_SNDFILE_HH
#define _MULTI_SNDFILE_HH

#include "simple_sndfile.hh"
#include <sys/time.h>

namespace jill { namespace util {


/**
 * @ingroup iogroup
 * @brief write data to a group of soundfiles using libsndfile
 *
 * This class is an extension of the sndfile to support splitting data
 * across multiple files. The usage is quite similar.  Instead of
 * supplying a file name, the user supplies a file template, which is
 * used to generate new file names whenever the user calls next().
 * Whenever the filename is changed, the user needs to call next()
 * before the object will be valid.
 */
class MultiSndfile : public SimpleSndfile {

public:
	struct Entry : public SimpleSndfile::Entry {
		Entry() : file_idx(0) {}
		int file_idx;
		struct timeval time[2];
	};

	/**
	 * Instantiate a sndfile family. The filename argument is
	 * replaced by a template.  Note: object is not valid until
	 * next() is called.
	 *
	 * @param sprintf-style template with one numeric argument, e.g. "myfile_%04d.wav"
	 */
	MultiSndfile(const char *templ, size_type samplerate) { _open(templ, samplerate); }
	MultiSndfile(const std::string &templ, size_type samplerate)  { _open(templ.c_str(), samplerate); }

	/** @return the currently open file */
	const std::string &current_file() const { return _entry.filename; }

	/** @return the current entry time */
	void get_entry_time(struct timeval* ptv) const { 
		ptv[0] = _entry.time[0];
		ptv[1] = _entry.time[1];
	};

protected:

	virtual void _open(const char *templ, size_type samplerate);
	virtual void _close();

private:

	virtual const Entry* _next(const char *);

	Entry _entry;
	std::string _fn_templ;
	size_type _samplerate;
};

}} // namespace jill::util

#endif
