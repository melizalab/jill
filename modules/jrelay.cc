/*
 * A jill module that relays stimulus onset and offset events to open-ephys
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <random>
#include <csignal>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/midi.hh"
#include "jill/program_options.hh"
#include "jill/dsp/buffered_data_writer.hh"
#include "jill/net/open_ephys_logger.hh"

#define PROGRAM_NAME "jrelay"
#define DEFAULT_OPEN_EPHYS_ENDPOINT "tcp://localhost:5556"

using namespace jill;
using std::string;

class jrelay_options : public program_options {

public:
        jrelay_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        /** Ports to connect to */
        std::vector<string> input_ports;
	
	/** The open-ephys NetworkEvents zmq address */
	string openephys_addr;

protected:

        void print_usage() override;

}; // jrelay_options

static jrelay_options options(PROGRAM_NAME);
/** buffer to send events from process thread to the main thread */
std::unique_ptr<dsp::buffered_data_writer> zmq_thread;
jack_port_t *port_in;


int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
	void * in = client->events(port_in, nframes);
        jack_midi_event_t event;
        nframes_t nevents = jack_midi_get_event_count(in);
        for (nframes_t i = 0; i < nevents; ++i) {
                jack_midi_event_get(&event, in, i);
                if (event.size < 1) continue;
		zmq_thread->push(time + event.time, EVENT, jack_port_short_name(port_in), event.size, event.buffer);
	}
	return 0;
}

/** handle server shutdowns */
void
jack_shutdown(jack_status_t _code, char const * msg)
{
        LOG << "jackd shut the client down (" << msg << ")";
	if (zmq_thread) zmq_thread->stop();

}

/** handle POSIX signals */
void
signal_handler(int sig)
{
        DBG << "shutting down on signal";
	if (zmq_thread) zmq_thread->stop();
}

int
main(int argc, char **argv)
{
        using namespace std;
	int ret = 0;
        try {
                // parse options
                options.parse(argc,argv);
		// future: support multiple endpoints. For now, only open-ephys
		if (options.count("open-ephys") > 0) {
			auto logger = std::make_unique<net::open_ephys_logger>(options.openephys_addr);
			logger->send("jrelay connected");
			zmq_thread.reset(new dsp::buffered_data_writer(std::move(logger)));
		}
		else {
			LOG << "error: no relay endpoints configured; exiting";
			return EXIT_FAILURE;
		}
                // start client
                auto client = std::make_unique<jack_client>(options.client_name,
                                                            options.server_name);

                // register ports
                port_in = client->register_port("in",JACK_DEFAULT_MIDI_TYPE,
                                                JackPortIsInput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                // register jack callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_process_callback(process);

                // activate client
                client->activate();
		zmq_thread->start();

                // connect ports
                client->connect_ports(options.input_ports.begin(), options.input_ports.end(), "in");

		zmq_thread->join();
                client->deactivate();
        }

        catch (Exit const &e) {
                ret = e.status();
        }
        catch (std::exception const &e) {
                LOG  << "ERROR: " << e.what();
                ret = EXIT_FAILURE;
        }
	zmq_thread.reset();
	return ret;
		

}

/** configure commandline options */
jrelay_options::jrelay_options(string const &program_name)
        : program_options(program_name)
{

        // this section is for general JILL options. try to maintain consistency
        // with other modules
        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<std::vector<string> >(&input_ports), "add connection to input port");
        po::options_description opts("Module options");
	opts.add_options()
		("open-ephys",
		 po::value(&openephys_addr)->implicit_value(DEFAULT_OPEN_EPHYS_ENDPOINT),
		 "endpoint for an open-ephys NetworkEvents plugin");
        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

/** provide the user with some information about the ports */
void
jrelay_options::print_usage()
{
        std::cout << _program_name << ": relay stimulus events to other data acquisition systems\n\n"
                  << "Usage: " << _program_name << " ...\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:        input event port\n"
                  << std::endl;
}


