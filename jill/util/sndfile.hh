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

#ifndef _SNDFILE_HH
#define _SNDFILE_HH

#include <boost/noncopyable.hpp>
#include <stdexcept>
#include <string>


/**
 * @defgroup iogroup Read and write sound files to disk.
 *
 * JILL uses libsndfile to read and write sound files. A variety of
 * classes provide simple read and write functionality.
 *
 */

namespace jill { namespace util {

/**
 * @ingroup iogroup
 * @brief exception for file IO errors
 */
struct FileError : public std::runtime_error {
	FileError(const std::string & w) : std::runtime_error(w) { }
};


/**
 * @ingroup iogroup
 * @brief ABC for a writable sound file
 *
 * Classes that write sound data to disk derive from this class. They
 * may represent a single file, a single file capable of holding
 * multiple entries, or a collection of files. Implementing classes
 * need to override the following functions:
 *
 * Required: @ref _open, @ref _write, @ref _close
 * Optional: @ref _next, @ref _valid, @ref _current_entry
 *
 * Information about the current entry/file is maintained in a
 * structure with nested type Sndfile::Entry.  Deriving classes can
 * extend this struct to add additional information.
 */
class Sndfile : boost::noncopyable {

public:
	typedef std::size_t size_type;

	/**
	 * Structure holding information about a file/entry
	 */
	struct Entry {
		std::string filename;
	};

	virtual ~Sndfile() {}

	/**
	 * Open a new file or file group for writing.
	 *
	 * @param filename    the name of the file, the template for the group, etc
	 * @param samplerate  the sampling rate of the file
	 */
	void open(const std::string &filename, size_type samplerate) { _open(filename, samplerate); }

	/** Close the currently open file */
	void close() { _close(); }

	/**
	 * Advance the writer to the next, or a specified entry. May
	 * not have an effect for single-entry files.
	 *
	 * @return  information about the new entry
	 */
	const Entry* next(const std::string &entry_name) { return _next(entry_name); }

	/**
	 * @return information about the currently open entry
	 */
	const Entry* current_entry() const { return _current_entry(); }

	/** @return true if the sndfile is valid for writing data */
	operator bool() const { return _valid(); }

	/**
	 * Write samples to the file
	 *
	 * @param buf      buffer containing samples to write
	 * @param samples  the number of samples in the buffer
	 *
	 * @return the number of samples written
	 */
	template <class T>
	size_type operator()(const T *buf, size_type samples) {
		return _write(buf, samples);
	}

private:

	/**
	 * Open a new file and set it up for writing.
	 *
 	 * @param filename    the name of the file, the template for the group, etc
	 * @param samplerate  the sampling rate of the file
	 */
	virtual void _open(const std::string &filename, size_type samplerate) = 0;

	/**
	 * Write a buffer to the file
	 *
	 * @param buf      buffer containing samples to write
	 * @param samples  the number of samples in the buffer
	 */
	virtual size_type _write(const float *buf, size_type nframes) = 0;
	virtual size_type _write(const double *buf, size_type nframes) = 0;
	virtual size_type _write(const int *buf, size_type nframes) = 0;
	virtual size_type _write(const short *buf, size_type nframes) = 0;

	/** Close the currently open file */
	virtual void _close() = 0;

	/** Advance to the next entry */
	virtual const Entry* _next(const std::string &entry_name) { return 0; }

	/**
	 * @return information about the currently open entry
	 */
	virtual const Entry* _current_entry() const { return 0; }

	/** @return true if the sndfile is valid for writing data */
	virtual bool _valid() const { return true; }

};

}} // namespace jill::util

#endif
