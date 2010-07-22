#ifndef _LOGGER_H
#define _LOGGER_H 1
/**
 * @file   logger.h
 * @author Daniel Meliza <dmeliza@uchicago.edu>
 * @date   Mon Mar  1 13:33:47 2010
 * 
 * @brief  A class for logging events to disk
 * 
 * Copyright C Daniel Meliza, 2010.  Licensed for use under Creative
 * Commons Attribution-Noncommercial-Share Alike 3.0 United States
 * License (http://creativecommons.org/licenses/by-nc-sa/3.0/us/).
 */
#include <cstdio>
#include <iostream>
#include <fstream>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace jill { namespace util {


/**
 * Class for logging data to a stream. The logger can be enabled or
 * disabled, and a convenience function is defined for adding a
 * timestamp to logged data.
 */
class logstream {
public:
	enum _fixed_fields { program, timestamp, allfields };
	/** 
	 * Construct logger
	 * @param s Stream to output log to
	 * @param b True to enable logging (default)
	 * @param p The name of the program doing the logging (when multiple loggers share a file)
	 */
	logstream(std::ostream & s, const char *p="", bool b = true)
		: _stream(s), _enabled(b), _program(p) {}

	/// Enable or disable the logger
	void enable(bool b) { _enabled = b; }

	template <typename T>
	logstream & operator<< (const T & p);

	logstream & operator<< (std::ostream & (*pf)(std::ostream &)) {
		if (_enabled) pf(_stream);
		return *this;
	}
	
private:
	std::ostream & _stream;
	bool _enabled;
	std::string _program;
};

template <typename T> inline
logstream & logstream::operator<< (const T & p) {
	if (_enabled) _stream << p;
	return *this;
}

// specialized template for fixed fields
template<> inline
logstream & logstream::operator<<(const logstream::_fixed_fields &p)
{
	using namespace boost::posix_time;
	if (p == program || p == allfields)
		_stream << '[' << _program << "] ";
	if (p == timestamp || p == allfields) {
		ptime t(microsec_clock::local_time());
		_stream << to_iso_string(t) << ' ';
	}
	return *this;
}

}}
#endif /* _LOGGER_H */

