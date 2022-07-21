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
#include <atomic>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/logging.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jnoise"

using namespace jill;
using namespace boost::posix_time;
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

        string start_time;
        string stop_time;
        float loud_db;
        float quiet_db;

protected:

        void print_usage() override;

}; // jnoise_options


static jnoise_options options(PROGRAM_NAME);
static std::unique_ptr<jack_client> client;
jack_port_t *port_out;
float loud_scale = 0;
float quiet_scale = 0;
std::atomic<int> ret(EXIT_SUCCESS);
std::atomic<bool> running(true);
std::atomic<bool> daytime(false);

static std::random_device rd;  //Will be used to obtain a seed for the random number engine
static std::mt19937 wn_gen(rd()); //Standard mersenne_twister_engine seeded with rd()
static std::normal_distribution<> wn_dis(0.0, 1.0);
int
process(jack_client *client, nframes_t nframes, nframes_t)
{
        sample_t *out = client->samples(port_out, nframes);

        if (daytime) {
                for (nframes_t i = 0; i < nframes; ++i) {
                        out[i] = wn_dis(wn_gen) * loud_scale;
                }
        } else {
                for (nframes_t i = 0; i < nframes; ++i) {
                        out[i] = wn_dis(wn_gen) * quiet_scale;
                }
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
        running = false;
}

void
signal_handler(int sig)
{
        ret = sig;
        running = false;
}

int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                client.reset(new jack_client(options.client_name, options.server_name));

                // parse the arguments
                time_duration start = duration_from_string(options.start_time);
                time_duration stop = duration_from_string(options.stop_time);
                LOG << "noise will be " << options.loud_db << " dB FS between " << start << "--" << stop;
                LOG << "noise will be " << options.quiet_db << " dB FS between " << stop << "--" << start;
                // calculate scaling factor. RMS of standard normal is 3.0103 dB FS
                loud_scale = pow(10, (options.loud_db - 3.0103) / 20);
                quiet_scale = pow(10, (options.quiet_db - 3.0103) / 20);

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
                        time_duration time_of_day = second_clock::local_time().time_of_day();
                        bool is_day;
                        if (stop > start)
                                is_day = (start < time_of_day) && (time_of_day <= stop);
                        else
                                is_day = !((stop < time_of_day) && (time_of_day <= start));
                        bool old = daytime.exchange(is_day);
                        if (old != is_day) {
                                if (is_day)
                                        LOG << "noise @ " options.loud_db " dB FS at " << time_of_day;
                                else
                                        LOG << "noise @ " options.quiet_db " dB FS at " << time_of_day;
                        }
                        sleep(5.0);
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
                ("loud-amplitude,l",    po::value<float>(&loud_db)->default_value(-33.0),
                 "amplitude of the noise during the day (in dB FS)")
                ("quiet-amplitude,q",    po::value<float>(&quiet_db)->default_value(-40.2),
                 "amplitude of the noise during the night (in dB FS)")
                ("start",          po::value<string>(&start_time)->default_value("00:00:00"),
                 "time of day when noise gets louder")
                ("stop",           po::value<string>(&stop_time)->default_value("24:00:00"),
                 "time of day when noise gets quieter");

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
