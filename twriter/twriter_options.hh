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
 *! @file
 *! @brief Options for the trigger writer
 *! 
 */
#ifndef _TWRITER_OPTIONS_HH
#define _TWRITER_OPTIONS_HH

#include <iostream>

namespace jill {

/**
 * Another way of unifying our testing and the final module is to have
 * a common options class. As in the previous chapter, we'll derive
 * from Options, but in this case the class will be defined here.
 *
 * Because we want to be able to use the TriggerOptions with
 * either a JILL application, or a standalone test application, the
 * base class is parameterized (i.e. specified as a template argument)
 */
template <typename Base=Options>
class TriggerOptions : public Base {

public:

	TriggerOptions(const char *program_name, const char *program_version)
		: Base(program_name, program_version) { // this calls the superclass constructor 
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
			 "set integration time for close gate (ms)");

		Base::cmd_opts.add(tropts);
		Base::cfg_opts.add(tropts);
		Base::visible_opts.add(tropts);
		Base::cmd_opts.add_options()
			("output-tmpl", po::value<std::string>(), "output file template");
		Base::pos_opts.add("output-tmpl", -1);
	} 

	std::string output_file_tmpl;

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

	/// adjust values for sample rate (in Hz)
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

	void print_usage() {
		std::cout << "Usage: " << Base::_program_name << " [options] [output-file-template]\n"
			  << Base::visible_opts << std::endl
			  << "output-file-template:  specify output files (e.g. myrecording_%03d.wav)\n"
			  << "                       if omitted, events are logged but no data is written\n\n"
			  << "configuration values will be read from twriter.ini, if it exists"
			  << std::endl;
	}

	void process_options() {
		Base::process_options();

		Base::assign(output_file_tmpl, "output-tmpl");

		Base::assign(prebuffer_size_ms, "prebuffer");
		Base::assign(period_size_ms, "period-size");

		Base::assign(open_threshold, "open-thresh");
		Base::assign(open_crossing_rate, "open-rate");
		Base::assign(open_crossing_period_ms, "open-period");

		Base::assign(close_threshold, "close-thresh");
		Base::assign(close_crossing_rate, "close-rate");
		Base::assign(close_crossing_period_ms, "close-period");
	}

};

}

#endif
