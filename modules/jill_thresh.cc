/*
 * A client that broadcasts data over the network using zeroMQ
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <iostream>
#include <signal.h>

#include <boost/scoped_ptr.hpp>

#include "jill/jack_client.hh"
#include "jill/options.hh"
#include "jill/logger.hh"
#include "jill/ringbuffer.hh"
#include "jill/midi.hh"
#include "jill/dsp/crossing_trigger.hh"
using namespace jill;


boost::scoped_ptr<dsp::CrossingTrigger<sample_t> > trigger;
jack_port_t *port_in, *port_trig, *port_count;
logstream logv;
int ret = EXIT_SUCCESS;

/*
 * Store open and close times in a ringbuffer for logging. Use longs
 * in order to use sign for open vs close.
 */
Ringbuffer<int64_t> gate_times(128);

int
process(JackClient *client, nframes_t nframes, nframes_t time)
{
	int frame;
	sample_t *in = client->samples(port_in, nframes);
        sample_t *out = (port_count) ? client->samples(port_count, nframes) : 0;
        void *trig = client->events(port_trig, nframes);

	// Step 1: Pass samples to window discriminator; its state may
	// change, in which case the return value will be > -1 and
	// indicate the frame in which the gate opened or closed. It
	// also takes care of copying the current state of the buffer
	// to the count monitor port (if not NULL)
	int offset = trigger->push(in, nframes, out);

	// Step 2: Check state of gate
	if (trigger->open()) {
		if (offset >= 0)
			gate_times.push(int64_t(time + offset));
                event_t evt(offset, event_t::note_on);
                evt.write(trig);
	}
	else {
		// 2B: gate is closed; write 1 before offset and 0 after
		if (offset >= 0)
			gate_times.push(int64_t(time + offset)*-1);
                event_t evt(offset, event_t::note_off);
                evt.write(trig);
	}
	// that's it!  It's up to the downstream client to decide what
	// to do with this information.

	return 0;
}

/** visitor function for gate time ringbuffer */
nframes_t log_times(int64_t const * times, nframes_t count)
{
	for (int i = 0; i < count; ++i) {
		if (times[i] > 0)
			logv << logv.allfields << "Signal start @"  << times[i] << std::endl;
		else
			logv << logv.allfields << "Signal stop  @"  << -times[i] << std::endl;
	}
}


void
jack_shutdown(void *arg)
{
	exit(1);
}

void
signal_handler(int sig)
{
        exit(ret);
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
		: JillOptions(program_name, program_version) { // this calls the superclass constructor

		// tropts is a group of options
		po::options_description tropts("Trigger options");
		tropts.add_options()
                        ("count-port", "create port to output integrator state")
			("period-size", po::value<float>(&period_size_ms)->default_value(20),
			 "set analysis period size (ms)")
			("open-thresh", po::value<float>(&open_threshold)->default_value(0.01),
			 "set sample threshold for open gate (0-1.0)")
			("open-rate", po::value<float>(&open_crossing_rate)->default_value(20),
			 "set crossing rate thresh for open gate (s^-1)")
			("open-period", po::value<float>(&open_crossing_period_ms)->default_value(500),
			 "set integration time for open gate (ms)")
			("close-thresh", po::value<float>(&close_threshold)->default_value(0.01),
			 "set sample threshold for close gate")
			("close-rate", po::value<float>(&close_crossing_rate)->default_value(2),
			 "set crossing rate thresh for close gate (s^-1)")
			("close-period", po::value<float>(&close_crossing_period_ms)->default_value(5000),
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

	/* additional help */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options]\n"
			  << visible_opts << std::endl
			  << "Ports:\n"
			  << " * in:       for input of the signal(s) to be monitored\n"
			  << " * trigger:  MIDI port with gate open and close events\n"
			  << " * count:    (optional) the current estimate of signal power"
			  << std::endl;
	}

}; // TriggerOptions


int
main(int argc, char **argv)
{
	using namespace std;
	try {

		TriggerOptions options("jill_thresh", "1.2.0rc1");
		options.parse(argc,argv);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                JackClient client(options.client_name);
		logv.set_program(client.name());
		logv.set_stream(options.logfile);


                port_in = client.register_port("in", JACK_DEFAULT_AUDIO_TYPE,
                                               JackPortIsInput | JackPortIsTerminal, 0);
                port_trig = client.register_port("trigger",JACK_DEFAULT_MIDI_TYPE,
                                                JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("count-port")) {
                        port_count = client.register_port("count",JACK_DEFAULT_AUDIO_TYPE,
                                                          JackPortIsOutput | JackPortIsTerminal, 0);
                }

                std::cout << event_t::note_off << std::endl;

                client.set_process_callback(process);
                client.activate();
		logv << logv.allfields << "Started client; samplerate " << client.samplerate() << endl;

		options.adjust_values(client.samplerate());
		trigger.reset(new dsp::CrossingTrigger<sample_t>(options.open_threshold,
                                                                  options.open_count_thresh,
                                                                  options.open_crossing_periods,
                                                                  options.close_threshold,
                                                                  options.close_count_thresh,
                                                                  options.close_crossing_periods,
                                                                  options.period_size));

		/* Log parameters */
		logv << logv.program << "period size (samples): " << options.period_size << endl
		     << logv.program << "open threshold: " << options.open_threshold << endl
		     << logv.program << "open count thresh: " << options.open_count_thresh << endl
		     << logv.program << "open periods: " << options.open_crossing_periods << endl
		     << logv.program << "close threshold: " << options.close_threshold << endl
		     << logv.program << "close count thresh: " << options.close_count_thresh << endl
		     << logv.program << "close periods: " << options.close_crossing_periods << endl;

		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client.connect_port(*it, "in");
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

                while(1) {
                        sleep(1);
                        gate_times.pop(log_times);
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
