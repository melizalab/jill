/* @file arf_sndfile.cc
 *
 * Copyright (C) 2011 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "string.hh"
#include "arf_sndfile.hh"
#include <arf.hpp>
#include <boost/format.hpp>

using namespace jill::util;

void
ArfSndfile::Entry::set_attribute(std::string const & name, std::string const & value)
{
	if (entry) entry->write_attribute(name, value);
}

void
ArfSndfile::Entry::set_attribute(std::string const & name, boost::int64_t value)
{
	if (entry) entry->write_attribute(name, value);
}

void 
ArfSndfile::Entry::set_attribute(std::string const & name, 
				 std::vector<boost::int64_t> & value) 
{
	if (entry) entry->write_attribute(name, value);
}

void
ArfSndfile::Entry::set_attribute(std::string const & name, float value)
{
	if (entry) entry->write_attribute(name, value);
}

std::string
ArfSndfile::Entry::name() const
{
	if (entry)
		return make_string() << entry->file()->name() << entry->name();
	else
		return "";
}

Sndfile::size_type
ArfSndfile::Entry::nframes() const
{
	return dataset->dataspace()->size();
}

void
ArfSndfile::_open(path const & filename, size_type samplerate)
{
	_base_filename = filename;
	_samplerate = samplerate;
	check_filesize();
}

void
ArfSndfile::_close()
{
	_file.reset(0);
}

bool
ArfSndfile::_valid() const
{
	return (_file && _entry.entry);
}

ArfSndfile::Entry *
ArfSndfile::_next(const std::string &entry_name)
{
	if (!_file)
		throw FileError("File is not open");
	_file->flush();
	check_filesize();  // this may close the current file
	// generate name if needed
	if (entry_name.empty()) {
		boost::format fmt("entry_%|04|");
		fmt % _file->nchildren();
		_entry.entry = _file->open_entry(fmt.str());
	}
	else
		_entry.entry = _file->open_entry(entry_name);
	// create the packet table
	_entry.dataset = _entry.entry->create_packet_table<storage_type>("pcm","",arf::ACOUSTIC,true);
	_entry.dataset->write_attribute("sampling_rate",_samplerate);
	// flush previous entry
	return &_entry;
}

ArfSndfile::Entry *
ArfSndfile::_current_entry()
{
	return &_entry;
}

Sndfile::size_type
ArfSndfile::_write(const float *buf, size_type nframes)
{
	if (!_valid())
		throw FileError("No entry selected");
	try {
		_entry.dataset->write(buf, nframes);
		return nframes;
	}
	catch (arf::Exception &e) {
		throw FileError(e.what());
	}

}

Sndfile::size_type
ArfSndfile::_write(const double *buf, size_type nframes)
{
	throw FileError("Conversion from double not supported");
}

Sndfile::size_type
ArfSndfile::_write(const int *buf, size_type nframes)
{
	throw FileError("Conversion from int not supported");
}

Sndfile::size_type
ArfSndfile::_write(const short *buf, size_type nframes)
{
	throw FileError("Conversion from short not supported");
}


void
ArfSndfile::check_filesize()
{
	if (_max_size <= 0) {
		// no splitting; just open file if necessary
		if (!_file) {
			_file.reset(new arf::file(_base_filename.string(), "a"));
		}
	}
	else if (!_file || _file->size() > (_max_size * 1048576)) {
		// if no file, or file is full, advance index
		_file_index += 1;
		// construct serially numbered file name
		boost::format fmt("%s_%|04|%s");
		fmt % _base_filename.stem().string() % _file_index % _base_filename.extension().string();
		path newfile = _base_filename.parent_path() / fmt.str();
		_file.reset(new arf::file(newfile.string(), "a"));
	}
}
