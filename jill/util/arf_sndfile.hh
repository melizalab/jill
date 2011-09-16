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
 *! @brief Classes for writing data to ARF files
 *!
 */

#ifndef _ARF_SNDFILE_HH
#define _ARF_SNDFILE_HH

#include "sndfile.hh"
#include <arf.hpp>
#include <sys/time.h>
#include <boost/scoped_ptr.hpp>
#include <jack/types.h>

namespace jill { namespace util {


/**
 * @ingroup iogroup
 * @brief write data to an ARF (hdf5) file
 *
 * Uses ARF as a container for data. Each entry is an HDF5 group in
 * the ARF file.  Currently only a single channel is supported.
 */
class ArfSndfile : public Sndfile {

public:
	typedef jack_default_audio_sample_t storage_type;

	struct Entry : public Sndfile::Entry {
		arf::entry::ptr_type entry;
		arf::h5pt::packet_table<storage_type>::ptr_type dataset;
	};

	/**
	 * Open a new or existing ARF file for writing.
	 * replaced by a template.  Note: object is not valid until
	 * next() is called.
	 *
	 * @param filename   the name of the file
	 * @param samplerate the default sampling rate of the data
	 */
	ArfSndfile(const std::string &filename, size_type samplerate) { _open(filename, samplerate); }

protected:

	virtual void _open(const std::string &filename, size_type samplerate);
	virtual void _close();
	virtual bool _valid() const;
	virtual size_type _write(const float *buf, size_type nframes);

	// these methods throw errors
	virtual size_type _write(const double *buf, size_type nframes);
	virtual size_type _write(const int *buf, size_type nframes);
	virtual size_type _write(const short *buf, size_type nframes);
private:

	virtual const Entry* _next(const std::string &);
	virtual const Entry* _current_entry() const;

	Entry _entry;
	boost::scoped_ptr<arf::file> _file;
	size_type _samplerate;
};

}} // namespace jill::util

#endif
