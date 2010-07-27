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
 */
#ifndef _BUFFERED_SNDFILE_HH
#define _BUFFERED_SNDFILE_HH

#include <boost/noncopyable.hpp>
#include "util/sndfile.hh"
#include "util/ringbuffer.hh"

namespace jill {

/**
 * This class writes data to a sound file buffered through a
 * ringbuffer. Clients write to the file through the writef()
 * function, and data is flushed to the file through the operator()
 * [to support passing the object as a functor]
 * 
 * @param T The data type handled by the file
 */
template <typename T>
class BufferedSndfile : public util::Sndfile<T> {

public:
	BufferedSndfile(size_t buffer_size=100000) 
		: util::Sndfile<T>(), _ringbuffer(buffer_size) {}

	/// store data in the ringbuffer. returns the number of samples stored
	sf_count_t writef(const T *buf, size_t nframes);
	/// write data from the ringbuffer to disk. Returns 0 if successful, -1 otherwise
	int operator() ();

private:
	util::Ringbuffer<T> _ringbuffer;
};

template<typename T> inline
sf_count_t BufferedSndfile<T>::writef(const T *buf, size_t nframes)
{
	return _ringbuffer.write(buf, nframes);
}


template<typename T> inline
int BufferedSndfile<T>::operator()()
{
	T *buf;
	sf_count_t cnt, frames = _ringbuffer.peek(&buf);	
	if (frames) {
		cnt = util::Sndfile<T>::writef(buf, frames);
		_ringbuffer.advance(frames);
		return (cnt == frames) ? 0 : -1;
	}
	return 0;
}

} // namespace jill
#endif // _BUFFERED_SNDFILE_HH
