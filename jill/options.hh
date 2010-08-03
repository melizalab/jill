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
#ifndef _OPTIONS_HH
#define _OPTIONS_HH

#include <string>
#include <vector>
#include <stdexcept>
#include <boost/program_options.hpp>


namespace jill {

namespace po = boost::program_options;

/**
 * The Options class is a base class for parsing options. It handles
 * the most basic options common to all programs.
 *
 * This class is a thin wrapper around boost/program_options. The
 * options_descriptions members and parsed value map are exposed to
 * allow additional options without a need for subclassing.  For
 * example, to add a single option:
 *
 * options.cmd_opts.add_options()("user_name", po:value<string>(), "the user's name");
 * options.parse();
 * string uname = options.get("blah","");
 *
 * Alternatively, a class can inherit from this class and override the
 * constructor and process_options() functions (possibly the parse()
 * function as well)
 */
class Options {
public:
	/** All options classes are initialized with the program name and version */
	Options(const char *program_name, const char *program_version);
	virtual ~Options() {}

	/// Description of the options for commandline usage
	po::options_description cmd_opts;
	/// Description of options visible in the help (a subset of cmd_opts)
	po::options_description visible_opts;
	/// Description of options parsed from a config file
	po::options_description cfg_opts;
	/// Options which are processed positionally
	po::positional_options_description pos_opts;
	/// Values for options are stored here after parsing
	po::variables_map vmap;

	/**
	 * Parse the command line arguments and store options.
	 * @param argc Number of arguments
	 * @param argv Array of command-line arguments
	 * @param config Optional config file name
	 */
	virtual void parse(int argc, char **argv, const char *configfile=0);

	/**
	 * Retrieve the value of an option from storage. It's
	 * recommended to explicitly specify the type, e.g. get<float>("blah")
	 *
	 * @param name  The name of the option
	 */
	template <typename T> inline
	const T &get(const char *name) { return vmap[name].as<T>();}

	/**
	 * Assign a parsed value to a variable, if the value is defined.
	 *
	 * @param ref  Reference to variable to recieve new value
	 * @param name The name of the option
	 * @returns true if the value was defined
	 * @throws boost::bad_any_cast if the value could not be case appropriately
	 */
	template <typename T> inline
	bool assign(T &ref, const char *name) {
		if (vmap.count(name)) {
			ref = vmap[name].as<T>();//boost::any_cast<T>(vmap[name]);
			return true;
		}
		return false;
	}

protected:
	std::string _program_name;
	std::string _program_version;

	/// This function is called once the options are parsed; its job is to load data into the fields
	virtual void process_options() = 0;
	/// Print the name and version of the program. Called by parse()
	virtual void print_version();
	/// Print the usage information
	virtual void print_usage();

};

/**
 * The JillOptions class stores options for JILL applications. Options
 * handled are:
 *
 * -n : client name
 * -l : log file
 * -i : input ports
 * -o : output ports
 *
 */
class JillOptions : public Options {
public:
	JillOptions(const char *program_name, const char *program_version);
	virtual ~JillOptions() {}

	/// The client name (used in internal JACK representations)
	std::string client_name;
	/// A vector of inputs to connect to the client
	std::vector<std::string> input_ports;
	/// A vector of outputs to connect to the client
	std::vector<std::string> output_ports;
	/// The log file to write application events to
	std::string logfile;

protected:
	virtual void process_options();
	
};

/**
 * A class of exceptions intended to trigger program termination with
 * a specific status value.
 */
class Exit : public std::exception {
public:
	Exit(int status) : _status(status) { }
	virtual ~Exit() throw () { }

	int status() const throw() { return _status; }

protected:
	int _status;
};


} // namespace jill

#endif // _OPTIONS_HH
