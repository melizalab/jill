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
#include <boost/tokenizer.hpp>
#include "options.hh"
#include "main.hh"

using namespace jill;
typedef boost::char_separator<char> char_sep;
typedef boost::tokenizer<char_sep> tokenizer;

Options::Options(const char *program_name, const char *program_version)
	: client_name(program_name), _program_name(program_name), _program_version(program_version) {}

void Options::print_version()
{
	std::cout << _program_name << " " << _program_version << std::endl;
	std::cout << "JILL " VERSION << std::endl;
}

void Options::print_usage()
{
	std::cout
		<< "Usage:  " << _program_name << " [options]\n\n"
		<< "Options:\n"
		<< "  -n name               set jack client name\n"
		<< "  -l logfile            set logfile (default standard out)\n"
		<< "  -o port,...           set output port(s)\n"
		<< "  -i port,...           set input port(s)\n"
		<< std::endl;
}

void Options::parse(int argc, char **argv)
{
	int c;
	char optstring[] = "+n:l:o:i:Vh";

	while ((c = ::getopt(argc, argv, optstring)) != -1) {
		switch (c) {

		case 'V':
			print_version();
			throw Exit(EXIT_SUCCESS);

		case 'h':
			print_usage();
			throw Exit(EXIT_SUCCESS);
			break;

		case 'n':
			client_name = std::string(::optarg);
			break;

		case 'l':
			logfile = std::string(::optarg);
			break;

		case 'o':
		{
			std::string str(::optarg);
			char_sep sep(",");
			tokenizer tok(str, sep);
			for (tokenizer::iterator i = tok.begin(); i != tok.end(); ++i)
				output_ports.push_back(*i);
		} break;

		case 'i':
		{
			std::string str(::optarg);
			char_sep sep(",");
			tokenizer tok(str, sep);
			for (tokenizer::iterator i = tok.begin(); i != tok.end(); ++i)
				input_ports.push_back(*i);
		} break;

		default:
			throw Exit(EXIT_FAILURE);
			break;
		}
	}
}
