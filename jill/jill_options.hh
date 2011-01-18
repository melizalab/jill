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
#ifndef _JILL_OPTIONS_HH
#define _JILL_OPTIONS_HH

#include "options.hh"
#include <vector>

namespace jill {

namespace po = boost::program_options;

/**
 * @ingroup optionsgroup
 * @brief implement Options with basic JILL options
 *
 * The JillOptions class stores options for JILL applications. Options
 * handled are:
 *
 * -n : client name
 * -l : log file
 * -i : input ports
 * -o : output ports
 * -c : control ports  [not included by default]
 *
 */
class JillOptions : public Options {
public:
	JillOptions(const char *program_name, const char *program_version, bool supports_control=false);
	virtual ~JillOptions() {}

	/** The client name (used in internal JACK representations) */
	std::string client_name;
	/** A vector of inputs to connect to the client */
	std::vector<std::string> input_ports;
	/** A vector of outputs to connect to the client */
	std::vector<std::string> output_ports;
	/** A vector of ports to connect to the client's control port */
	std::vector<std::string> control_ports;
	/** The log file to write application events to */
	std::string logfile;

protected:
	virtual void process_options();

};

// implementations in options.cc

} // namespace jill

#endif // _JILL_OPTIONS_HH
