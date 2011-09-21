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
 *! @file window_discriminator.cc
 *! @brief window discriminator module
 *!
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>
#include <iostream>
/*
 * Note that the references to jill headers use angle brackets; do
 * likewise if the program lives outside the current directory
 */
#include <jill/simple_client.hh>
#include <jill/util/logger.hh>
#include <jill/jill_options.hh>
#include <jill/util/ringbuffer.hh>
#include "window_discriminator.hh"
using namespace jill;

typedef jill::WindowDiscriminator<sample_t> wdiscrim_t;

/* A smart pointer to the window discriminator */
static boost::scoped_ptr<SimpleClient> client;
static boost::scoped_ptr<wdiscrim_t> wdiscrim;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

/*
 * Store open and close times in a ringbuffer for logging. Use longs
 * in order to use sign for open vs close.
 */
util::Ringbuffer<int64_t> gate_times(128);

/*
 * The process callback has fairly simple behavior; it stuffs samples
 * into the window discriminator, determines if the threshold was
 * crossed, and then generates the output signal.  The control port
 * may or may not be defined; if it is, it's for outputting count
 * data.
 */
static void
process(sample_t *in, sample_t *out, sample_t *control, nframes_t nframes, nframes_t time)
{
	int frame;
	// Step 1: Pass samples to window discriminator; its state may
	// change, in which case the return value will be > -1 and
	// indicate the frame in which the gate opened or closed. It
	// also takes care of copying the current state of the buffer
	// to the control port (if not NULL)
	int offset = wdiscrim->push(in, nframes, control);

	// Step 2: Check state of gate
	if (wdiscrim->open()) {
		if (offset >= 0)
			gate_times.push_one(int64_t(time + offset));
		// 2A: gate is open; write 0 before offset and 1 after
		for (frame = 0; frame < offset; ++frame)
			out[frame] = 0.0f;
		for (; frame < (int)nframes; ++frame)
			out[frame] = 0.7f;
	}
	else {
		// 2B: gate is closed; write 1 before offset and 0 after
		if (offset >= 0)
			gate_times.push_one(int64_t(time + offset)*-1);
		for (frame = 0; frame < offset; ++frame)
			out[frame] = 0.7f;
		for (; frame < (int)nframes; ++frame)
			out[frame] = 0.0f;
	}
	// that's it!  It's up to the downstream client to decide what
	// to do with this information.
}

/* The usual signal handler */
static void
signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop("Received interrupt");
}

int
mainloop()
{
	int64_t *times;
	int ntimes = gate_times.peek(&times);
	for (int i = 0; i < ntimes; ++i) {
		if (times[i] > 0)
			logv << logv.allfields << "Signal start @"  << times[i] << std::endl;
		else
			logv << logv.allfields << "Signal stop  @"  << -times[i] << std::endl;
	}
	gate_times.advance(ntimes);
	return 0;
}

/**
 * This class parses options for the program from the command-line
 * and/or a configuration file.  As in writer.cc, we also want to
 * parse the default options for the JACK client, so we will derive
 * from a class that handles these options. There are a lot of
 * options, so this class requires a lot of boilerplate code.
 */
class TriggerOptions : public jill::JillOptions {

public:
	TriggerOptions(std::string const &program_name, std::string const &program_version)
		: JillOptions(program_name, program_version, true) { // this calls the superclass constructor

		// tropts is a group of options
		po::options_description tropts("Trigger options");
		tropts.add_options()
			("period-size", po::value<float>()->default_value(20),
			 "set analysis period size (ms)")
			("open-thresh", po::value<float>()->default_value(0.01),
			 "set sample threshold for open gate (0-1.0)")
			("open-rate", po::value<float>()->default_value(20),
			 "set crossing rate thresh for open gate (s^-1)")
			("open-period", po::value<float>()->default_value(500),
			 "set integration time for open gate (ms)")
			("close-thresh", po::value<float>()->default_value(0.01),
			 "set sample threshold for close gate")
			("close-rate", po::value<float>()->default_value(2),
			 "set crossing rate thresh for close gate (s^-1)")
			("close-period", po::value<float>()->default_value(5000),
			 "set integration time for close gate (ms)");

		// we add our group of options to various groups
		// already defined in the base class.
		cmd_opts.add(tropts);
		// cfg_opts holds options that are parsed from a configuration file
		cfg_opts.add(tropts);
		// visible_opts holds options that are shown in the help text
		visible_opts.add(tropts);
	}

