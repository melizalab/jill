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
#include <vector>
#include <map>
#include "../types.hh"

namespace jill { namespace file {

/**
 * @ingroup iogroup
 * @brief write data to an ARF (hdf5) file
 *
 * Uses ARF as a container for data. Each entry is an HDF5 group in the ARF
 * file. Supports splitting across multiple files.
 */
class arffile : boost::noncopyable {

public:
	typedef jack_default_audio_sample_t storage_type;

	/**
	 * Open a new or existing ARF file for writing.
	 *
	 * @param basename   the name of the file (or basename if max_size > 0)
	 * @param max_size   the maximum size the file can get, in
	 *                   MB. If zero or less, file is allowed to grow
	 *                   indefinitely. If nonnegative, files will be indexed, and
	 *                   when the file size exceeds this (checked after
	 *                   each entry is closed), a new file will be
	 *                   created.
	 */
	arffile(std::string const & basename, std::size_t max_size=0);
        ~arffile();

        /**
         * Open a new entry (closing current one if necessary). Sets required
         * attributes and creates new datasets for each of the channels.
         *
         */
        arf::entry_ptr new_entry(nframes_t frame, utime_t usec, nframes_t sampling_rate,
                                 std::vector<port_info_t> const * channels=0,
                                 std::map<std::string,std::string> const * attributes=0,
                                 timeval const * timestamp=0);

        /**
         * Append data to a channel's dataset
         */

        /**
         * @brief compare file size against max size and opens new file if needed
         *
         * @note the current file size as reported by the API may not reflect
         * recently written data, so call file()->flush() if precise values are
         * needed.
         *
         * @returns true iff a new file was opened
         */
	bool check_filesize();

        /** return name of current file */
        std::string const & name() const {
                return _current_filename;
        }

        /** log message to current file */
        void log(std::string const & msg);

private:
        std::string _base_filename;
        hsize_t _max_size;      // in bytes
	int _file_index;
        int _entry_index;
        std::string _current_filename;

	arf::file_ptr _file;
        arf::entry_ptr _entry;
        std::vector<arf::packet_table_ptr> _dsets;
};

}} // namespace jill::file

#endif
