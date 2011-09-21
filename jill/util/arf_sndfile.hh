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
		void set_attribute(std::string const & name, std::string const & value);
		void set_attribute(std::string const & name, boost::int64_t value);
		void set_attribute(std::string const & name, float value);

		/** Return name of entry */
		std::string name() const;
		/** Return size of entry, in frames */
		size_type nframes() const;

		arf::entry::ptr_type entry;
		arf::h5pt::packet_table<storage_type>::ptr_type dataset;
	};

	/**
	 * Open a new or existing ARF file for writing.  Note: object
	 * is not valid until next() is called to create a new entry.
	 *
	 * @param filename   the name of the file
	 * @param samplerate the default sampling rate of the data
	 * @param max_size   the maximum size the file can get, in
	 *                   MB. If zero or less, file is allowed to grow
	 *                   indefinitely. If nonnegative, files will be indexed, and
	 *                   when the file size exceeds this (checked after
	 *                   each entry is closed), a new file will be
	 *                   created.
	 */
	ArfSndfile(path const & filename, size_type samplerate, int max_size=-1)
		: _max_size(max_size), _file_index(-1) {
		_open(filename, samplerate);
	}

protected:

	virtual void _open(path const & filename, size_type samplerate);
	virtual void _close();
	virtual bool _valid() const;
	virtual size_type _write(const float *buf, size_type nframes);

	// these methods throw errors
	virtual size_type _write(const double *buf, size_type nframes);
	virtual size_type _write(const int *buf, size_type nframes);
	virtual size_type _write(const short *buf, size_type nframes);
private:

	virtual Entry* _next(const std::string &);
	virtual Entry* _current_entry();

	/** Handles creation of serially numbered files */
	void check_filesize();

	Entry _entry;
	boost::scoped_ptr<arf::file> _file;
	size_type _samplerate;
	int _max_size;
	path _base_filename;
	int  _file_index;
};

}} // namespace jill::util

#endif