	float open_threshold;
	float close_threshold;

	float open_crossing_rate;  // s^-1
	int open_count_thresh; // raw count
	float close_crossing_rate;
	int close_count_thresh;

	float period_size_ms; // in ms
	nframes_t period_size; // in samples

	float open_crossing_period_ms;
	int open_crossing_periods;
	float close_crossing_period_ms;
	int close_crossing_periods;

	/**
	 * Some of the values are specified in terms of time, but the
	 * WindowDiscriminator class uses units of samples.  The
	 * values can only be corrected after the samplerate is known.
	 * This function should be called before reading any of the
	 * following variables:
	 *
	 * prebuffer_size, period_size, open_count_thresh,
	 * close_count_thresh, open_crossing_periods, close_crossing_periods
	 */
	void adjust_values(nframes_t samplerate) {
		period_size = (nframes_t)(period_size_ms * samplerate / 1000);
		open_crossing_periods = (int)(open_crossing_period_ms / period_size_ms);
		close_crossing_periods = (int)(close_crossing_period_ms / period_size_ms);
		// count thresh is count rate * integration period
		open_count_thresh = (int)(open_crossing_rate * period_size / 1000 * open_crossing_periods);
		close_count_thresh = (int)(close_crossing_rate * period_size / 1000 * close_crossing_periods);
	}


protected:

	/*
	 * Need to override process_options() to parse the options
	 * specified in this class. We call the function from the base
	 * class explicitly so that it parses all of the generic
	 * options first.
	 */
	virtual void process_options() {
		JillOptions::process_options();

		assign(period_size_ms, "period-size");
		assign(open_threshold, "open-thresh");
		assign(open_crossing_rate, "open-rate");
		assign(open_crossing_period_ms, "open-period");
		assign(close_threshold, "close-thresh");
		assign(close_crossing_rate, "close-rate");
		assign(close_crossing_period_ms, "close-period");
	}

	/* also override print_usage to give user some additional help */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options]\n"
			  << visible_opts << std::endl
			  << "Ports:\n"
			  << " * in:         for input of the signal(s) to be monitored\n"
			  << " * trig_out:   is >0.6 when the gate is open\n"
			  << " * count_out:  (optional) the current estimate of signal power"
			  << std::endl;
	}

}; // TriggerOptions

/* the main function looks a lot like mixer or delay */
int
main(int argc, char **argv)
{
	using namespace std;
	try {

		/* Note the additional argument to load options from a configuration file */
		TriggerOptions options("jill_thresh", "1.2.0rc1");
		options.parse(argc,argv);

		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * if the user asks for a count port to be created
		 * (for debugging), we use the control port on
		 * SimpleClient, setting it as an output.
		 */
		std::string count_port = (options.control_ports.empty()) ? "" : "count_out";
		client.reset(new SimpleClient(options.client_name, "in", "trig_out", count_port, false));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;

		options.adjust_values(client->samplerate());
		wdiscrim.reset(new wdiscrim_t(options.open_threshold,
					      options.open_count_thresh,
					      options.open_crossing_periods,
					      options.close_threshold,
					      options.close_count_thresh,
					      options.close_crossing_periods,
					      options.period_size));
		/* Log parameters */
		logv << logv.program << "sampling rate: " << client->samplerate() << endl
		     << logv.program << "period size (samples): " << options.period_size << endl
		     << logv.program << "open threshold: " << options.open_threshold << endl
		     << logv.program << "open count thresh: " << options.open_count_thresh << endl
		     << logv.program << "open periods: " << options.open_crossing_periods << endl
		     << logv.program << "close threshold: " << options.close_threshold << endl
		     << logv.program << "close count thresh: " << options.close_count_thresh << endl
		     << logv.program << "close periods: " << options.close_crossing_periods << endl;

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_port(*it, "in");
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client->connect_port("trig_out", *it);
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		for (it = options.control_ports.begin(); it != options.control_ports.end(); ++it) {
			client->connect_port(count_port, *it);
			logv << logv.allfields << "Connected counter to port " << *it << endl;
		}

		client->set_mainloop_callback(mainloop);
		client->set_mainloop_delay(1000);
		client->set_process_callback(process);
		client->run();
		logv << logv.allfields << client->get_status() << endl;

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
