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
#include <iostream>
#include <signal.h>

#include <boost/scoped_ptr.hpp>

#include "jill/jack_client.hh"
#include "jill/options.hh"
#include "jill/logger.hh"
#include "jill/ringbuffer.hh"
#include "jill/midi.hh"
#include "jill/util/string.hh"
#include "jill/sndfile/arf_sndfile.hh"
using namespace jill;

boost::scoped_ptr<JackClient> client;
std::map<std::string, jack_port_t*> ports_in;
jack_port_t *port_trig;
event_t trig_event;
// boost::shared_ptr<TriggeredWriter> twriter;
logstream logv;
int ret = EXIT_SUCCESS;

int
process(JackClient *client, nframes_t nframes, nframes_t time)
{
        nframes_t nevents, i;
        void *trigbuf = client->events(port_trig, nframes);
        if (trigbuf) {
                nevents = jack_midi_get_event_count(trigbuf);
                for (i = 0; i < nevents; ++i) {
                        trig_event.set(trigbuf, i); // copies
                        std::cout << trig_event << std::endl;
                }
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

/* commandline options */
class CaptureOptions : public Options {

public:
	CaptureOptions(std::string const &program_name, std::string const &program_version)
		: Options(program_name, program_version) {

                using std::string;
                using std::vector;

                po::options_description jillopts("JILL options");
                jillopts.add_options()
                        ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                         "set client name")
                        ("log,l",     po::value<string>(&logfile), "set logfile (default stdout)")
                        ("in,i",      po::value<vector<string> >(&input_ports), "add connection to input port")
                        ("trig,t",     po::value<vector<string> >(&trig_ports), "add connection to trigger port")
                        ("attr,a",     po::value<vector<string> >(), "set additional attribute (key=value)");
                cfg_opts.add_options()
                        ("attr,a",     po::value<vector<string> >());
                cmd_opts.add(jillopts);
                visible_opts.add(jillopts);

		po::options_description tropts("Capture options");
		tropts.add_options()
			("prebuffer", po::value<float>(&prebuffer_size_ms)->default_value(1000),
			 "set prebuffer size (ms)")
			("buffer", po::value<float>(&buffer_size_ms)->default_value(2000),
			 "set file output buffer size (ms)")
			("max-size", po::value<int>(&max_size)->default_value(100),
			 "save data to serially numbered files with max size (MB)");

		// we add our group of options to various groups
		// already defined in the base class.
		cmd_opts.add(tropts);
		// cfg_opts holds options that are parsed from a configuration file
		cfg_opts.add(tropts);
		// visible_opts holds options that are shown in the help text
		visible_opts.add(tropts);
		// the output template is not added to the visible
		// options, since it's specified positionally.
		cmd_opts.add_options()
			("output-file", po::value<std::string>(), "output filename");
		pos_opts.add("output-file", -1);
	}

	/*
	 * The public member variables are filled when we parse the
	 * options, and subsequently accessed by external clients
	 */
	/** The client name (used in internal JACK representations) */
	std::string client_name;
	/** The log file to write application events to */
	std::string logfile;

	/** A vector of inputs to connect to the client */
	std::vector<std::string> input_ports;
	/** A vector of outputs to connect to the client */
	std::vector<std::string> trig_ports;

        std::map<std::string, std::string> additional_options;

	std::string output_file;
	float prebuffer_size_ms;  // in ms
	float buffer_size_ms;  // in ms
	nframes_t prebuffer_size; // samples
	nframes_t buffer_size; // samples
	int max_size;  // in MB

	void adjust_values(nframes_t samplerate) {
		prebuffer_size = (nframes_t)(prebuffer_size_ms * samplerate / 1000);
		buffer_size = (nframes_t)(buffer_size_ms * samplerate / 1000);
	}


protected:

	/*
	 * Here we override the base class's print_usage command to add some additional information
	 * about specifying the output filename template and the configuration file name
	 */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
			  << visible_opts << std::endl
			  << "Ports:\n"
			  << " * in_NN:      for input of the signal(s) to be recorded\n"
			  << " * trig_in:    MIDI port to receive events triggering recording"
			  << std::endl;
	}

	virtual void process_options() {
		Options::process_options();
		if (!assign(output_file, "output-file")) {
			std::cerr << "Error: missing required output file name " << std::endl;
			throw Exit(EXIT_FAILURE);
		}
                keyvals(additional_options, "attr");
	}

};

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		CaptureOptions options("jill_capture", "1.2.0rc1");
		options.parse(argc,argv);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client.reset(new JackClient(options.client_name));
		logv.set_program(client->name());
		logv.set_stream(options.logfile);
		logv << logv.allfields << "Created client" << endl;

                /* create ports, one for each input */
		vector<string>::const_iterator it;
                int i = 0;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it, ++i) {
                        jack_port_t *port_in = client->register_port(util::make_string() << "in_" << i,
                                                                     JACK_DEFAULT_AUDIO_TYPE,
                                                                     JackPortIsInput | JackPortIsTerminal, 0);
                        ports_in[*it] = port_in;
		}
                port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                  JackPortIsInput | JackPortIsTerminal, 0);

		/* Initialize writer object and triggered writer */
		options.adjust_values(client->samplerate());
		// jill::util::ArfSndfile writer(options.output_file, client->samplerate(), options.max_size);
		// twriter.reset(new TriggeredWriter(writer, logv, options.prebuffer_size,
		// 				  options.buffer_size, client->samplerate(),
		// 				  options.trig_threshold,
		// 				  &options.additional_options));

		/* Log parameters */
		if (options.max_size > 0)
			logv << logv.program << "output file base: " << options.output_file << endl
			     << logv.program << "max file size (MB): " << options.max_size << endl;
		else
			logv << logv.program << "output file name: " << options.output_file << endl;
		logv << logv.program << "sampling rate (Hz): " << client->samplerate() << endl
		     << logv.program << "buffer size (samples): " << options.buffer_size << endl
		     << logv.program << "prebuffer size (samples): " << options.prebuffer_size << endl;
		logv << logv.program << "additional metadata:" << endl;
		for (map<string,string>::const_iterator it = options.additional_options.begin();
		     it != options.additional_options.end(); ++it)
			logv << logv.program << "  " << it->first << "=" << it->second << endl;

		/* Initialize the client. */
                client->set_process_callback(process);
                client->activate();
		logv << logv.allfields << "Activated client" << endl;

		/* connect ports */

		for (map<string,jack_port_t*>::const_iterator it = ports_in.begin(); it != ports_in.end(); ++it) {
			client->connect_port(it->first, jack_port_name(it->second));
			logv << logv.allfields << "Connected " << it->first
                             << " to " << jack_port_name(it->second) << endl;
		}

		for (vector<string>::const_iterator it = options.trig_ports.begin(); it != options.trig_ports.end(); ++it) {
			client->connect_port(*it, "trig_in");
			logv << logv.allfields << "Connected " << *it << " to trigger port" << endl;
		}

		// client->set_process_callback(boost::ref(*twriter));
		// client->set_mainloop_callback(mainloop);
		// client->run();
		// logv << logv.allfields << client->get_status() << endl;
                while (1) {
                        sleep(1);
                }
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
		cerr << "Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
