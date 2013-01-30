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
#include "jill/midi.hh"
#include "jill/dsp/period_ringbuffer.hh"
#include "jill/util/string.hh"
#include "jill/file/arffile.hh"

#define PROGRAM_NAME "jrecord"
#define PROGRAM_VERSION "1.3.0"

using namespace jill;

/* commandline options */
class CaptureOptions : public Options {

public:
	CaptureOptions(std::string const &program_name, std::string const &program_version)
		: Options(program_name, program_version) {

                using std::string;
                using std::vector;

                po::options_description jillopts("JILL options");
                jillopts.add_options()
                        ("name,n",     po::value<string>(&client_name)->default_value(_program_name),
                         "set client name")
                        ("n",          po::value<int>(&n_input_ports)->default_value(0), "number of input ports to create")
                        ("in,i",       po::value<vector<string> >(&input_ports), "connect to input port")
                        ("event,e",    po::value<vector<string> >(&event_ports), "connect to input port (store full event information)")
                        ("trig,t",     po::value<vector<string> >(&trig_ports), "connect to trigger port")
                        ("attr,a",     po::value<vector<string> >(), "set additional attributes for recorded entries (key=value)");
                cfg_opts.add_options()
                        ("attr,a",     po::value<vector<string> >())
			("buffer",     po::value<float>(&buffer_size_ms)->default_value(2000),
			 "minimum ringbuffer size (ms)");
                cmd_opts.add(jillopts);
                visible_opts.add(jillopts);

		po::options_description tropts("Capture options");
		tropts.add_options()
                        ("continuous,c", "record in continuous mode (default is in triggered epochs)")
			("prebuffer", po::value<float>(&prebuffer_size_ms)->default_value(1000),
			 "set prebuffer size (ms)")
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

	/** Vectors of inputs to connect to the client */
	std::vector<std::string> input_ports;
	std::vector<std::string> event_ports;
	std::vector<std::string> trig_ports;

        std::map<std::string, std::string> additional_options;

	std::string output_file;
	float prebuffer_size_ms;  // in ms
	float buffer_size_ms;  // in ms
	int max_size;  // in MB
        int n_input_ports;


protected:

	/*
	 * Here we override the base class's print_usage command to add some additional information
	 * about specifying the output filename template and the configuration file name
	 */
	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
			  << visible_opts << std::endl
			  << "Ports:\n"
			  << " * in_NNN:      for input of the signal(s) to be recorded\n"
			  << " * trig_in:    MIDI port to receive events triggering recording"
			  << std::endl;
	}

	virtual void process_options() {
		Options::process_options();
		if (!assign(output_file, "output-file")) {
			std::cerr << "Error: missing required output file name " << std::endl;
			throw Exit(EXIT_FAILURE);
		}
                parse_keyvals(additional_options, "attr");
	}

};

CaptureOptions options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<JackClient> client;
std::map<std::string, jack_port_t*> ports_in;
jack_port_t *port_trig;
boost::scoped_ptr<dsp::period_ringbuffer> ringbuffer;
boost::scoped_ptr<jill::file::arffile> outfile;

/* locks and condition variables used to synchronize during various events */
// pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
// pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;
bool terminate_program = false;
bool terminate_entry = false;


/*
 * Copy data from ports into ringbuffer. Note that the first port is the trigger
 * port, so it contains MIDI data; however, the buffer size is the same so it
 * can be treated like audio data for now (should probably assert this
 * somewhere)
 */
int
process(JackClient *client, nframes_t nframes, nframes_t time)
{
        sample_t *port_buffer;
        std::list<jack_port_t*>::const_iterator it;
        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                port_buffer = client->samples(*it, nframes);
        }
        return 0;
}

int
jack_xrun(JackClient *client, float delay)
{
        return 0;
}

int
jack_bufsize(JackClient *client, nframes_t nframes)
{
        // terminate entry; reallocate ringbuffer if needed
        // logv << logv.program << "buffer size (ms): " << options.buffer_size_ms << endl
        //      << logv.program << "prebuffer size (ms): " << options.prebuffer_size_ms << endl;
        return 0;
}


void
jack_portcon(JackClient *client, jack_port_t* port1, jack_port_t* port2, int connected)
{
        // deal with loss of trigger client while recording
        //client->log() <<
}

void
jack_shutdown(jack_status_t code, char const *)
{
        terminate_program = true;
}


void
signal_handler(int sig)
{
        terminate_program = true;
}

struct register_ports {
        register_ports() : i(0) {}
        void operator() (std::string const & src) {
                jack_port_t *p = client->get_port(src);
                if (p) {
                        ports_in[src] = client->register_port(util::make_string() << "in_" << i,
                                                              jack_port_type(p),
                                                              JackPortIsInput | JackPortIsTerminal, 0);
                        ++i;
                }
                else {
                        client->log(false) << "warning: " << src << " does not exist" << std::endl;
                }
        }
        std::size_t i;
};

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                cout << "[" << options.client_name << "] " <<  PROGRAM_NAME ", version " PROGRAM_VERSION << endl;
                client.reset(new JackClient(options.client_name));

                outfile.reset(new jill::file::arffile(options.output_file, options.max_size));
                outfile->file()->write_attribute("creator", PROGRAM_NAME ": " PROGRAM_VERSION);
                std::ostream& os = client->log() << "opened output file " << outfile->file()->name();
                if (options.max_size)
                        os << " ( max size : " << options.max_size << " MB)";
                os << endl;
		// logv << logv.program << "additional metadata:" << endl;
		// for (map<string,string>::const_iterator it = options.additional_options.begin();
		//      it != options.additional_options.end(); ++it) {
		// 	logv << logv.program << "  " << it->first << "=" << it->second << endl;
                // }

                /* create ports: one for trigger, and one for each input */
                port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                  JackPortIsInput | JackPortIsTerminal, 0);
                register_ports rp;
                for_each(options.input_ports.begin(), options.input_ports.end(), rp);
                for_each(options.event_ports.begin(), options.event_ports.end(), rp);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_process_callback(process);
                client->activate();

		/* connect ports */
		for (map<string,jack_port_t*>::const_iterator it = ports_in.begin(); it != ports_in.end(); ++it) {
			client->connect_port(it->first, jack_port_name(it->second));
		}

		for (vector<string>::const_iterator it = options.trig_ports.begin(); it != options.trig_ports.end(); ++it) {
			client->connect_port(*it, "trig_in");
		}

                while (!terminate_program) {
                        sleep(1);
                }
                return 0;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
		cerr << "Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}
