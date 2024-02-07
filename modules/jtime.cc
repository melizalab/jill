/*
 * Generate trigger events based on time of day
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
#include "jill/midi.hh"

#define PROGRAM_NAME "jtime"

using namespace jill;
using namespace boost::posix_time;
using std::string;

class jtime_options : public program_options {

public:
        jtime_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        /** Ports to connect to */
        std::vector<string> output_ports;
        /** The MIDI output channel */
        midi::data_type output_chan;

	string start_time;
	string stop_time;
	
protected:

        void print_usage() override;

}; // jtime_options


static jtime_options options(PROGRAM_NAME);
static std::unique_ptr<jack_client> client;
jack_port_t *port_trig;
std::atomic<int> ret(EXIT_SUCCESS);
// gets set to true when client is shutting down
std::atomic<bool> stopping(false);
// gets set to true by main thread the time interval starts or stops
std::atomic<bool> trigger(false);
// holds the current state
std::atomic<midi::data_type> status(midi::status_type::note_off);

int
process(jack_client *client, nframes_t nframes, nframes_t)
{
        void *trig_buffer = client->events(port_trig, nframes);
        jack_midi_data_t buf[] = {
		0,
		midi::default_pitch,
		midi::default_velocity
	};

	if (stopping.exchange(false) && status == midi::status_type::note_on) {
		buf[0] = midi::status_type(midi::status_type::note_off, options.output_chan);
		jack_midi_event_write(trig_buffer, 0 , buf, 3);
		status = midi::status_type::note_off;
	} else if (trigger.exchange(false)) {
		buf[0] |= status;
		jack_midi_event_write(trig_buffer, 0, buf, 3);
        }

        return 0;
}

// ignore xruns
int
jack_xrun(jack_client *client, float delay)
{
        return 0;
}


void
jack_shutdown(jack_status_t code, char const *)
{
	// we don't really need to try to clean up because all the downstream
	// stuff is going to get killed too, but we can try
        stopping = true;
	ret = -1;
}

void
signal_handler(int sig)
{
        stopping = true;
	ret = sig;
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
		LOG << "start event will occur at: " << start;
		LOG << "stop event will occur at:  " << stop;
			
                port_trig = client->register_port("trig_out", JACK_DEFAULT_MIDI_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->activate();

                client->connect_ports("trig_out", options.output_ports.begin(), options.output_ports.end());

		// check time here
                while (true) {
			bool is_day;
			midi::data_type new_status, old_status;
			time_duration time_of_day = second_clock::local_time().time_of_day();
			if (stopping) {
				old_status = status.exchange(midi::status_type::note_off);
				if (old_status == midi::status_type::note_on) {
					trigger = true;
					LOG << "signal off at " << time_of_day;
				}
				usleep(2e6 * client->buffer_size() / client->sampling_rate());
				break;
			}
			if (stop > start)
				is_day = ((start < time_of_day) && (time_of_day <= stop));
			else
				is_day = (!((stop < time_of_day) && (time_of_day <= start)));
			new_status = (is_day) ? midi::status_type::note_on : midi::status_type::note_off;
			old_status = status.exchange(new_status);
			if (old_status != new_status) {
				trigger = true;
				LOG << "signal " << ((is_day) ? "on " : "off") << " at " << time_of_day;
			}
                        sleep(1.0);
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


jtime_options::jtime_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output port")
                ("chan,c",    po::value<midi::data_type>(&output_chan)->default_value(0),
                 "set MIDI channel for output messages (0-16)");

        // tropts is a group of options
        po::options_description opts("Module options");
        opts.add_options()
		("start",
		 po::value<string>(&start_time)->default_value("00:00:00"),
		 "time of day to send start event")
		("stop",
		 po::value<string>(&stop_time)->default_value("24:00:00"),
		 "time of day to send stop event");

        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jtime_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * trig_out:       MIDI port producing start and stop events\n"
                  << std::endl;
}
