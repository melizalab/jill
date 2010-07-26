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
#include "jill/buffered_sndfile.hh"
#include "jill/util/logger.hh"

using namespace jill;

/**
 * First, we need to subclass Options to handle the additional
 * command-line options for this module.  Note how we call the
 * superclass functions to parse the default options.
 *
 * The option added is:
 * -f  : output file
 */
class WriterOptions : public Options {

public:
	WriterOptions(const char *program_name, const char *program_version)
		: Options(program_name, program_version) {} // this calls the superclass constructor

	std::string output_file;
	int parse(int argc, char **argv) {
		int n, optind;
		optind = Options::parse(argc, argv);
		for (n = optind; n < argc; ++n) {
			output_file = std::string(argv[n]);
			if (!output_file.empty()) return ++n;
		}
		throw CmdlineError("Need to specify an output file");
	}
protected:
	void print_usage() {
		Options::print_usage();
		std::cout << "\n\n"
			  << "Arguments:\n"
			  << "   wavfile            specify output file\n";
	}
};

static util::logstream logv;
static boost::scoped_ptr<Application> app;
static BufferedSndfile<sample_t> sndfile;
static int ret = EXIT_SUCCESS;

/**
 * This function is the processing loop, which runs in a real-time
 * JACK thread.  For this module, we just copy the data to the the
 * buffered writer.  If not all the samples get written, we throw an
 * error.
 *
 * @param in Pointer to the input buffer. NULL if the client has no input port
 * @param out Pointer to the output buffer. NULL if no output port
 * @param nframes The number of frames in the data
 */
static void
process(sample_t *in, sample_t *out, nframes_t nframes)
{
	nframes_t nf = sndfile.writef(in, nframes);
	if (nf < nframes)
		throw std::runtime_error("ringbuffer filled up");
}

/** 
 * This function is a callback that runs in the main thread of this
 * program. Actions that don't require realtime priority should be
 * placed here.  In this example, we use the main loop to write data
 * to the disk. Note that sndfile's operator()() performs the same
 * thing, so we could have treated the object as a function.
 * 
 * @return 0 for success, non-zero to terminate the application
 */
static int 
mainloop()
{
	sndfile();
	return 0;
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

/**
 * This function logs xruns; important to figure out where audio data is bad.
 */
static int log_xrun(float usec)
{
	logv << logv.allfields << "xrun: " << usec << " us" << std::endl;
	return 0;
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		// parse options
		WriterOptions options("writer", "1.0.0rc");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name.c_str());
		logv.set_stream(options.logfile);

		// start up the client
		logv << logv.allfields << "Starting client" << endl;
		AudioInterfaceJack client(options.client_name, JackPortIsInput);
		client.set_xrun_callback(log_xrun);
		client.set_process_callback(process);

		// open the output file (after connecting to server to sampling rate)
		logv << logv.allfields << "Opening " << options.output_file 
		     << " for output; Fs = " << client.samplerate() << endl;
		sndfile.open(options.output_file.c_str(), client.samplerate());

		// set up signal handlers to exit cleanly when terminated
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		// instantiate the application
		app.reset(new Application(client, options, logv));
		app->setup();
		app->set_mainloop_callback(mainloop);
		app->run();
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::runtime_error const &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	// cleanup is automatic!
}
