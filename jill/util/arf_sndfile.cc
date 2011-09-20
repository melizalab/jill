/* @file arf_sndfile.cc
 *
 * Copyright (C) 2011 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

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
ArfSndfile::Entry::set_attribute(std::string const & name, float value)
{
	if (entry) entry->write_attribute(name, value);
}

void
ArfSndfile::_open(const std::string &filename, size_type samplerate)
{
	_file.reset(new arf::file(filename, "a"));
	_samplerate = samplerate;
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
	_entry.nframes = 0;
	// flush previous entry
	_file->flush();
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
		_entry.nframes += nframes;
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
