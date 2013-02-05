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
                        ("input-list,I", po::value<string>(&port_list_file),
                         "list of ports in file with three columns: name, source, arf datatype")

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
			("max-size", po::value<int>(&max_size_mb)->default_value(100),
			 "save data to serially numbered files with max size (MB)");

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

        /* the option values */

	/** The client name (used in internal JACK representations) */
        std::string client_name;

	/** Vectors of inputs to connect to the client */
        std::vector<std::string> input_ports;
        std::vector<std::string> trig_ports;

        /** key-value pairs to store as attributes in created entries */
        std::map<std::string, std::string> additional_options;

        std::string output_file;
        std::string port_list_file;
	float prebuffer_size_s;
	float buffer_size_s;
	int max_size_mb;
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
			  << " * in_NNN:     for input of the signal(s) to be recorded\n"
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


/**
 * Registers ports and stores information about them. On startup, jrecord needs
 * to register input ports for the trigger line and for each additional input to
 * be recorded. Sampled data is stored in dense arrays, and event data can be
 * stored either as an array of times (e.g., for spikes) or as a table.
 *
 */


CaptureOptions options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<dsp::period_ringbuffer> ringbuffer;
boost::scoped_ptr<jill::file::arffile> outfile;
port_registry ports;
jack_port_t *port_trig;

/*
 * Notes on synchronization
 *
 * During normal operation, the disk thread will read data from the ringbuffer
 * and write it to the current entry (or the prebuffer, if not in continuous
 * mode). It will lock disk_thread_lock while running, and wait on data_ready
 * after all available data has been handled.
 */

pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;
long xruns = 0;


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
        // Reserve space in the ringbuffer. If there's not enough, (1) wait for
        // more, potentially causing xruns in the whole executation graph, or
        // (2) discard the data, even though we might have been able to save it
        // by waiting a few cycles...
        if (ringbuffer->reserve(time, nframes, client->ports().size()) == 0) {
                // increment xrun counter
                return 0;       // discards data
        }
        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                port_buffer = client->samples(*it, nframes);
                ringbuffer->push(port_buffer);
        }

	/* signal that there is data available */
	// if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
	//     pthread_cond_signal (&data_ready);
	//     pthread_mutex_unlock (&disk_thread_lock);
	// }

        return 0;
}

/*
 * Write data to arf file in continous mode.
 *
 * @pre client is started and ports registered
 * @pre outfile is initialized
 */
// void *
// write_continuous(void * arg)
// {
//         dsp::period_ringbuffer::period_info_t const * period;
//         std::size_t my_xruns = 0;                       // internal counter
//         std::size_t entry_count = outfile->file()->root()->nchildren();
//         arf::entry_ptr entry;
//         boost::format fmt("entry_%|06|");
//         jack_time_t usec;
//         timeval tp;

// 	// pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
// 	// pthread_mutex_lock (&disk_thread_lock);

//         while (1) {
//                 period = ringbuffer->request();
//                 if (period) {
//                         client->log() << period->size() << " bytes: "
//                                       << period->nchannels << "x" << period->nbytes << std::endl;
//                         // check for valid entry
//                         // if check_filesize() is true, entry is invalid
//                         if (outfile->check_filesize() || !entry) {
//                                 usec = client->time(period->time);
//                                 gettimeofday(&tp);
//                                 entry = outfile->new_entry(boost::str(fmt % entry_count), &tp, usec);
//                                 entry->set_attribute("jack_usec", usec);
//                                 for (std::map<std::string,std::string>::const_iterator it = options.additional_options.begin();
//                                      it != options.additional_options.end(); ++it) {
//                                         entry->set_attribute(it->first, it->second);
//                                 }
//                                 // create datasets for each channel

//                                 entry_count += 1;
//                         }
//                         ringbuffer->pop_all(0);
//                         for (std::size_t chan = 0; chan < period->nchannels; ++chan) {
//                                 // look up channel name and type
//                                 // write data
//                         }
//                         // check for overrun & disconnected port_trig; close/mark entry
//                 }
//                 else {
//                         // wait on data_ready
//                         // pthread_cond_wait (&data_ready, &disk_thread_lock);
//                 }
//         }

// 	// pthread_mutex_unlock (&disk_thread_lock);
//         return 0;
// }

int
jack_xrun(jack_client *client, float delay)
{
        // increment xrun counter
        return 0;
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        // TODO flush all data in ringbuffer

        // reallocate ringbuffer if necessary - 10 periods or 2x seconds
        nframes_t sz1 = nframes * ports.size() * 10;
        nframes_t sz2 = client->samplerate() * ports.size() * options.buffer_size_s;
        if (sz1 < sz2) sz1 = sz2;
        if (!ringbuffer || ringbuffer->size() < sz1) {
                ringbuffer.reset(new dsp::period_ringbuffer(sz1));
                client->log() << "ringbuffer size (s): "
                              << (ringbuffer->write_space() /
                                  ports.size() / client->samplerate())
                              << std::endl;
        }
        //      << logv.program << "prebuffer size (ms): " << options.prebuffer_size_ms << endl;
        return 0;
}


void
jack_portcon(jack_client *client, jack_port_t* port1, jack_port_t* port2, int connected)
{
        if (!outfile) return;
        util::make_string msg;
        msg << "IIII Ports " << ((connected) ? "" : "dis") << "connected: "
            << jack_port_name(port1) << " -> " << jack_port_name(port2) << std::endl;
        // TODO lock disk mutex?
        outfile->file()->log(msg);
}

void
jack_shutdown(jack_status_t code, char const *)
{
        // terminate_program = true;
}


void
signal_handler(int sig)
{
        // terminate_program = true;
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                cout << "[" << options.client_name << "] " <<  PROGRAM_NAME ", version " PROGRAM_VERSION << endl;

                // initialize client
                client.reset(new jack_client(options.client_name));

                // open output file
                outfile.reset(new jill::file::arffile(options.output_file, options.max_size_mb));
                outfile->file()->write_attribute("creator", PROGRAM_NAME ": " PROGRAM_VERSION);
                std::ostream& os = client->log() << "opened output file " << outfile->file()->name();
                if (options.max_size_mb)
                        os << " (max size: " << options.max_size_mb << " MB)";
                os << endl;

		// logv << logv.program << "additional metadata:" << endl;

                /* create ports: one for trigger, and one for each input */
                port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                  JackPortIsInput | JackPortIsTerminal, 0);
                ports.add(port_trig, arf::INTERVAL);
                ports.add(client.get(), options.input_ports.begin(), options.input_ports.end());
                std::ifstream ff(options.port_list_file.c_str());
                ports.add_from_file(client.get(), ff);

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                // register callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_process_callback(process);
                client->set_buffer_size_callback(jack_bufsize);

                // start disk thread

                // activate process callback
                client->activate();

		/* connect ports */
                vector<string>::const_iterator it;
		for (it = options.trig_ports.begin(); it != options.trig_ports.end(); ++it) {
			client->connect_port(*it, "trig_in");
		}
                ports.connect_all(client.get()); // connects input ports

                sleep(1000);
                return 0;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
                cerr << "Error: " << e.what() << endl;
                if (outfile) outfile->file()->log(util::make_string() << "EEEE " << e.what());
		return EXIT_FAILURE;
	}
}
