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
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#include "jill_options.hh"
#include "version.hh"

using namespace jill;
using std::string;
using std::vector;

Options::Options(const char *program_name, const char *program_version)
	:  _program_name(program_name), _program_version(program_version)
{
	po::options_description generic("General options");
	generic.add_options()
		("version,v", "print version string")
		("help,h",    "print help message")
		("config,C",  po::value<string>(), "load options from a ini file (overruled by command-line)");
	cmd_opts.add(generic);
	visible_opts.add(generic);
}

void
Options::print_version()
{
	std::cout << _program_name << " " << _program_version
		  << " (JILL " << JILL_VERSION << ")" << std::endl;
}


void
Options::print_usage()
{
	std::cout << "Usage: " << _program_name << " [options] " << std::endl;
	std::cout << visible_opts;
}


void
Options::parse(int argc, char **argv)
{

	// need two passes, one to find out if we need to read a config file
	po::store(po::command_line_parser(argc, argv).options(cmd_opts).
		  positional(pos_opts).allow_unregistered().run(), vmap);
	if (vmap.count("help")) {
		print_usage();
		throw Exit(EXIT_SUCCESS);
	}
	else if (vmap.count("version")) {
		print_version();
		throw Exit(EXIT_SUCCESS);
	}

	boost::filesystem::path configfile = get<string>("config","");
	if (boost::filesystem::is_regular_file(configfile)) {
		boost::filesystem::ifstream ff(configfile);
		std::cout << "[Parsing " << configfile.string() << ']' << std::endl;
		po::parsed_options parsed = po::parse_config_file(ff, cfg_opts, true);
		po::store(parsed, vmap);
	}
	// second pass to override configfile values
	po::parsed_options parsed = po::command_line_parser(argc, argv).options(cmd_opts).
		positional(pos_opts).allow_unregistered().run();
	po::store(parsed, vmap);

	po::notify(vmap);
	process_options();
}



JillOptions::JillOptions(const char *program_name, const char *program_version, bool supports_control)
	: Options(program_name, program_version), client_name(program_name)
{
	po::options_description jillopts("JILL options");
	jillopts.add_options()
		("name,n",    po::value<string>()->default_value(_program_name), "set client name")
		("log,l",     po::value<string>(), "set logfile (default stdout)")
		("out,o",     po::value<vector<string> >(), "add connection to output port")
		("in,i",      po::value<vector<string> >(), "add connection to input port/file")
		("attr,a",     po::value<vector<string> >(), "set additional attribute (key=value)");
	if (supports_control)
		jillopts.add_options()
			("ctrl,c",    po::value<vector<string> >(), "add connection to control port");
	cfg_opts.add_options()
		("attr,a",     po::value<vector<string> >());
	cmd_opts.add(jillopts);
	visible_opts.add(jillopts);
}


void
JillOptions::process_options()
{
	assign(output_ports,"out");
	assign(input_ports,"in");
	assign(control_ports,"ctrl");
	assign(client_name,"name");
	assign(logfile,"log");

	// parse key/value pairs
	if (vmap.count("attr")==0) return;
	vector<string> const & kv = vmap["attr"].as<vector<string> >();
	for (vector<string>::const_iterator it = kv.begin(); it != kv.end(); ++it) {
		string::size_type eq = it->find_last_of("=");
		if (eq == string::npos)
			throw po::invalid_option_value(" additional option syntax: key=value");
		additional_options[it->substr(0,eq)] = it->substr(eq+1);
	}
}
