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

#include <iostream>
#include <fstream>
#include "options.hh"

using namespace jill;
using std::string;
using std::vector;

Options::Options(const char *program_name, const char *program_version)
	:  client_name(program_name), _program_name(program_name), _program_version(program_version)
{
	po::options_description generic("General options");
	generic.add_options()
		("version,v", "print version string")
		("help,h",      "print help message")
		("name,n",    po::value<string>()->default_value(_program_name), "set client name")
		("log,l",     po::value<string>(), "set logfile (default stdout)")
		("out,o",     po::value<vector<string> >(), "add output port")
		("in,i",      po::value<vector<string> >(), "add input port");
	cmd_opts.add(generic);
	visible_opts.add(generic);
}

void
Options::print_version()
{
	std::cout << _program_name << " " << _program_version
		  << " (JILL " VERSION ")" << std::endl;
}


void
Options::print_usage()
{
	std::cout << "Usage: " << _program_name << " [options] " << std::endl;
	std::cout << visible_opts;
}


void
Options::parse(int argc, char **argv, const char *configfile)
{
	po::store(po::command_line_parser(argc, argv).options(cmd_opts).positional(pos_opts).run(), vmap);
	if (configfile) {
		std::ifstream ff(configfile);
		if (ff.good())
			po::store(po::parse_config_file(ff, cfg_opts), vmap);
	}
	po::notify(vmap);

	if (vmap.count("help")) {
		print_usage();
		throw Exit(EXIT_SUCCESS);
	}
	else if (vmap.count("version")) {
		print_version();
		throw Exit(EXIT_SUCCESS);
	}
	process_options();
}


void
Options::process_options()
{
	if (vmap.count("out"))
		output_ports = get<vector<string> >("out");
	if (vmap.count("in"))
		input_ports = get<vector<string> >("in");
	if (vmap.count("name"))
		client_name = get<string>("name");
	if (vmap.count("log"))
		logfile = get<string>("log");
}
