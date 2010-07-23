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
#ifndef _STRING_HH
#define _STRING_HH

#include <string>
#include <sstream>

namespace jill { namespace util {

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

  private:
    std::ostringstream _stream;
};

std::string get_filename_extension(const std::string & filename)
{
    std::string::size_type period = filename.find_last_of('.');
    if (period == std::string::npos) {
        return "";
    }

    std::string ext(filename, period + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext;
}

}}

#endif // _STRING_HH
