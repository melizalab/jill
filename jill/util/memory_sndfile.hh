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

#ifndef _MEMORY_SNDFILE_HH
#define _MEMORY_SNDFILE_HH

#include "sndfile.hh"
#include <vector>
#include <algorithm>

namespace jill { namespace util {


/**
 * @ingroup iogroup
 * @brief dummy sndfile that just stores data to memory
 *
 * This class writes its data to memory. Obviously this isn't good for
 * extremely large files, but it can come in handy if the data just
 * need to be examined online.
 *
 */
class MemorySndfile : public Sndfile {

public:
	typedef float data_type;
	
	/**
	 * Structure holding information about a file/entry
	 */
	struct Entry : public Sndfile::Entry {
		std::vector<data_type> data;
		size_type framerate;
	};

	MemorySndfile();
	virtual ~MemorySndfile();

	std::vector<data_type> & data() { return _entry.data; }

protected:
	virtual void _open(const char *filename, size_type samplerate);
	virtual void _close();

	virtual size_type _write(const float *buf, size_type nframes) {
		return insert_data(buf, nframes);
	}
	virtual size_type _write(const double *buf, size_type nframes) {
		return insert_data(buf, nframes);
	}
	virtual size_type _write(const int *buf, size_type nframes) {
		return insert_data(buf, nframes);
	}
	virtual size_type _write(const short *buf, size_type nframes) {
		return insert_data(buf, nframes);
	}

private:
	virtual const Entry* _next(const char *entry_name);
	virtual const Entry* _current_entry() const { return &_entry; }

	template <class T>
	size_type insert_data(const T *buf, size_type nframes) {
		std::back_insert_iterator<std::vector<data_type> > ii(_entry.data);
		std::copy(buf, buf+nframes, ii);
		return nframes;
	}

	Entry _entry;
};


}}


#endif 

