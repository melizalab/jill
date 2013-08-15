/*
 * Plays a full-scale sine wave at various frequencies for testing output range
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
#include "jill/logging.hh"

#define PROGRAM_NAME "jsine"

using namespace jill;
using std::string;

class jsine_options : public program_options {

public:
	jsine_options(string const &program_name);

	/** The server name */
	string server_name;
	/** The client name (used in internal JACK representations) */
	string client_name;

	/** Ports to connect to */
	std::vector<string> output_ports;

        double min_freq;
        double max_freq;
        double freq_rate;

protected:

	virtual void print_usage();

};

static jsine_options options(PROGRAM_NAME);
static boost::shared_ptr<jack_client> client;
jack_port_t *port_out;
static int ret = EXIT_SUCCESS;
static int running = 1;
double dt;
const double m_pi = 2*acos(0.0);

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        const double frange = log(options.max_freq) - log(options.min_freq);
        const double frate  = options.freq_rate / frange / log(2);

	sample_t *out = client->samples(port_out, nframes);
        double t, logf;
        for (nframes_t i = 0; i < nframes; ++i) {
                t = dt * (time + i);
                logf =  sin(2 * m_pi * frate * t);
                out[i] = sin(2 * m_pi * options.min_freq * t);
        }

        return 0;
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
                LOG << "frequency [" << options.min_freq << "," << options.max_freq
                              << "] Hz; rate " << options.freq_rate << "oct/s";


                client.reset(new jack_client(options.client_name, logger, options.server_name));
                dt = 1.0 / client->sampling_rate();

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client->set_process_callback(process);
                client->activate();

                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());

                while (running) {
                        usleep(100000);
                }

                client->deactivate();
	}

	catch (Exit const &e) {
		ret = e.status();
	}
	catch (std::exception const &e) {
                LOG << "ERROR: " << e.what();
		ret = EXIT_FAILURE;
	}
        return ret;
}

jsine_options::jsine_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output port");

        // tropts is a group of options
        po::options_description opts("jsine options");
        opts.add_options()
                ("min-freq,m",  po::value<double>(&min_freq)->default_value(100), "minimum frequency (Hz)")
                ("max-freq,M",  po::value<double>(&max_freq)->default_value(8000), "maximum frequency (Hz)")
                ("freq-rate,r", po::value<double>(&freq_rate)->default_value(1), "modulation rate (octaves/s)");

        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jsine_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       output port\n"
                  << std::endl;
}

