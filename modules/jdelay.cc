/*
 * Inserts a fixed delay between the input and output
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dmeliza@uchicago.edu>
 */
#include <iostream>
#include <signal.h>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/util/stream_logger.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jdelay"
#define PROGRAM_VERSION "2.0.0-beta2"

using namespace jill;
using std::string;
typedef dsp::ringbuffer<sample_t> sample_ringbuffer;

class jdelay_options : public program_options {

public:
	jdelay_options(string const &program_name, string const &program_version);

	/** The server name */
	string server_name;
	/** The client name (used in internal JACK representations) */
	string client_name;

	/** Ports to connect to */
        std::vector<string> input_ports;
	std::vector<string> output_ports;

        float delay_msec;
        nframes_t delay;

protected:

	virtual void print_usage();

}; // jstim_options


static jdelay_options options(PROGRAM_NAME, PROGRAM_VERSION);
static boost::shared_ptr<util::stream_logger> logger;
static boost::shared_ptr<jack_client> client;
static sample_ringbuffer ringbuf(1024);
jack_port_t *port_in, *port_out;
static int ret = EXIT_SUCCESS;
static int running = 1;

int
process(jack_client *client, nframes_t nframes, nframes_t)
{
	sample_t *in = client->samples(port_in, nframes);
	sample_t *out = client->samples(port_out, nframes);

        if (ringbuf.push(in, nframes) != nframes) {
#ifndef NDEBUG
                logger->log() << "error: buffer overrun";
#endif
        }
        if (ringbuf.pop(out, nframes) != nframes) {
#ifndef NDEBUG
                logger->log() << "error: buffer underrun";
#endif
        }

        return 0;
}

/** this is called by jack when calculating latency */
void
jack_latency (jack_latency_callback_mode_t mode, void *arg)
{
        float sr = 0.001 * client->sampling_rate();
	jack_latency_range_t range;
	if (mode == JackCaptureLatency) {
		jack_port_get_latency_range (port_in, mode, &range);
                logger->log() << "estimated capture latency (ms): ["
                              << range.min / sr << "," << range.max / sr << "]";
		range.min += options.delay;
		range.max += options.delay;
		jack_port_set_latency_range (port_out, mode, &range);
	}
        else {
		jack_port_get_latency_range (port_out, mode, &range);
		range.min += options.delay;
		range.max += options.delay;
		jack_port_set_latency_range (port_in, mode, &range);
                logger->log() << "estimated playback latency (ms): ["
                              << range.min / sr << "," << range.max / sr << "]";
	}
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        ringbuf.resize(options.delay + nframes);
        // simple way to set a fixed delay is to advance pointer
        ringbuf.push(0, options.delay);
        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        ret = -1;
        running = 0;
}

void
signal_handler(int sig)
{
        ret = sig;
        running = 0;
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                logger.reset(new util::stream_logger(options.client_name, cout));
                logger->log() << PROGRAM_NAME ", version " PROGRAM_VERSION;

                client.reset(new jack_client(options.client_name, logger, options.server_name));
                options.delay = options.delay_msec * client->sampling_rate() / 1000;
                logger->log() << "delay: " << options.delay_msec << " ms (" << options.delay << " frames)";

                port_in = client->register_port("in",JACK_DEFAULT_AUDIO_TYPE,
                                                JackPortIsInput, 0);
                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_buffer_size_callback(jack_bufsize);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                jack_set_latency_callback (client->client(), jack_latency, 0);
                client->activate();

                client->connect_ports(options.input_ports.begin(), options.input_ports.end(), "in");
                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());

                while (running) {
                        usleep(100000);
                }

                client->deactivate();
		return ret;
	}

	catch (Exit const &e) {
		return e.status();
	}
	catch (std::exception const &e) {
                if (logger) logger->log() << "FATAL ERROR: " << e.what();
		return EXIT_FAILURE;
	}

}


jdelay_options::jdelay_options(string const &program_name, string const &program_version)
        : program_options(program_name, program_version)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<vector<string> >(&input_ports), "add connection to input port")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output port");

        // tropts is a group of options
        po::options_description opts("Delay options");
        opts.add_options()
                ("delay,d",   po::value<float>(&delay_msec)->default_value(10),
                 "delay to add between input and output (ms)");

        cmd_opts.add(jillopts).add(opts);
        cfg_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jdelay_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:        input port\n"
                  << " * out:       output port with delayed signal\n"
                  << std::endl;
}

