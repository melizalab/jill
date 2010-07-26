/**
 * @file   logger.hh
 * @author Daniel Meliza <dmeliza@uchicago.edu>
 * @date   Mon Mar  1 13:33:47 2010
 *
 * @brief  A class for logging events to disk
 *
 * Copyright C Daniel Meliza, 2010.  Licensed for use under Creative
 * Commons Attribution-Noncommercial-Share Alike 3.0 United States
 * License (http://creativecommons.org/licenses/by-nc-sa/3.0/us/).
 */
#ifndef _LOGGER_HH
#define _LOGGER_HH 1
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
	 * @param p The name of the program doing the logging (when multiple loggers share a file)
	 */
	logstream(std::ostream * s=0)
		: _stream(s) {}

	/// Set or switch streams
	void set_stream(std::ostream * s) { _stream = s; }
	/// Open a file and use it as an output stream, or if file is empty, connect to std::out
	void set_stream(const std::string &file) {
		if (!file.empty()) {
			_fstream.open(file.c_str());
			_stream = &_fstream;
		}
		else
			_stream = &std::cout;
	}
	void set_program(const std::string &p) {
		_program = p;
	}

	template <typename T>
	logstream & operator<< (const T & p);

	logstream & operator<< (std::ostream & (*pf)(std::ostream &)) {
		if (_stream) pf(*_stream);
		return *this;
	}

private:
	std::ofstream _fstream; // only used if we need to open our own file
	std::ostream * _stream; // does not own the resource
	std::string _program;
};

template <typename T> inline
logstream & logstream::operator<< (const T & p) {
	if (_stream) *_stream << p;
	return *this;
}

// specialized template for fixed fields
template<> inline
logstream & logstream::operator<<(const logstream::_fixed_fields &p)
{
	using namespace boost::posix_time;
	if (p == program || p == allfields)
		*this << '[' << _program << "] ";
	if (p == timestamp || p == allfields) {
		ptime t(microsec_clock::local_time());
		*this << to_iso_string(t) << ' ';
	}
	return *this;
}

}}
#endif /* _LOGGER_H */
