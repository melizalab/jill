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
 */

#ifndef _SNDFILE_READER_HH
#define _SNDFILE_READER_HH

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <sndfile.h>


namespace jill { namespace util {


/**
 * @ingroup iogroup
 * @brief a class for reading from a sound file
 *
 * A thin wrapper around the libsndfile header for reading sound
 * files. This duplicates to some extent the SndfileHandle class in
 * <sndfile.hh> but with a much simpler interface (and no copy semantics)
 */
class SndfileReader : boost::noncopyable {
public:
	typedef sf_count_t size_type;

	SndfileReader();
	explicit SndfileReader(const std::string &path);
	explicit SndfileReader(const char *path);

	void open(const char *path);

	operator bool () const { return _sndfile; };
	size_type samplerate() const { return (_sndfile) ? _sfinfo.samplerate : 0; }
	size_type frames() const { return (_sndfile) ? _sfinfo.frames : 0; }
	size_type nread() const { return _nread; }

	size_type seek(size_type frames, int whence) {
		return sf_seek(_sndfile.get(), frames, whence);
	}

	template <class T>
	size_type operator()(T *buf, size_type nframes) {
		size_type n = _read(buf, nframes);
		_nread += n;
		return n;
	}

private:
	size_type _read(float *buf, size_type nframes) {
		return sf_readf_float(_sndfile.get(), buf, nframes);
	}
	size_type _read(double *buf, size_type nframes) {
		return sf_readf_double(_sndfile.get(), buf, nframes);
	}
	size_type _read(int *buf, size_type nframes) {
		return sf_readf_int(_sndfile.get(), buf, nframes);
	}
	size_type _read(short *buf, size_type nframes) {
		return sf_readf_short(_sndfile.get(), buf, nframes);
	}

	boost::shared_ptr<SNDFILE> _sndfile;
	SF_INFO _sfinfo;
	size_type _nread;

};


}} // namespace jill::util

#endif
