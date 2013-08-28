/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _NULL_WRITER_HH
#define _NULL_WRITER_HH

#include "../logging.hh"
#include "../data_writer.hh"

namespace jill { namespace file {

/**
 * A no-op implementation of jill::data_writer. This class prints useful log
 * messages but doesn't write any data. It's used primarily for testing.
 */
class null_writer : public data_writer {

public:
        null_writer() : _entry(0), _last_entry(0) {}
        void new_entry(nframes_t frame) {
                _entry = ++_last_entry;
                LOG << "new entry " << _entry << ", frame=" << frame;
        }
        void close_entry() {
                LOG << "closed entry " <<  _entry;
                _entry = 0;
        }
        void xrun() {
                LOG << "got xrun";
        }
        bool ready() const { return _entry; }
        bool aligned() const { return true; }
        void write(data_block_t const * data, nframes_t start, nframes_t stop) {
                if (!_entry) new_entry(data->time);
                std::cout << "\rgot period: time=" << data->time << ", id=" << data->id()
                          << ", type=" << data->dtype << ", nframes=" << data->nframes()
                          << ", start=" << start << ", stop=" << stop << ' ' << std::flush;
        }

private:
        int _entry;
        int _last_entry;
};

}}

#endif
