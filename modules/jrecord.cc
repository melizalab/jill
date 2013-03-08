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
#include <fstream>
#include <signal.h>
#include <boost/scoped_ptr.hpp>
#include <boost/format.hpp>
#include <sys/time.h>

#include "jill/jack_client.hh"
#include "jill/options.hh"
#include "jill/midi.hh"
#include "jill/dsp/period_ringbuffer.hh"
#include "jill/util/string.hh"
#include "jill/util/portreg.hh"
#include "jill/file/arf_thread.hh"

#define PROGRAM_NAME "jrecord"
#define PROGRAM_VERSION "1.3.0"

using namespace jill;

/* declare options parsing class */
class CaptureOptions : public Options {

public:
	CaptureOptions(std::string const &program_name, std::string const &program_version);

	/** The client name (used in internal JACK representations) */
        std::string client_name;

	/** Vectors of inputs to connect to the client */
        std::vector<std::string> input_ports;
        std::vector<std::string> trig_ports;

        /** key-value pairs to store as attributes in created entries */
        std::map<std::string, std::string> additional_options;

        std::string output_file;
	float prebuffer_size_s;
	float buffer_size_s;
	int max_size_mb;
        int n_input_ports;
        int compression;

protected:

	virtual void print_usage();
	virtual void process_options();

};


/**
 * Registers ports and stores information about them. On startup, jrecord needs
 * to register input ports for the trigger line and for each additional input to
 * be recorded. Sampled data is stored in dense arrays, and event data can be
 * stored either as an array of times (e.g., for spikes) or as a table.
 *
 */


CaptureOptions options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<jack_client> client;
dsp::period_ringbuffer ringbuffer(1024); // this may be resized
boost::scoped_ptr<file::arf_thread> arf_thread;
jack_port_t *port_trig;
// has to be a global variable to track port creation
util::port_registry ports;

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
        std::list<jack_port_t*>::const_iterator it;
        // Reserve space in the ringbuffer. If there's not enough, increase the
        // xrun count and discard the data (even though we might have been able
        // to save it by waiting a few cycles)
        if (ringbuffer.reserve(time, nframes, client->ports().size()) == 0) {
                arf_thread->xrun();
                return 0;
        }
        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                port_buffer = client->samples(*it, nframes);
                ringbuffer.push(port_buffer);
        }

	/* signal that there is data available */
	if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
	    pthread_cond_signal (&data_ready);
	    pthread_mutex_unlock (&disk_thread_lock);
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
        // TODO flush all data in ringbuffer

        // reallocate ringbuffer if necessary - 10 periods or 2x seconds
        nframes_t sz1 = nframes * client->nports() * 10;
        nframes_t sz2 = client->sampling_rate() * client->nports() * options.buffer_size_s;
        if (sz1 < sz2) sz1 = sz2;
        if (ringbuffer.size() < sz1) {
                ringbuffer.resize(sz1);
        }
        client->log() << "ringbuffer size (s): "
                      << (sz1 / client->sampling_rate())
                      << std::endl;
        return 0;
}


void
jack_portcon(jack_client *client, jack_port_t* port1, jack_port_t* port2, int connected)
{
        util::make_string msg;
        msg << "[" PROGRAM_NAME "] port " << ((connected) ? "" : "dis") << "connect: "
            << jack_port_name(port1) << " -> " << jack_port_short_name(port2);
        // this can cause problems if it's called while the thread is cleaning
        // up (even with a nice trylock). The solution is to deactivate the
        // client (disconnecting the ports) or disable this callback before the
        // program ends.
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
        if (arf_thread) arf_thread->stop();
}

int
main(int argc, char **argv)
{
	using namespace std;

        vector<string>::const_iterator it;
	try {
		options.parse(argc,argv);
                cout << "[" << options.client_name << "] " <<  PROGRAM_NAME ", version " PROGRAM_VERSION << endl;

                // initialize client
                client.reset(new jack_client(options.client_name));

                // set up disk thread
                arf_thread.reset(new file::arf_thread(options.output_file,
                                                      &options.additional_options,
                                                      client.get(),
                                                      &ringbuffer,
                                                      options.compression));
                arf_thread->log("[" PROGRAM_NAME "] opened file for writing");
                arf_thread->log("[" PROGRAM_NAME "] version = " PROGRAM_VERSION);
                client->log() << "opened output file " << options.output_file << endl;

                /* create ports: one for trigger, and one for each input */
                port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                  JackPortIsInput | JackPortIsTerminal, 0);
                ports.add(client.get(), options.input_ports.begin(), options.input_ports.end());

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                // register callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                // client->set_process_callback(process);
                client->set_port_connect_callback(jack_portcon);
                client->set_buffer_size_callback(jack_bufsize);

                // activate process callback
                client->activate();

		/* connect ports */
		for (it = options.trig_ports.begin(); it != options.trig_ports.end(); ++it) {
			client->connect_port(*it, "trig_in");
		}
                ports.connect_all(client.get()); // connects input ports

                // arf_thread->start();
                // arf_thread->join();

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


/** implementation of CaptureOptions */
CaptureOptions::CaptureOptions(std::string const &program_name, std::string const &program_version)
        : Options(program_name, program_version)
{

        using std::string;
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("name,n",     po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("n",          po::value<int>(&n_input_ports)->default_value(0), "number of input ports to create")
                ("in,i",       po::value<vector<string> >(&input_ports), "connect to input port")
                ("trig,t",     po::value<vector<string> >(&trig_ports), "connect to trigger port")
                ("attr,a",     po::value<vector<string> >(), "set additional attributes for recorded entries (key=value)")
                ("buffer",     po::value<float>(&buffer_size_s)->default_value(2.0),
                 "minimum ringbuffer size (s)");
        cfg_opts.add_options()
                ("attr,a",     po::value<vector<string> >());
        cmd_opts.add(jillopts);
        visible_opts.add(jillopts);

        po::options_description tropts("Capture options");
        tropts.add_options()
                ("continuous,c", "record in continuous mode (default is in triggered epochs)")
                ("prebuffer", po::value<float>(&prebuffer_size_s)->default_value(1.0),
                 "set prebuffer size (s)")
                ("compression", po::value<int>(&compression)->default_value(0),
                 "set compression in output file (0-9)");

        // command-line options
        cmd_opts.add(tropts);
        // configuration file options
        cfg_opts.add(tropts);
        // options shown in help text
        visible_opts.add(tropts);
        // the output template is not added to the visible
        // options, since it's specified positionally.
        cmd_opts.add_options()
                ("output-file", po::value<std::string>(), "output filename");
        pos_opts.add("output-file", -1);
}


void
CaptureOptions::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in_NNN:     for input of the signal(s) to be recorded\n"
                  << " * trig_in:    MIDI port to receive events triggering recording"
                  << std::endl;
}

void
CaptureOptions::process_options()
{
        Options::process_options();
        if (!assign(output_file, "output-file")) {
                std::cerr << "Error: missing required output file name " << std::endl;
                throw Exit(EXIT_FAILURE);
        }
        parse_keyvals(additional_options, "attr");
}
