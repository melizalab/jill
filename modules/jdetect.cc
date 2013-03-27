/*
 * Simple crossing-based signal detector
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 */
#include <iostream>
#include <signal.h>
#include <boost/scoped_ptr.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/crossing_trigger.hh"
#include "jill/util/stream_logger.hh"

#define PROGRAM_NAME "jdetect"
#define PROGRAM_VERSION "1.3.0"

using namespace jill;

class jdetect_options : public program_options {

public:
	jdetect_options(std::string const &program_name, std::string const &program_version);

	/** The client name (used in internal JACK representations) */
	std::string client_name;

	/** A vector of inputs to connect to the client */
	std::vector<std::string> input_ports;
	/** A vector of outputs to connect to the client */
	std::vector<std::string> output_ports;
        /** The MIDI output channel */
        midi::data_type output_chan;

	float open_threshold;
	float close_threshold;

	float open_crossing_rate;  // s^-1
	float close_crossing_rate;

	float period_size_ms; // in ms
	float open_crossing_period_ms;
	float close_crossing_period_ms;

protected:

	virtual void print_usage();

}; // jdetect_options


jdetect_options options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<util::stream_logger> logger;
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<dsp::crossing_trigger<sample_t> > trigger;
jack_port_t *port_in, *port_trig, *port_count;
int stopping = 0;               // set to 1 to get process to clean up

/* data storage for event times */
struct event_t {
        nframes_t time;
        int status;
};
dsp::ringbuffer<event_t> trig_times(128);

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
	sample_t *in = client->samples(port_in, nframes);
        sample_t *out = (port_count) ? client->samples(port_count, nframes) : 0;
        void *trig_buffer = client->events(port_trig, nframes);
        jack_midi_data_t buf[] = { jack_midi_data_t(options.output_chan & midi::chan_nib),
                                   midi::default_pitch, midi::default_velocity };

        if (stopping && trigger->open()) {
                buf[0] += midi::note_off;
                jack_midi_event_write(trig_buffer, 0, buf, 3);
                __sync_add_and_fetch(&stopping, -1);
                return 0;
        }

	// Step 1: Pass samples to window discriminator; its state may
	// change, in which case the return value will be > -1 and
	// indicate the frame in which the gate opened or closed. It
	// also takes care of copying the current state of the buffer
	// to the count monitor port (if not NULL)
	int offset = trigger->push(in, nframes, out);
        if (offset < 0) return 0;

        buf[0] += (trigger->open()) ? midi::note_on : midi::note_off;
        event_t event = { time + offset, buf[0] & midi::type_nib }; // data sent to logger
        if (jack_midi_event_write(trig_buffer, offset, buf, 3) != 0) {
                // indicate error to logger function
                event.status = midi::sysex;
        }
        trig_times.push(event);

	return 0;
}

/** visitor function for gate time ringbuffer */
std::size_t log_times(event_t const * events, std::size_t count)
{
        event_t const *e;
        std::size_t i;
	for (i = 0; i < count; ++i) {
                e = events+i;
                std::ostream &os = logger->log();
                if (e->status==midi::note_on)
                        os << "signal on: ";
                else if (e->status==midi::note_off)
                        os << "signal off:";
                else
                        os << "WARNING: detected but couldn't send event: ";
                os << " frames=" << e->time << ", us=" << client->time(e->time) << std::endl;
        }
        return i;
}

void
signal_handler(int sig)
{
        __sync_add_and_fetch(&stopping, 1);
        // wait for at least one process loop; not strictly async safe
        usleep(2e6 * client->buffer_size() / client->sampling_rate());
        exit(sig);
}

/**
 * Callback for samplerate changes. This function is only called once.
 */
int
samplerate_callback(jack_client *client, nframes_t samplerate)
{
        using std::endl;
	nframes_t period_size = options.period_size_ms * samplerate / 1000;
	int open_crossing_periods = options.open_crossing_period_ms / options.period_size_ms;
	int close_crossing_periods  = options.close_crossing_period_ms / options.period_size_ms;
	int open_count_thresh = options.open_crossing_rate * period_size / 1000 * open_crossing_periods;
	int close_count_thresh = options.close_crossing_rate * period_size / 1000 * close_crossing_periods;

        trigger.reset(new dsp::crossing_trigger<sample_t>(options.open_threshold,
                                                         open_count_thresh,
                                                         open_crossing_periods,
                                                         options.close_threshold,
                                                         close_count_thresh,
                                                         close_crossing_periods,
                                                         period_size));

        // Log parameters
        logger->msg() << "period size: " << options.period_size_ms << " ms, " << period_size << " samples" << endl;
        logger->msg() << "open threshold: " << options.open_threshold << endl;
        logger->msg() << "open count thresh: " << open_count_thresh << endl;
        logger->msg() << "open integration window: " << options.open_crossing_period_ms << " ms, " << open_crossing_periods << " periods " << endl;
        logger->msg() << "close threshold: " << options.close_threshold << endl;
        logger->msg() << "close count thresh: " << close_count_thresh << endl;
        logger->msg() << "close integration window: " << options.close_crossing_period_ms << " ms, " << close_crossing_periods << " periods " << endl;
        return 0;
}


int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                logger.reset(new util::stream_logger(cout, options.client_name));
                logger->msg() << PROGRAM_NAME ", version " PROGRAM_VERSION << endl;
                client.reset(new jack_client(options.client_name, *logger));

                port_in = client->register_port("in", JACK_DEFAULT_AUDIO_TYPE,
                                               JackPortIsInput | JackPortIsTerminal, 0);
                port_trig = client->register_port("trig_out",JACK_DEFAULT_MIDI_TYPE,
                                                JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("count-port")) {
                        port_count = client->register_port("count",JACK_DEFAULT_AUDIO_TYPE,
                                                          JackPortIsOutput | JackPortIsTerminal, 0);
                }

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client->set_sample_rate_callback(samplerate_callback);
                client->set_process_callback(process);
                client->activate();

                client->connect_ports(options.input_ports.begin(), options.input_ports.end(), "in");
                client->connect_ports("trig_out", options.output_ports.begin(), options.output_ports.end());

                while(1) {
                        sleep(1);
                        trig_times.pop(log_times); // calls visitor function on ringbuffer
                }

		return EXIT_SUCCESS;
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


jdetect_options::jdetect_options(std::string const &program_name, std::string const &program_version)
        : program_options(program_name, program_version)
{
        using std::string;
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<vector<string> >(&input_ports), "add connection to input port")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output port")
                ("chan,c",    po::value<midi::data_type>(&output_chan)->default_value(0),
                 "set MIDI channel for output messages (0-16)");

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

        cmd_opts.add(jillopts).add(tropts);
        cfg_opts.add(jillopts).add(tropts);
        visible_opts.add(jillopts).add(tropts);
}

void
jdetect_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:       for input of the signal(s) to be monitored\n"
                  << " * trig_out:  MIDI port producing gate open and close events\n"
                  << " * count:    (optional) the current estimate of signal power"
                  << std::endl;
}

