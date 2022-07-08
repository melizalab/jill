/*
 * Generate noise
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2021 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <random>
#include <csignal>
#include <boost/filesystem.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/logging.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jnoise"

using namespace jill;
using std::string;
using sample_ringbuffer = dsp::ringbuffer<sample_t>;

class jnoise_options : public program_options {

public:
        jnoise_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        /** Ports to connect to */
        std::vector<string> output_ports;

	float start_hour;
	float stop_hour;
	
protected:

        void print_usage() override;

}; // jnoise_options


static jnoise_options options(PROGRAM_NAME);
static std::unique_ptr<jack_client> client;
jack_port_t *port_out;
static int ret = EXIT_SUCCESS;
static int running = 1;
static int daytime = 0;

static std::random_device rd;  //Will be used to obtain a seed for the random number engine
static std::mt19937 wn_gen(rd()); //Standard mersenne_twister_engine seeded with rd()
static std::normal_distribution<> wn_dis(0.0, 1.0);
int
process(jack_client *client, nframes_t nframes, nframes_t)
{
        sample_t *out = client->samples(port_out, nframes);

        for (nframes_t i = 0; i < nframes; ++i) {
		// TODO scale the output
		out[i] = wn_dis(wn_gen);
        }

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
                client.reset(new jack_client(options.client_name, options.server_name));

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->activate();

                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());

		// check time here
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
                LOG << "ERROR: " << e.what();
                return EXIT_FAILURE;
        }

}


jnoise_options::jnoise_options(string const &program_name)
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
        po::options_description opts("Module options");
        opts.add_options()
                ("amplitude,a",    po::value<float>(&output_scale)->default_value(1.0),
                 "scale output by factor")
		("start",          po::value<float>(&start_hour), "hour when noise starts")
		("stop",           po::value<float>(&stop_hour), "hour when noise stops")

        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jnoise_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       output port with noise\n"
                  << std::endl;
}
