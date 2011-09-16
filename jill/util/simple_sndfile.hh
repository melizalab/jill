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
 */

#ifndef _SIMPLE_SNDFILE_HH
#define _SIMPLE_SNDFILE_HH

#include "sndfile.hh"
#include <boost/shared_ptr.hpp>
#include <sndfile.h>


namespace jill { namespace util {

/**
 * @ingroup iogroup
 * @brief writes data to a single file using libsndfile
 *
 * This class implements Sndfile using the libsndfile library. It is
 * pretty darn simple.
 */
class SimpleSndfile : public Sndfile {

public:

	SimpleSndfile();
	SimpleSndfile(const std::string &filename, size_type samplerate);

protected:
	virtual void _open(std::string const &filename, size_type samplerate);
	virtual void _close();

	virtual bool _valid() const;

	virtual size_type _write(const float *buf, size_type nframes);
	virtual size_type _write(const double *buf, size_type nframes);
	virtual size_type _write(const int *buf, size_type nframes);
	virtual size_type _write(const short *buf, size_type nframes);

private:
	virtual const Entry* _next(std::string const &entry_name) { return &_entry; }
	virtual const Entry* _current_entry() const { return &_entry; }

	Entry _entry;
	SF_INFO _sfinfo;
	boost::shared_ptr<SNDFILE> _sndfile;

};


}} // namespace jill::util

#endif
