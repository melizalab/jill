/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is a template of a JILL client, which just copies data
 * from its input to its output.  Adapt it to write your own module.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <fstream>

#include "jill/main.hh"
#include "jill/application.hh"
#include "jill/util/logger.hh"

using namespace jill;

/// The name of the client and ports; obviously this won't work if things are unusual
static const std::string client_name("template");
static const std::string input_port("system:capture_1");
static const std::string output_port("system:playback_1");

static boost::scoped_ptr<Application> app;

static int ret = EXIT_SUCCESS;

/** 
 * This function is the processing loop, which runs in a real-time
 * JACK thread.  Depending on the type of interface, it can receive
 * input data, or be required to generate output data.
 * 
 * @param in Pointer to the input buffer. NULL if the client has no input port
 * @param out Pointer to the output buffer. NULL if no output port
 * @param nframes The number of frames in the data
 */
void 
process(sample_t *in, sample_t *out, nframes_t nframes)
{
	memcpy(out, in, sizeof(sample_t) * nframes);
}

/** 
 * This function handles termination signals and gracefully closes the
 * application.
 */
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
		// parse options
		Options options("template", "1.0.0rc");
		options.parse(argc,argv);

		// fire up the logger
		util::logstream logv(0, options.client_name.c_str());
		std::ofstream log;
		if (!options.logfile.empty()) {
			log.open(options.logfile.c_str());
			logv.set_stream(&log);
		}
		else
			logv.set_stream(&std::cout);
		logv << logv.allfields << "Starting client" << endl;

 		// start up the client
 		AudioInterfaceJack client(client_name, JackPortIsInput|JackPortIsOutput);
 		client.set_process_callback(process);

 		// instantiate the application
 		app.reset(new Application(&client, &options, &logv));
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::runtime_error const &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
	
// 	

// 	boost::scoped_ptr<AudioInterfaceJack> client;
// 	client.reset(new AudioInterfaceJack(client_name, JackPortIsInput|JackPortIsOutput));
// 	logv << logv.allfields << " Initialized interface" << endl;

// 	// set the callback
// 	client->set_process_callback(process);

// 	// connect the ports
// 	client->connect_input(input_port);
// 	client->connect_output(output_port);
// 	logv << logv.allfields << " Connected to ports" << endl;

// 	// let the client run for a while
// 	for (;;) {
// 		::usleep(10000);
// 		if (client->is_shutdown()) {
// 			logv << logv.program << " Jack server shut down: exiting " << endl;
// 			break;
// 		}
// 	}

	// cleanup is automatic!
}
