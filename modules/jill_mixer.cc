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
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers.  Most applications will
 * use the jack_client.hh header. In addition, options.hh contains a
 * class used to parse command-line options, and jill/logger.hh
 * contains code for writing a timestamped logfile.
 */
#include "jill/jack_client.hh"
#include "jill/options.hh"
#include "jill/logger.hh"
using namespace jill;


/*
 * Next, we define several variables with module scope. These refer to objects
 * that need to be accessible to multiple functions in this compilation module.
 * The most important objects are the ports we'll use to communicate with input
 * and output data. We also allocate a logstream, and an integer to hold the
 * exit status of the application. We don't really need the logstream at module
 * scope for this application, but it will come in handy in later examples.
 */
jack_port_t *port_in, *port_out;
logstream logv;
int ret = EXIT_SUCCESS;

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
 * The __restrict keyword is a promise to the compiler that in and out
 * point to different buffers, which can allow some additional
 * optimization.
 *
 * @param in Pointer to the input buffer. NULL if the client has no
 *           input port
 * @param out Pointer to the output buffer. NULL if no
 *            output port
 * @param trig Pointer to the event list. Not used here.
 * @param nframes The number of frames in the data
 * @param time The frame count at the beginning of the process loop. This is
 * guaranteed to be unique for all loops in this process
 */
int
process(JackClient *client, nframes_t nframes, nframes_t time)
{
        sample_t *in = client->buffer(port_in, nframes);
        sample_t *out = client->buffer(port_out, nframes);
	memcpy(out, in, sizeof(sample_t) * nframes);
        return 0;
}

/*
 * We also need to handle signals from the user and the operating
 * system and shut down in a graceful manner. This function is called
 * by the OS when the user hits Ctrl-C, or the application is killed
 * with kill(1).
 */
static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;
        exit(ret);
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
		JillOptions options("jill_mixer", "1.2.0rc1");
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
		JackClient client(options.client_name);

                port_in = client.register_port("in", JACK_DEFAULT_AUDIO_TYPE,
                                               JackPortIsInput | JackPortIsTerminal, 0);
                port_out = client.register_port("out",JACK_DEFAULT_AUDIO_TYPE,
                                                JackPortIsOutput | JackPortIsTerminal, 0);

                client.set_process_callback(process);
                client.activate();
		logv << logv.allfields << "Started client; samplerate " << client.samplerate() << endl;

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
		 * Next, we connect our client to the input and output
		 * ports specified by the user. When we called
		 * from_port_list(), it modified the list of input
		 * ports so that if the user specified a sound file,
		 * that place in the list is replaced by a connection
		 * to the associated PlayerClient.
		 */
		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client.connect_port(*it, "in");
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client.connect_port("out", *it);
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		/*
		 * At last, we start the client processing
		 * samples. This doesn't happen until the process
		 * callback gets registered (which one should avoid
		 * doing until after the ports are connected and
		 * everything else is set up.  The process callback
		 * runs in a separate thread.
		 *
		 * Depending on whether the user specified an input
		 * file, we either wait indefinitely in the main loop,
		 * or wait until the playback is finished. We could
		 * make the choice using a jack if construction, but
		 * just to illustrate, we take advantage of the fact
		 * that JackClient and PlayerClient share a
		 * common base class (Client) and use a
		 * polymorphic pointer.
		 */

                while(1) {
                        sleep(1);
                }

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
