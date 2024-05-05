/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "stimfile.hh"
#include "../logging.hh"

#if MLOCK_STIMFILES
#include <sys/mman.h>
#endif
#include <boost/static_assert.hpp>
#include <boost/filesystem.hpp>
#include <samplerate.h>

namespace fs = boost::filesystem;
using namespace jill::file;

stimfile::stimfile(std::string const & path)
        : _name(fs::path(path).stem().string()), _sndfile(nullptr)
{
        _sndfile = sf_open(path.c_str(), SFM_READ, &_sfinfo);
        if (!_sndfile) throw jill::FileError(sf_strerror(_sndfile));
        if (_sfinfo.channels != 1) {
                throw jill::FileError("input file contains more than one channel");
        }
        _nframes = _sfinfo.frames;
        _samplerate = _sfinfo.samplerate;
}

stimfile::~stimfile()
{
        if (_sndfile) sf_close(_sndfile);
#if MLOCK_STIMFILES
        if (_buffer) munlock(_buffer.get(), _nframes * sizeof(sample_t));
#endif
}

void
stimfile::load_samples(nframes_t samplerate)
{
        // TODO: assert that sample_t is same type as float
        SRC_DATA rs;
        sample_t *buf;

        // check if we actually need to do work
        if (_buffer) {
                if (samplerate == 0 && _samplerate == nframes_t(_sfinfo.samplerate)) return;
                else if (samplerate == _samplerate) return;
        }

        rs.input_frames = _sfinfo.frames;
        rs.data_in = buf = new sample_t[rs.input_frames];

        sf_seek(_sndfile, 0, SEEK_SET);
        // read file, ignoring any discrepancies in # of samples
        _nframes = rs.input_frames = sf_read_float(_sndfile, buf, rs.input_frames);
        _samplerate = _sfinfo.samplerate;
        LOG << "read " << _nframes << " frames from " << _name << " at " << _samplerate << " Hz";

        if ((samplerate > 0) && (samplerate != _samplerate)) {
                rs.src_ratio = float(samplerate) / float(_samplerate);
                rs.output_frames = (int)(rs.input_frames * rs.src_ratio);
                rs.data_out = new sample_t[rs.output_frames];
                LOG << "resampling " << _name << " to " << samplerate
		    << " Hz (" << rs.src_ratio << "x) -> "
                    << rs.output_frames << " frames";

                int ec = src_simple(&rs, SRC_SINC_BEST_QUALITY, 1);
                delete[] rs.data_in;
                if (ec != 0) {
                        delete[] rs.data_out;
                        throw std::runtime_error(src_strerror(ec));
                }

                _nframes = rs.output_frames;
                _samplerate = samplerate;
                buf = rs.data_out;
        }

#if MLOCK_STIMFILES
        mlock(buf, _nframes * sizeof(sample_t));
#endif
        _buffer.reset(buf);

}
