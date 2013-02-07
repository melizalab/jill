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
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <sys/time.h>
#include <arf.hpp>
#include "../util/string.hh"

using namespace jill::file;
using jill::util::make_string;
using boost::filesystem::path;

/** for finding the next highest numbered entry in a file */
struct name_matcher {

        name_matcher() : value(0) {}

        typedef int return_value;
        return_value value;

	static herr_t iterate(hid_t id, const char * name, const H5L_info_t *link_info, void *data) {
                int idx;
		name_matcher *_this = static_cast<name_matcher*>(data);
                if (sscanf(name, "entry_%d", &idx) > 0) {
                        if (idx > _this->value) _this->value = idx;
                }
		return 0;
	}
};


arffile::arffile(std::string const & basename, std::size_t max_size)
        : _base_filename(basename), _max_size(1048576L * max_size), _file_index(-1)
{
	check_filesize();
        _entry_index = _file->nchildren();
}

arffile::~arffile()
{

}

arf::entry_ptr
arffile::new_entry(nframes_t frame, utime_t usec, nframes_t sampling_rate,
                   std::vector<port_info_t> const * channels,
                   std::map<std::string,std::string> const * attributes,
                   timeval const * timestamp)
{
        using namespace std;

        timeval tp;
	gettimeofday(&tp,0);

        name_matcher nm;
        int idx = _file->iterate(nm);
        boost::format fmt("entry_%|06|");
        fmt % (idx + 1);
        if (_file->contains(fmt.str())) // this is very unlikely
                throw jill::FileError("Error: couldn't find unused entry name");

        // create new entry
        _dsets.clear();
        _entry.reset(new arf::entry(*_file, fmt.str(), (timestamp) ? timestamp : &tp));
        _entry->write_attribute()("jack_frame",frame)("jack_usec",usec);
        if (attributes) {
                std::for_each(attributes->begin(), attributes->end(), _entry->write_attribute());
        }

        // create channels
        if (channels) {
                for (vector<port_info_t>::const_iterator it = channels->begin(); it != channels->end(); ++it) {
                        arf::h5pt::packet_table::ptr_type pt;
                        if (it->storage >= arf::INTERVAL) {
                                 pt = _entry->create_packet_table<arf::interval>(it->name,"s",it->storage);
                        }
                        else if (it->storage >= arf::EVENT) {
                                pt = _entry->create_packet_table<sample_t>(it->name,"s",it->storage);
                        }
                        else {
                                pt = _entry->create_packet_table<sample_t>(it->name,"",it->storage);
                                pt->write_attribute("sampling_rate", sampling_rate);
                        }
                        _dsets.push_back(pt);
                }
        }
	return _entry;
}


bool
arffile::check_filesize()
{
        if (_max_size == 0) {
                if (!_file) {
                        _file.reset(new arf::file(_base_filename, "a"));
                        return true;
                }
                else return false;
	}

        // close file if exceeds max_size
        if (_file && _file->size() > _max_size) {
                log(make_string() << "FFFF " << _current_filename << " size exceeds max; closing");
                _file.reset();
        }

        // open new file if necessary
        if (!_file) {
		_file_index += 1;

		// construct serially numbered file name
		boost::format fmt("%s_%|04|%s");
                path base(_base_filename);
		fmt % base.stem().string() % _file_index % base.extension().string();
		path newfile = base.parent_path() / fmt.str();

                _current_filename = newfile.string();
		_file.reset(new arf::file(_current_filename, "a"));
                return true;
        }
        return false;
}

void
arffile::log(std::string const & msg)
{
        _file->log(msg);
}
