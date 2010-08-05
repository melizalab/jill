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
#ifndef _OFFLINE_OPTIONS_HH
#define _OFFLINE_OPTIONS_HH

#include "options.hh"
#include <vector>


namespace jill {

namespace po = boost::program_options;


/**
 * The OfflineOptions class should be used instead of JillOptions when
 * running an offline test. Options handled are:
 *
 * -l : log file
 * -i : input wave file
 * -o : output wave file
 */
class OfflineOptions : public Options {
public:
	OfflineOptions(const char *program_name, const char *program_version);
	virtual ~OfflineOptions() {}

	/// The input file name
	std::string input_file;
	/// The output file name
	std::string output_file;
	/// The log file to write application events to
	std::string logfile;
	/// The size of block to read from the input file or write to output file
	int blocksize;
	/// The sampling rate to use in opening up output file, if no input file supplied
	int samplerate;

protected:
	virtual void process_options();

};

// implementations in options.cc

} // namespace jill

#endif // _OFFLINE_OPTIONS_HH
