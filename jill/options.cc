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
#include "jill_options.hh"
#include "offline_options.hh"

using namespace jill;
using std::string;
using std::vector;

Options::Options(const char *program_name, const char *program_version)
	:  _program_name(program_name), _program_version(program_version)
{
	po::options_description generic("General options");
	generic.add_options()
		("version,v", "print version string")
		("help,h",      "print help message");
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



JillOptions::JillOptions(const char *program_name, const char *program_version)
	: Options(program_name, program_version), client_name(program_name)
{
	po::options_description jillopts("JILL options");
	jillopts.add_options()
		("name,n",    po::value<string>()->default_value(_program_name), "set client name")
		("log,l",     po::value<string>(), "set logfile (default stdout)")
		("out,o",     po::value<vector<string> >(), "add output port")
		("in,i",      po::value<vector<string> >(), "add input port");
	cmd_opts.add(jillopts);
	visible_opts.add(jillopts);
}


void
JillOptions::process_options()
{
	std::cout << vmap.size() << std::endl;
	assign(output_ports,"out");
	assign(input_ports,"in");
	assign(client_name,"name");
	assign(logfile,"log");
}


/* ----------------------- OfflineOptions ----------------------- */

OfflineOptions::OfflineOptions(const char *program_name, const char *program_version)
	: Options(program_name, program_version)
{
	po::options_description testopts("Offline module options");
	testopts.add_options()
		("log,l",     po::value<std::string>(), "set logfile (default stdout)")
		("in,i",      po::value<std::string>(), "set input file")
		("out,o",     po::value<std::string>(), "set output file")
		("blocksize,b", po::value<int>()->default_value(1024), 
		 "set number of samples processed in each loop")
		("samplerate,s", po::value<int>()->default_value(20000), 
		 "set sampling rate (if no input file)");
	cmd_opts.add(testopts);
	visible_opts.add(testopts);
}


void
OfflineOptions::process_options()
{
	if (vmap.count("out"))
		output_file = get<std::string>("out");
	if (vmap.count("in"))
		input_file = get<std::string>("in");
	if (vmap.count("log"))
		logfile = get<std::string>("log");
	if (vmap.count("blocksize"))
		blocksize = get<int>("blocksize");
	if (vmap.count("samplerate"))
		samplerate = get<int>("samplerate");
	    
}
