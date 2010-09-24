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
#ifndef _UTIL_STRING_HH
#define _UTIL_STRING_HH

#include <string>
#include <sstream>
#include <boost/filesystem/path.hpp>
#include <boost/filesystem/convenience.hpp>

/**
 * @defgroup miscgroup Miscellaneous classes and functions
 *
 * A number of miscellaneous classes to handle miscellaneous tasks
 * dealing with strings, logging, etc.
 */

namespace jill { namespace util {

/**
 * @ingroup miscgroup
 * @brief a convenient wrapper for stringstream
 *
 * Stringstreams are handy for building strings using operator<<, but
 * the allocation and deallocation is somewhat annoying.  This class
 * eliminates some of the extra steps.
 *
 * string my_string = make_string() << 12 << " + " << ...;
 *
 * Conversion operators to std::string and const char* are provided,
 * allowing this class to be used in argument lists.
 */
class make_string
{
  public:
    template <typename T>
    make_string & operator<< (T const& t) {
	_stream << t;
	return *this;
    }

    make_string & operator<< (std::ostream & (*pf)(std::ostream &)) {
	pf(_stream);
	return *this;
    }

    operator std::string() {
	return _stream.str();
    }

    operator const char*() {
	return _stream.str().c_str();
    }

  private:
    std::ostringstream _stream;
};

/**
 * @ingroup miscgroup
 *
 * Extract the extension of a filename.
 *
 * @param filename    the input filename
 * @return            the extension, minus the period and in lower case
 */
std::string 
get_filename_extension(const std::string & filename)
{
	using namespace boost::filesystem;
	return basename(path(filename));
}

}}

#endif // _UTIL_STRING_HH
