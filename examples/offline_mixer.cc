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
 *
 * This file shows how to test a JILL client offline.  All of the code
 * remains the same except for a few substitutions, which are noted below.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

#include "jill/offline_application.hh"  // instead of jill_application.hh
#include "jill/offline_options.hh" // instead of jill_options.hh
#include "jill/util/logger.hh"
using namespace jill;

static boost::scoped_ptr<OfflineApplication> app; // instead of JillApplication
static util::logstream logv;
static int ret = EXIT_SUCCESS;

void
process(sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	memcpy(out, in, sizeof(sample_t) * nframes);
}

static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	app->signal_quit();
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {

		OfflineOptions options("mixer", "1.0.0rc3"); // instead of JillOptions
		options.parse(argc,argv);

		logv.set_program("mixer_test");
		logv.set_stream(options.logfile);

		// The interface is AudioInterfaceOffline instead of AudioInterfaceJack
		AudioInterfaceOffline client(options.blocksize, options.samplerate);
		// Need to connect to input and output files
		if (options.input_file.empty()) {
			std::cerr << "Error: input file required" << std::endl;
			throw Exit(EXIT_FAILURE);
		}
		client.set_input(options.input_file);
		logv << logv.allfields << "Opened input file " << options.input_file 
		     << " (Fs = " << client.samplerate() << ")" << std::endl;

		if (!options.output_file.empty()) {
			client.set_output(options.output_file);
			logv << logv.allfields << "Opened output file " << options.output_file 
			     << " (Fs = " << client.samplerate() << ")" << std::endl;
		}
		client.set_process_callback(process);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		app.reset(new OfflineApplication(client, logv)); // instead of JillApplication
		app->run();
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::exception const &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
