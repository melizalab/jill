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
 * This example is the second chapter in the JILL tutorial. It shows
 * how to implement a slightly more complicated process function and
 * how to add additional command-line options.  This application
 * differs from the simple mixer in that it inserts a fixed delay
 * between the input and output stream. This requires storing the
 * samples in a temporary buffer; we're using a JILL class called
 * DelayBuffer for this.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers.  We've added an
 * additional header which gives us access to the
 * DelayBuffer class.
 */
#include "jill/jill_application.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
#include "jill/filters/delay_buffer.hh"
using namespace jill;


/*
 * We've added an additional module-scope variable, the delay
 * buffer. This has to be available at this scope in order for the
 * process() function to have access to it.
 */
static boost::scoped_ptr<JillApplication> app;
static util::logstream logv;
static int ret = EXIT_SUCCESS;
static boost::scoped_ptr<filters::DelayBuffer<sample_t> > buffer;

/*
 * In this app, we write incoming data to the delay buffer and read
 * the outgoing data from the buffer.  The buffer will pad the output
 * with zeros if not enough data is available.
 *
 */
void
process(sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	buffer->push_pop(in, out, nframes);
}

void 
signal_handler(int sig)
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
		/*
		 * We can use the same Options class to parse the
		 * command-line options, but we need to add an
		 * additional option so the user can specify the
		 * delay.  Under the hood, the Options class uses a
		 * boost library that makes it very easy to add
		 * additional options. For more details see
		 * http://www.boost.org/doc/libs/1_43_0/doc/html/program_options.html
		 */	
		JillOptions options("delay", "1.0.0rc4"); 
		options.cmd_opts.add_options()
			("delay,d", po::value<float>()->default_value(10), "set delay time (ms)");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * We need to start up the client to get access to the
		 * JACK server's sample rate; however, we cant' start
		 * the process loop until the buffer has been
		 * allocated.
		 */
		AudioInterfaceJack client(options.client_name, AudioInterfaceJack::Filter);
		logv << logv.allfields << "Started client; samplerate " << client.samplerate() << endl;

		/*
		 * To calculate the buffer size, we get the delay
		 * option from the command line and multiply it by the
		 * sampling rate.
		 */
		float delay_time = options.get<float>("delay");
		unsigned int buffer_size = ceil(delay_time / 1000 * client.samplerate());
		logv << logv.allfields << "Allocating buffer of size " << buffer_size 
		     << " (" << delay_time << " ms)" << endl;
		buffer.reset(new filters::DelayBuffer<sample_t>(buffer_size));

		logv << logv.allfields << "Starting client" << endl;
		client.set_process_callback(process);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		app.reset(new JillApplication(client, logv));
		app->connect_inputs(options.input_ports);
		app->connect_outputs(options.output_ports);
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
