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
 * This header contains functions and classes used by main translation units.
 */
#ifndef _MAIN_HH
#define _MAIN_HH
#include <exception>

namespace jill {

class Exit : public std::exception {
public:
	Exit(int status) : _status(status) { }
	virtual ~Exit() throw () { }

	int status() const throw() { return _status; }

protected:
	int _status;
};

}
#endif // _MAIN_HH
