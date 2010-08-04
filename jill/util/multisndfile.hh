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

#ifndef _MULTISNDFILE_HH
#define _MULTISNDFILE_HH

#include "sndfile.hh"

#include <boost/format.hpp>
#include <sys/time.h>

namespace jill { namespace util {


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

	/** Override close fxn to set timestamp of output file */
	virtual void close();

	/**
	 * Close the current file and open a new one.
	 *
	 * @returns   The name of the new file
	 */
	virtual const std::string &next();

	/// Return the currently open file
	virtual const std::string &current_file() const { return _current_file; }
		

private:
	std::string _fn_templ;
	std::string _current_file;
	size_type _samplerate;
	int _file_idx;
	struct timeval _entry_time[2];
};

}} // namespace jill::util

#endif
