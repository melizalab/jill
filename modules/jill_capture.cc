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
 *
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include some headers from the JILL framework. Note
 * that we're using angle-brackets to reference the headers stored in
 * standard locations. This will be necessary when developing
 * standalone clients.
 */
#include <jill/simple_client.hh>
#include <jill/util/logger.hh>
#include <jill/util/arf_sndfile.hh>
#include <jill/jill_options.hh>

/*
 * These are headers for the custom class we're using in this
 * application.
 */
#include "triggered_writer.hh"

using namespace jill;

/*
 * This section is similar to writer.cc and mixer.cc. The
 * PlayerJillClass will be used in offline testing. We also declare a
 * pointer to our custom TriggerWriter class (which will be
 * initialized after we read in the options.
 */
static boost::scoped_ptr<SimpleClient> client;
static boost::shared_ptr<TriggeredWriter> twriter;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

/*
 * Note that the process function isn't here any more; it's
 * encapsulated in the TriggeredWriter class.  However, we do still
 * need to write a function that will be called in the main loop.  In
 * theory, we could pass a pointer to the flush() member function
 * directly to the Client class, but this requires some extra
 * work, and in many cases writing a wrapper function is much simpler.
 */
static int
mainloop()
{
	std::string outfile = twriter->flush();
	return 0;
}

static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop("Received interrupt");
}


/**
 * As in jill_thresh we define a class for parsing options
 */
class CaptureOptions : public JillOptions {

public:
	CaptureOptions(const char *program_name, const char *program_version)
		: JillOptions(program_name, program_version, true) {

		po::options_description tropts("Capture options");
		tropts.add_options()
			("prebuffer", po::value<float>()->default_value(1000),
			 "set prebuffer size (ms)")
			("buffer", po::value<float>()->default_value(2000),
			 "set file output buffer size (ms)")
			("trig-thresh", po::value<float>()->default_value(0.6),
			 "set threshold for trigger signal (-1.0-1.0)");

		// we add our group of options to various groups
		// already defined in the base class.
		cmd_opts.add(tropts);
		// cfg_opts holds options that are parsed from a configuration file
		cfg_opts.add(tropts);
		// visible_opts holds options that are shown in the help text
		visible_opts.add(tropts);
		// the output template is not added to the visible
		// options, since it's specified positionally.
		cmd_opts.add_options()
			("output-file", po::value<std::string>(), "output filename");
		pos_opts.add("output-file", -1);
	}

	/*
	 * The public member variables are filled when we parse the
	 * options, and subsequently accessed by external clients
	 */
	std::string output_file;
	float prebuffer_size_ms;  // in ms
	float buffer_size_ms;  // in ms
	nframes_t prebuffer_size; // samples
	nframes_t buffer_size; // samples
	float trig_threshold;

	void adjust_values(nframes_t samplerate) {
		prebuffer_size = (nframes_t)(prebuffer_size_ms * samplerate / 1000);
		buffer_size = (nframes_t)(buffer_size_ms * samplerate / 1000);
	}


protected:

	/*
	 * Here we override the base class's print_usage command to add some additional information
	 * about specifying the output filename template and the configuration file name
	 */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
			  << visible_opts << std::endl
			  << "Ports:\n"
			  << " * in:         for input of the signal(s) to be recorded\n"
			  << " * trig_in:    when >0.6, starts recording"
			  << std::endl;
	}

	virtual void process_options() {
		JillOptions::process_options();
		assign(output_file, "output-file");
		assign(prebuffer_size_ms, "prebuffer");
		assign(buffer_size_ms, "buffer");
		assign(trig_threshold, "trig-thresh");
	}

};

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		CaptureOptions options("jill_capture", "1.2.0b1");
		options.parse(argc,argv);

		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/* Initialize the client. */
		client.reset(new SimpleClient(options.client_name.c_str(), "in", 0, "trig_in"));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;
		options.adjust_values(client->samplerate());

		/* Initialize writer object and triggered writer */
		jill::util::ArfSndfile writer(options.output_file, client->samplerate());
		twriter.reset(new TriggeredWriter(writer, logv, options.prebuffer_size,
						  options.buffer_size, options.trig_threshold,
				      &options.additional_options));

		/* Log parameters */
		logv << logv.program << "output file: " << options.output_file << endl
		     << logv.program << "sampling rate: " << client->samplerate() << endl
		     << logv.program << "buffer size: " << options.buffer_size << endl
		     << logv.program << "prebuffer size: " << options.prebuffer_size << endl
		     << logv.program << "trigger threshold: " << options.trig_threshold << endl
		     << logv.program << "additional metadata:" << endl;
		for (map<string,string>::const_iterator it = options.additional_options.begin();
		     it != options.additional_options.end(); ++it)
			logv << logv.program << "  " << it->first << "=" << it->second << endl;


		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		/* connect ports */
		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_port(it->c_str(), "in");
			logv << logv.allfields << "Connected " << *it << " to input port" << endl;
		}

		for (it = options.control_ports.begin(); it != options.control_ports.end(); ++it) {
			client->connect_port(it->c_str(), "trig_in");
			logv << logv.allfields << "Connected " << *it << " to trigger port" << endl;
		}
		/*
		 * Here's where we pass the processing object to the
		 * client. The semantics are a little tricky, because
		 * twriter is a shared pointer (which we need in order
		 * to dynamically allocate it here).  The * operator
		 * dereferences the pointer, which we then wrap in
		 * boost::ref in order to ensure that the object is
		 * passed by reference - otherwise
		 * set_process_callback tries to copy it (which will
		 * raise a compiler error because TriggeredWriter is
		 * noncopyable)
		 */
		client->set_process_callback(boost::ref(*twriter));
		client->set_mainloop_callback(mainloop);
		client->run();
		logv << logv.allfields << client->get_status() << endl;
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
		cerr << "Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
