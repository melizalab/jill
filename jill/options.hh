/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _OPTIONS_HH
#define _OPTIONS_HH

#include <string>
#include <vector>
#include <stdexcept>

#include "util/string.hh"

namespace jill {

/** 
 * The Options class stores options for JILL applications and handles
 * parsing the command line.  This base class processes options common
 * to all JILL applications:
 *
 * -n : client name
 * -l : log file
 * -i : input ports
 * -o : output ports
 *
 * Applications requiring additional options can derive from this class and then
 * call the base method, if desired.
 */
class Options {
public:
	Options(const char *program_name, const char *program_version);

	/// Parse the command line arguments and store options
	void parse(int argc, char **argv);

	/// The client name (used in internal JACK representations)
	std::string client_name;
	/// A vector of inputs to connect to the client
	std::vector<std::string> input_ports;
        /// A vector of outputs to connect to the client
	std::vector<std::string> output_ports;
	/// The log file to write application events to
	std::string logfile;

private:
	struct CmdlineError : public std::runtime_error
	{
		CmdlineError(std::string const & w) : std::runtime_error(w) { }
	};

	struct InvalidArgument : public std::runtime_error
	{
		InvalidArgument(char a, std::string const & w)
			: std::runtime_error(util::make_string() << "invalid argument to -" 
					     << a << " (" << w << ")") {}
	};

	void print_version();
	void print_usage();
	std::string _program_name;
	std::string _program_version;
};

} // namespace jill

#endif // _OPTIONS_HH
