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
 * This file is an extremely simple JILL client, which just copies
 * data from its input to its output.  It's also the first chapter in
 * the JILL tutorial.
 *
 * A JILL application needs to perform three tasks:
 * 1. Define a real-time processing function
 * 2. Parse command-line options and set up data logging
 * 3. Handle application startup and shutdown
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers.  Most applications will
 * use the simple_jill_client.hh header. If you plan to support
 * offline input (i.e. from a wav file), you will also need to include
 * player_jill_client.hh.  In addition, jill_options.hh contains a
 * class used to parse command-line options, and jill/util/logger.hh
 * contains code for writing a timestamped logfile.
 */
#include "jill/simple_jill_client.hh"
#include "jill/player_jill_client.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
using namespace jill;


/*
 * Next, we define several variables with module scope. These refer to
 * objects that need to be accessible to multiple functions in this
 * compilation module.  The SimpleJillClient, for example, is used by
 * both main() and signal_handler().
 *
 * Note, however, that the constructor for SimpleJillClient requires
 * that we specify the audio client and a logger, and we have no way
 * of initializing the client until we've parsed the command-line
 * options.  The solution is to create a pointer to an object of type
 * SimpleJillClient, and then initialize the object once we have enough
 * information to do so.  Using pointers introduces some issues in
 * resource management; to deal with these we use a "smart pointer",
 * which guarantees that the object will be cleaned up when the
 * application exits.
 *
 * The pclient is also a smart pointer, but to an object of type
 * PlayerJillClient. This object is a second client that is only
 * instantiated if the user specifies a sound file as input.  It's
 * responsible for reading that file and sending the samples to JACK.
 * 
 * We also create a logstream, and an integer to hold the exit status
 * of the application.  We don't really need the logstream at module
 * scope for this application, but it will come in handy in later
 * examples.
 */
static boost::scoped_ptr<SimpleJillClient> client;
static boost::shared_ptr<PlayerJillClient> pclient;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

/*
 * This function is the processing loop, which is how the application
 * interacts with the auditory stream.  The JACK server calls this
 * function in a real-time loop, providing pointers to an input and an
 * output buffer.  Applications can be, generally speaking, Sinks,
 * Sources, or Filters (see below for how to define this).  If it
 * receives data, it needs to deal with this data as rapidly as
 * possible and then return control to the server.  If it generates
 * data, it needs to fill the output buffer and then return control to
 * the server.
 *
 * This example doesn't do much; it just copies the data from the
 * input to the output buffer. However, if more than one source is
 * connected to the application's input port, the streams will be
 * mixed.
 *
 * @param in Pointer to the input buffer. NULL if the client has no
 *           input port 
 * @param out Pointer to the output buffer. NULL if no
 *            output port 
 * @param nframes The number of frames in the data 
 * @param time The frame count at the beginning of the process loop. This is
 * guaranteed to be unique for all loops in this process
 */
void
process(sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	memcpy(out, in, sizeof(sample_t) * nframes);
}

/*
 * We also need to handle signals from the user and the operating
 * system and shut down in a graceful manner. This function is called
 * by the OS when the user hits Ctrl-C, or the application is killed
 * with kill(1). It calls the signal_quit() method of the application,
 * which tells it to shut down.
 */
static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop();
	// here we have to check to see if the pclient object actually got allocated
	if (pclient)
		pclient->stop();
}

/*
 * This is the main() function of the application. It performs tasks
 * related to setting up the application, including parsing command
 * line options, starting the logger, and starting the appplication.
 */
int
main(int argc, char **argv)
{
	using namespace std;
	try {

		/*
		 * The Options class is responsible for parsing
		 * command line arguments. It comes with some
		 * predefined options.  Here, we tell the class the
		 * name of the application, the version, and pass it
		 * the command line arguments.
		 */
		JillOptions options("mixer", "1.1.0rc1");
		options.parse(argc,argv);

		/*
		 * Next, we set up logging. This is handled by the
		 * logstream class, which we instantiated
		 * earlier. After parsing the options, we know the
		 * name of the client and the logfile to use.
		 * Logstream has some members (e.g. allfields) that
		 * can be used to add the name of the client and a
		 * timestamp to the logfile, as we do here.
		 */
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * Now we're ready to start up the processing
		 * client. Most simple clients can use the
		 * AudioInterfaceJack class, which can handle both
		 * inputs and outputs, up to one of each.  The
		 * arguments to the constructor specify the client
		 * name and client type.  A Filter has one input and
		 * one output; a Sink has one input, and a Source has
		 * one output.  We then tell the client what process
		 * function to call with the set_process_callback()
		 * function.
		 */
		client.reset(new SimpleJillClient(options.client_name, "in", "out"));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;
		client->set_process_callback(process);

		/*
		 * Similarly, we need to tell the OS what function to
		 * call when it receives a termination signal
		 * (i.e. SIGINT, SIGTERM, SIGHUP). See signal(3) for
		 * details on other types of signals the program can handle.
		 */
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		/*
		 * Here, we handle the case where the user wants to
		 * use a sound file as input, rather than a JACK
		 * port. This is done by creating a second client
		 * whose job it is to read in the sound file and
		 * output the samples to the JACK server, which will
		 * send them to the primary client. This may seem
		 * unecessarily complicated, but by doing it this way,
		 * we use the JACK framework as much as possible and
		 * we don't have to write a separate set of code for
		 * handling the offline case.
		 *
		 * The PlayerJillClient has a static factory function,
		 * from_port_list(), that will parse our list of input
		 * ports and determine if any of them refer to a file.
		 * If so, it creates a PlayerJillClient object to
		 * handle the file, and returns a pointer to the newly
		 * created client.  At various points we'll have to
		 * check to make sure whether this happened or not.
		 */
		pclient = PlayerJillClient::from_port_list(options.input_ports);
		if (pclient)
			logv << logv.allfields << "Input file: " << *pclient << endl;

		/*
		 * Next, we connect our client to the input and output
		 * ports specified by the user. When we called
		 * from_port_list(), it modified the list of input
		 * ports so that if the user specified a sound file,
		 * that place in the list is replaced by a connection
		 * to the associated PlayerJillClient.
		 */
		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_input(*it);
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client->connect_output(*it);
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		/*
		 * At last, we start the client running. We have to
		 * make a decision about which client's run() function
		 * to call.  If the user specified an input file, we
		 * want to play the file and then exit, so we call
		 * run() on pclient.  Otherwise, we just want to copy
		 * samples from the input ports to the output ports
		 * endlessly, and we call run() on client.
		 */
		if (pclient) 
			pclient->run(100000);
		else
			client->run(100000);

		return ret;
	}
	/*
	 * These catch statements handle two kinds of exceptions.  The
	 * Exit exception is thrown to terminate the application
	 * normally (i.e. if the user asked for the app version or
	 * usage); other exceptions are typically thrown if there's a
	 * serious error, in which case the user is notified on
	 * stderr.
	 */
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::exception const &e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	/* Because we used smart pointers and locally scoped variables, there is no cleanup to do! */
}
