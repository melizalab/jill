/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _CAPTURE_OPTIONS_HH
#define _CAPTURE_OPTIONS_HH

#include <jill/jill_options.hh>
#include <iostream>

namespace jill {

/**
 * This class parses options for the capture program from the
 * command-line and/or a configuration file.  As in writer.cc, we also
 * want to parse the default options for the JACK client, so we will
 * derive from a class that handles these options. The only main
 * difference is that the class is defined in a separate file from the application.
 */
class CaptureOptions : public JillOptions {

public:
	/**
	 * As in writer.cc, the constructor calls the base class's
	 * constructor and then adds any additional options of its
	 * own.
	 */
	CaptureOptions(const char *program_name, const char *program_version)
		: JillOptions(program_name, program_version) { // this calls the superclass constructor 

		// tropts is a group of options
		po::options_description tropts("Trigger options");
		tropts.add_options()
			("prebuffer", po::value<float>()->default_value(1000), 
			 "set prebuffer size (ms)")
			("period-size", po::value<float>()->default_value(20), 
			 "set analysis period size (ms)")
			("open-thresh", po::value<float>()->default_value(0.01), 
			 "set sample threshold for open gate (0-1.0)")
			("open-rate", po::value<float>()->default_value(20), 
			 "set crossing rate thresh for open gate (s^-1)")
			("open-period", po::value<float>()->default_value(500), 
			 "set integration time for open gate (ms)")
			("close-thresh", po::value<float>()->default_value(0.01), 
			 "set sample threshold for close gate")
			("close-rate", po::value<float>()->default_value(2),
			 "set crossing rate thresh for close gate (s^-1)")
			("close-period", po::value<float>()->default_value(5000), 
			 "set integration time for close gate (ms)")
			("write-counts,c", po::value<std::string>(),
			 "write state of threshold discriminator");

		// we add our group of options to various groups
		// already defined in the base class.
		cmd_opts.add(tropts);
		// cfg_opts holds options that are parsed from a configuration file
		cfg_opts.add(tropts);
		// visible_opts holds options that are shown in the help text
		visible_opts.add(tropts);
		// the output template is not added to the visible
		// options, since it's specified positionally.
		cmd_opts.add_options()
			("output-tmpl", po::value<std::string>(), "output file template");
		pos_opts.add("output-tmpl", -1);
	} 

	/*
	 * The public member variables are filled when we parse the
	 * options, and subsequently accessed by external clients
	 */

	std::string output_file_tmpl;
	std::string debug_file_name;

	float prebuffer_size_ms;  // in ms
	nframes_t prebuffer_size; // samples

	float open_threshold;
	float close_threshold;

	float open_crossing_rate;  // s^-1
	int open_count_thresh; // raw count
	float close_crossing_rate;
	int close_count_thresh;
	
	float period_size_ms; // in ms
	nframes_t period_size; // in samples

	float open_crossing_period_ms;
	int open_crossing_periods;
	float close_crossing_period_ms;
	int close_crossing_periods;

	/**
	 * Some of the values are specified in terms of time, but the
	 * WindowDiscriminator class uses units of samples.  The
	 * values can only be corrected after the samplerate is known.
	 * This function should be called before reading any of the
	 * following variables:
	 *
	 * prebuffer_size, period_size, open_count_thresh,
	 * close_count_thresh, open_crossing_periods, close_crossing_periods
	 */
	void adjust_values(nframes_t samplerate) {
		prebuffer_size = prebuffer_size_ms * samplerate / 1000;
		period_size = period_size_ms * samplerate / 1000;
		open_crossing_periods = open_crossing_period_ms / period_size_ms;
		close_crossing_periods = close_crossing_period_ms / period_size_ms;
		// count thresh is count rate * integration period
		open_count_thresh = open_crossing_rate * period_size / 1000 * open_crossing_periods;
		close_count_thresh = close_crossing_rate * period_size / 1000 * close_crossing_periods;
	}
		

protected:

	/*
	 * Here we override the base class's print_usage command to add some additional information
	 * about specifying the output filename template and the configuration file name
	 */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] [output-file-template]\n"
			  << visible_opts << std::endl
			  << "output-file-template:  specify output files (e.g. myrecording_%entry%.wav)\n"
			  << "                       if omitted, events are logged but no data is written\n"
		          << "                       Template parameters:\n"
		          << "                           %entry%        entry number\n"
		          << "                           %total_msec%   msec since start of day\n"
		          << "                           %month%        full month name\n"
		          << "                           %day%          day of month (1-31)\n"
		          << "                           %year%         full year\n"
		          << "                           %hour%         hour (0-23)\n"
		          << "                           %min%          minute (0-59)\n"
		          << "                           %sec%          second (0-59)\n"
		          << "\n"
			  << "configuration values will be read from capture.ini, if it exists"
			  << std::endl;
	}
	
	/*
	 * Likewise, we override process_options() to parse the
	 * options specified in this class. We call the Base function
	 * explicitly so that it parses all of the generic options first.
	 */
	virtual void process_options() {
		JillOptions::process_options();

		assign(output_file_tmpl, "output-tmpl");
		assign(debug_file_name, "write-counts");

		assign(prebuffer_size_ms, "prebuffer");
		assign(period_size_ms, "period-size");

		assign(open_threshold, "open-thresh");
		assign(open_crossing_rate, "open-rate");
		assign(open_crossing_period_ms, "open-period");

		assign(close_threshold, "close-thresh");
		assign(close_crossing_rate, "close-rate");
		assign(close_crossing_period_ms, "close-period");
	}

};

}

#endif
