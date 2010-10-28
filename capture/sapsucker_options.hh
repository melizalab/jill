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
#ifndef _SAPSUCKER_OPTIONS_HH
#define _SAPSUCKER_OPTIONS_HH

#include <jill/jill_options.hh>
#include <iostream>

#include "capture_options.hh"
#include "quotas.hh"

namespace capture {

/**
 * This class parses options for the capture program from the
 * command-line and/or a configuration file.  As in writer.cc, we also
 * want to parse the default options for the JACK client, so we will
 * derive from a class that handles these options. The only main
 * difference is that the class is defined in a separate file from the application.
 */
class SapsuckerOptions : public jill::CaptureOptions  {

public:
	/**
	 * As in writer.cc, the constructor calls the base class's
	 * constructor and then adds any additional options of its
	 * own.
	 */
	SapsuckerOptions(const char *program_name, const char *program_version)
		: CaptureOptions(program_name, program_version) { // this calls the superclass constructor 

		// tropts is a group of options
		jill::po::options_description tuopts("Tutor Playback options");
		tuopts.add_options()
			("tutor-song", jill::po::value<std::string>()->default_value(""), 
			 "tutor song to play back")
			("switch-refraction", jill::po::value<int>()->default_value(2), 
			 "minimum delay between switch pulls")
			("interval-file", jill::po::value<std::string>()->default_value(""), 
			 "file for interval quotas")
			("switch-active", jill::po::value<int>()->default_value(0), 
			 "respond to the tutoring switch")
			("switch-keypress", jill::po::value<int>()->default_value(1), 
			 "use keypress instead of nidaq")
			("dio-device-name", jill::po::value<std::string>()->default_value("/dev/comedi0"), 
			 "nidaq device name")
			("dio-device-line", jill::po::value<int>()->default_value(0),
			 "dio device line to monitor")
			("dio-subdevice-index", jill::po::value<int>()->default_value(0), 
			 "subdevice index for dio card (almost always 0)")
			("switch-status-file", jill::po::value<std::string>()->default_value(""), 
			 "file for switch playback information");

		// we add our group of options to various groups
		// already defined in the base class.
		cmd_opts.add(tuopts);
		// cfg_opts holds options that are parsed from a configuration file
		cfg_opts.add(tuopts);
		// visible_opts holds options that are shown in the help text
		visible_opts.add(tuopts);
	} 

	/*
	 * The public member variables are filled when we parse the
	 * options, and subsequently accessed by external clients
	 */

	QuotaInfo quotas;
	std::string tutor_song_file_name;
	std::string interval_file_name;
	std::string switch_status_file_name;
	int switch_refraction;
	int use_keypress;
	int switch_active;
	std::string comedi_device_name;
	int comedi_subdevice_index;
	int dio_line;

	virtual void process_options() {
		CaptureOptions::process_options();

		assign(switch_active, "switch-active");
		assign(tutor_song_file_name, "tutor-song");
		assign(switch_refraction, "switch-refraction");
		assign(interval_file_name, "interval-file");
		assign(comedi_device_name, "dio-device-name");
		assign(comedi_subdevice_index, "dio-subdevice-index");
		assign(dio_line, "dio-device-line");
		assign(use_keypress, "switch-keypress");
		assign(switch_status_file_name, "switch-status-file");

		quotas = load_quotas_from_file(interval_file_name.c_str());
		std::cout << "Done!" << std::endl;
	}

};

}

#endif
