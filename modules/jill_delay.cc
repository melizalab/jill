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
 * Here we include the relevant JILL headers.  We've dropped the
 * SndfilePlayerClient (sort of pointless), and added an additional
 * header which gives us access to the DelayBuffer class.
 */
#include "jill/simple_client.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
#include "jill/util/delay_buffer.hh"
using namespace jill;


/*
 * We've added an additional module-scope variable, the delay
 * buffer. This has to be available at this scope in order for the
 * process() function to have access to it.
 */
static boost::scoped_ptr<SimpleClient> client;
static util::logstream logv;
static int ret = EXIT_SUCCESS;
static util::DelayBuffer<sample_t> buffer;

/*
 * In principle, this application is quite similar to mixer, in that
 * we are moving data from the input port to the output port.
 * However, now we want to insert a delay between the input and
 * output.  This is accomplished by the DelayBuffer class, which acts
 * like a pipe.  After pushing N samples into the top of the pipe, N
 * samples are available at the other end, which are copied to the
 * output port.  The interface for this is quite simple, since all the
 * internals are hidden in the DelayBuffer class.
 *
 * We also introduce the concept of a control line.  For this client,
 * the control line is interpreted like a mask; it's multiplied by the
 * output signal, so if the control line is 0 the output will be
 * silent.  This allows the user to hook the delay module up to a
 * trigger module to control when delayed playback occurs.
 *
 */
void
process(sample_t *in, sample_t *out, sample_t *ctrl, nframes_t nframes, nframes_t time)
{
	buffer.push_pop(in, out, nframes);
	// the ctrl pointer will be NULL if user didn't register a control port
	if (ctrl) {
		for (unsigned int i = 0; i < nframes; ++i)
			out[i] *= ctrl[i];
	}
}

void
signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop("Received interrupt");
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
		 *
		 * Also, note the additional third argument. By
		 * default, the user doesn't get an option to create a
		 * control port, but this can be changed with an
		 * argument.
		 */
		JillOptions options("jill_delay", "1.2.0rc1", true);
		po::options_description delopts("Delay options");
		delopts.add_options()
			("delay,d", po::value<float>()->default_value(10), "set delay time (ms)");
		options.cmd_opts.add(delopts);
		options.visible_opts.add(delopts);
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * We need to start up the client to get access to the
		 * JACK server's sample rate, but first need to figure
		 * out if the user wants a control port.  Delay
		 * starting the process loop until the buffer has been
		 * allocated.
		 */
		std::string ctrl_port = (options.control_ports.empty()) ? "" : "control";
		client.reset(new SimpleClient(options.client_name, "in", "out", ctrl_port));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;

		/*
		 * To calculate the buffer size, we get the delay
		 * option from the command line and multiply it by the
		 * sampling rate.
		 */
		float delay_time = options.get<float>("delay");
		nframes_t buffer_size = (nframes_t)ceil(delay_time / 1000 * client->samplerate());
		logv << logv.allfields << "Allocating delay buffer of size " << buffer_size
		     << " (" << delay_time << " ms)" << endl;
		buffer.resize(buffer_size);


		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_port(*it, "in");
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client->connect_port("out", *it);
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		for (it = options.control_ports.begin(); it != options.control_ports.end(); ++it) {
			client->connect_port(*it, ctrl_port);
			logv << logv.allfields << "Connected control to port " << *it << endl;
		}

		/*
		 * start the client running; it will continue until
		 * the user hits ctrl-c or an error occurs
		 */
		client->set_process_callback(process);
		client->run();
		logv << logv.allfields << client->get_status() << endl;
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
