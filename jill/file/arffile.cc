/* @file arf_sndfile.cc
 *
 * Copyright (C) 2011 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "arffile.hh"
#include <arf.hpp>
#include <boost/format.hpp>
#include "../util/string.hh"

using namespace jill::file;
using boost::filesystem::path;

arffile::arffile(path const & basename, std::size_t max_size)
        : _base_filename(basename), _max_size(1048576L * max_size), _file_index(-1)
{
	check_filesize();
}

arffile::~arffile()
{}

arf::entry::ptr_type
arffile::new_entry(std::string const & entry_name, timeval const * timestamp)
{
        if (entry_name.empty())
                throw jill::FileError("Entry name cannot be empty");
        // checks name against current file to avoid duplicating names even
        // across files. Doesn't check against all entries, though.
        if (_file->contains(entry_name))
                throw jill::FileError("An entry by that name already exists");
        // create new entry
        _entry.reset(new arf::entry(*_file, entry_name, timestamp));
	return _entry;
}


bool
arffile::check_filesize()
{
        if (_max_size == 0) {
                if (!_file) {
                        _file.reset(new arf::file(_base_filename.string(), "a"));
                        _file->log(jill::util::make_string() << "FFFF " << _file->name() << " opened for writing");
                        return true;
                }
                else return false;
	}

        // close file if exceeds max_size
        if (_file && _file->size() > _max_size) {
                _file->log(jill::util::make_string() << "FFFF " << _file->name() << " size exceeds max; closing");
                _file.reset();
        }

        // open new file if necessary
        if (!_file) {
		_file_index += 1;
		// construct serially numbered file name
		boost::format fmt("%s_%|04|%s");
		fmt % _base_filename.stem().string() % _file_index % _base_filename.extension().string();
		path newfile = _base_filename.parent_path() / fmt.str();
		_file.reset(new arf::file(newfile.string(), "a"));
                _file->log(jill::util::make_string() << "FFFF " << _file->name() << " opened for writing");
                return true;
        }
        return false;
}
