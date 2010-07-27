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
 * This file is a template of a JILL client, which just copies data
 * from its input to its output.  Adapt it to write your own module.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <fstream>
#include <signal.h>

#include "jill/main.hh"
#include "jill/application.hh"
#include "jill/filters.hh"
#include "jill/util/logger.hh"

using namespace jill;

// module-level variables are the application and return type
static util::logstream logv;
static boost::scoped_ptr<Application> app;
static int ret = EXIT_SUCCESS;
// for the delay app we need a delay buffer
static boost::scoped_ptr<DelayBuffer<sample_t> > buffer;

/**
 * This function is the processing loop, which runs in a real-time
 * JACK thread.  Depending on the type of interface, it can receive
 * input data, or be required to generate output data.
 *
 * In this app, we write incoming data to the delay buffer and read
 * the outgoing data from the buffer.  The buffer will pad the output
 * with zeros if not enough data is available.
 *
 * @param in Pointer to the input buffer. NULL if the client has no input port
 * @param out Pointer to the output buffer. NULL if no output port
 * @param nframes The number of frames in the data
 */
void
process(sample_t *in, sample_t *out, nframes_t nframes)
{
	buffer->push_pop(in, out, nframes);
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
		Options options("delay", "1.0.0rc");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name.c_str());
		logv.set_stream(options.logfile);

		// initialize buffer
		buffer.reset(new DelayBuffer<sample_t>(2000));

		// start up the client
		logv << logv.allfields << "Starting client" << endl;
		AudioInterfaceJack client(options.client_name, JackPortIsInput|JackPortIsOutput);
		client.set_process_callback(process);

		// set up signal handlers to exit cleanly when terminated
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		// instantiate the application
		app.reset(new Application(client, options, logv));
		app->setup();
		app->run();
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::runtime_error const &e) {
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}
}
