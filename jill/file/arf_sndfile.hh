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

#ifndef _ARFFILE_HH
#define _ARFFILE_HH

#include <boost/noncopyable.hpp>
#include <boost/filesystem.hpp>
#include <arf.hpp>
#include "../types.hh"

namespace jill { namespace file {

/**
 * @ingroup iogroup
 * @brief write data to an ARF (hdf5) file
 *
 * Uses ARF as a container for data. Each entry is an HDF5 group in the ARF
 * file. Supports splitting across multiple files (in a somewhat hacky way).
 */
class arffile : boost::noncopyable {

public:
	typedef jack_default_audio_sample_t storage_type;

	/**
	 * Open a new or existing ARF file for writing.  Note: object
	 * is not valid until next() is called to create a new entry.
	 *
	 * @param basename   the name of the file (or basename if max_size > 0)
	 * @param max_size   the maximum size the file can get, in
	 *                   MB. If zero or less, file is allowed to grow
	 *                   indefinitely. If nonnegative, files will be indexed, and
	 *                   when the file size exceeds this (checked after
	 *                   each entry is closed), a new file will be
	 *                   created.
	 */
	arffile(boost::filesystem::path const & basename, std::size_t max_size=0);
        ~arffile();

        arf::file::ptr_type file() { return _file; }
        arf::entry::ptr_type entry() { return _entry; }

        /**
         * @brief close the current entry and open a new one
         *
         * @param entry_name the name of the new entry
         * @param timestamp  the timestamp of the entry
         * @returns pointer to entry
         */
        arf::entry::ptr_type new_entry(std::string const & entry_name,
                                       std::vector<boost::int64_t> const & timestamp);

protected:
        /**
         * @brief check filesize prior to opening a new entry
         *
         * Flushes data to disk; opens new sequentially numbered file if the
         * current file exceeds max size
         */
	void check_filesize();

private:
	arf::file::ptr_type _file;
        arf::entry::ptr_type _entry;
	boost::filesystem::path _base_filename;
        std::size_t _max_size;
	int _file_index;
};

}} // namespace jill::file

#endif
