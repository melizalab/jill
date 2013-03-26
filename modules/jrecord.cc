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
 *
 */
#include <signal.h>
#include <boost/scoped_ptr.hpp>
#include <boost/format.hpp>
#include <sys/time.h>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/dsp/period_ringbuffer.hh"
#include "jill/util/string.hh"
#include "jill/util/portreg.hh"
#include "jill/file/continuous_arf_thread.hh"
#include "jill/file/triggered_arf_thread.hh"

#define PROGRAM_NAME "jrecord"
#define PROGRAM_VERSION "1.3.0"

using namespace jill;

/* declare options parsing class */
class jrecord_options : public program_options {

public:
	jrecord_options(std::string const &program_name, std::string const &program_version);

	/** The client name (used in internal JACK representations) */
        std::string client_name;

	/** Vectors of inputs to connect to the client */
        std::vector<std::string> input_ports;
        std::vector<std::string> trig_ports;

        /** key-value pairs to store as attributes in created entries */
        std::map<std::string, std::string> additional_options;

        std::string output_file;
	float pretrigger_size_s;
	float posttrigger_size_s;
	float buffer_size_s;
	int max_size_mb;
        int compression;

protected:

	virtual void print_usage();
	virtual void process_options();

};

jrecord_options options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<dsp::multichannel_data_thread> arf_thread;

/*
 * Copy data from ports into ringbuffer. Note that the first port is the trigger
 * port, so it contains MIDI data; however, the buffer size is the same so it
 * can be treated like audio data for now (should probably assert this
 * somewhere)
 */
int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        sample_t *port_buffer;
        jack_client::port_list_type::const_iterator it;

        // check that the ringbuffer has enough space for all the channels so we
        // don't write partial periods
        if (arf_thread->write_space(nframes) < client->nports())
                arf_thread->xrun();

        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                period_info_t info = {time, nframes, *it};
                port_buffer = client->samples(*it, nframes);
                arf_thread->push(port_buffer, info);
        }

        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        // increment xrun counter
        arf_thread->xrun();
        return 0;
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        arf_thread->log(util::make_string() << "[jack] period size = " << nframes);
        // only takes effect if requested size > buffer size; blocks until old
        // data is flushed
        nframes = arf_thread->resize_buffer(nframes * 32, client->nports());
        client->log() << "ringbuffer size (samples): " << nframes << std::endl;
        return 0;
}


void
jack_portcon(jack_client *client, jack_port_t* port1, jack_port_t* port2, int connected)
{
        util::make_string msg;
        msg << "[jack] port " << ((connected) ? "" : "dis") << "connect: "
            << jack_port_name(port1) << " -> " << jack_port_short_name(port2);
        if (arf_thread) arf_thread->log(msg);
}

void
jack_shutdown(jack_status_t code, char const *)
{
        if (arf_thread) arf_thread->stop();
}


void
signal_handler(int sig)
{
        if (arf_thread) {
                arf_thread->log(util::make_string() << "[" << client->name() << "] got terminate signal");
                arf_thread->stop();
                arf_thread->join();
        }
        if (client)
                client->deactivate();

        exit(sig);
}

int
main(int argc, char **argv)
{
	using namespace std;
        jack_port_t *port_trig = 0;

        vector<string>::const_iterator it;
        util::port_registry ports;
	try {
		options.parse(argc,argv);
                cout << "[" << options.client_name << "] " <<  PROGRAM_NAME ", version " PROGRAM_VERSION << endl;

                // initialize client
                client.reset(new jack_client(options.client_name));

                /* create ports: one for trigger, and one for each input */
                if (options.count("trig")) {
                        port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                          JackPortIsInput | JackPortIsTerminal, 0);
                        arf_thread.reset(new file::triggered_arf_thread(options.output_file,
                                                                        port_trig,
                                                                        options.pretrigger_size_s * client->sampling_rate(),
                                                                        options.posttrigger_size_s * client->sampling_rate(),
                                                                        options.additional_options,
                                                                        client.get(),
                                                                        options.compression));
                }
                else {
                        arf_thread.reset(new file::continuous_arf_thread(options.output_file,
                                                                         options.additional_options,
                                                                         client.get(),
                                                                         options.compression));
                }
                arf_thread->log("[" PROGRAM_NAME "] version = " PROGRAM_VERSION);
                client->log() << "opened output file " << options.output_file << endl;

                ports.add(client.get(), options.input_ports.begin(), options.input_ports.end());

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                // register callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->set_port_connect_callback(jack_portcon);
                client->set_buffer_size_callback(jack_bufsize);

                // start disk thread and activate process callback
                arf_thread->start();
                client->activate();

		/* connect ports */
		for (it = options.trig_ports.begin(); it != options.trig_ports.end(); ++it) {
			client->connect_port(*it, "trig_in");
		}
                ports.connect_all(client.get()); // connects input ports

                arf_thread->join();

                // manually deactivating the client ensures shutdown events get logged
                client->deactivate();

                return 0;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
                cerr << "Error: " << e.what() << endl;
                if (arf_thread) arf_thread->log(util::make_string() << "[" PROGRAM_NAME "] ERROR: " << e.what());
		return EXIT_FAILURE;
	}
}


/** implementation of jrecord_options */
jrecord_options::jrecord_options(std::string const &program_name, std::string const &program_version)
        : program_options(program_name, program_version)
{

        using std::string;
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("name,n",     po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",       po::value<vector<string> >(&input_ports), "connect to input port")
                ("trig,t",    po::value<vector<string> >(&trig_ports)->multitoken()->zero_tokens(),
                 "record in triggered mode (optionally specify input ports)")
                ("buffer",     po::value<float>(&buffer_size_s)->default_value(2.0),
                 "minimum ringbuffer size (s)");

        po::options_description tropts("Capture options");
        tropts.add_options()
                ("attr,a",     po::value<vector<string> >(),
                 "set additional attributes for recorded entries (key=value)")
                ("pretrigger", po::value<float>(&pretrigger_size_s)->default_value(1.0),
                 "duration to record before onset trigger (s)")
                ("posttrigger", po::value<float>(&posttrigger_size_s)->default_value(0.0),
                 "duration to record after offset trigger (s)")
                ("compression", po::value<int>(&compression)->default_value(0),
                 "set compression in output file (0-9)");

        // command-line options
        cmd_opts.add(jillopts).add(tropts);
        cmd_opts.add_options()
                ("output-file,f", po::value<std::string>(), "output filename");
        pos_opts.add("output-file", -1);

        cfg_opts.add(jillopts).add(tropts);
        visible_opts.add(jillopts).add(tropts);
}


void
jrecord_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in_NNN:     for input of the signal(s) to be recorded\n"
                  << " * trig_in:    MIDI port to receive events triggering recording"
                  << std::endl;
}

void
jrecord_options::process_options()
{
        program_options::process_options();
        if (!assign(output_file, "output-file")) {
                std::cerr << "Error: missing required output file name " << std::endl;
                throw Exit(EXIT_FAILURE);
        }
        parse_keyvals(additional_options, "attr");
}
