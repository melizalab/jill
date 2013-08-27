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
#include <boost/shared_ptr.hpp>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/arf_writer.hh"
#include "jill/dsp/buffered_data_writer.hh"
#include "jill/dsp/triggered_data_writer.hh"

#define PROGRAM_NAME "jrecord"

using namespace jill;
using std::string;
typedef std::vector<string> svec;

/* declare options parsing class */
class jrecord_options : public program_options {

public:
	jrecord_options(string const &program_name);

        string server_name;
        string client_name;

        /** key-value pairs to store as attributes in created entries */
        std::map<string, string> additional_options;

        string output_file;
	float pretrigger_size_s;
	float posttrigger_size_s;
	float buffer_size_s;
	int max_size_mb;
        int compression;

protected:

	virtual void print_usage();
	virtual void process_options();

};

jrecord_options options(PROGRAM_NAME);
boost::shared_ptr<jack_client> client;
boost::shared_ptr<dsp::buffered_data_writer> arf_thread;
jack_port_t * port_trig = 0;


int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        jack_port_t *port;
        void *buffer;
        jack_client::port_list_type::const_iterator it;

        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                port = *it;
                buffer = jack_port_get_buffer(port, nframes);
                if (buffer == 0) continue;
                if (strcmp(jack_port_type(port), JACK_DEFAULT_AUDIO_TYPE) == 0) {
                        arf_thread->push(time, SAMPLED, jack_port_short_name(port),
                                         nframes * sizeof(sample_t), buffer);
                }
                else {
                        jack_midi_event_t event;
                        nframes_t nevents = jack_midi_get_event_count(buffer);
                        for (nframes_t j = 0; j < nevents; ++j) {
                                jack_midi_event_get(&event, buffer, j);
                                if (event.size == 0) continue;
                                arf_thread->push(time + event.time, EVENT, jack_port_short_name(port),
                                                 event.size, event.buffer);
                        }
                }
        }
        arf_thread->data_ready();

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
        std::size_t bytes = client->sampling_rate() * options.buffer_size_s * client->nports();
        if (port_trig != 0)
                bytes += client->sampling_rate() * options.pretrigger_size_s * client->nports();
        // will block until buffer is empty
        bytes = arf_thread->request_buffer_size(bytes * sizeof(sample_t));
        arf_thread->reset();
        LOG << "ringbuffer size (bytes): " << bytes;
        return 0;
}


void
jack_portcon(jack_client *client, jack_port_t* port1, jack_port_t* port2, int connected)
{
        /* determine if the last connection to the trigger port was cut */
        if (port_trig == 0 || connected || strcmp(jack_port_name(port2), jack_port_name(port_trig)) != 0)
                return;
        const char ** connections = jack_port_get_connections(port_trig);
        // can't directly compare port addresses
        if (connections) {
                jack_free(connections);
        }
        else {
                // close current entry
                INFO << "last input to trigger port disconnected";
                arf_thread->reset();
        }
}


void
jack_shutdown(jack_status_t code, char const * msg)
{
        LOG << "jackd shut the client down (" << msg << ")";
        if (arf_thread) {
                arf_thread->stop();
        }
}


void
signal_handler(int sig)
{
        if (arf_thread) {
                arf_thread->stop();
        }
}


int
main(int argc, char **argv)
{
	using namespace std;
        typedef svec::const_iterator svec_iterator;
        int ret = 0;
        map<string,string> port_connections;
        boost::shared_ptr<data_writer> writer;
	try {
		options.parse(argc,argv);
                client.reset(new jack_client(options.client_name, options.server_name));
                writer.reset(new file::arf_writer(options.output_file,
                                                  *client,
                                                  options.additional_options,
                                                  options.compression));

                /* create ports: one for trigger, and one for each input */
                if (options.count("trig")) {
                        LOG << "recordings will be triggered";
                        port_trig = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                          JackPortIsInput | JackPortIsTerminal, 0);
                        arf_thread.reset(new dsp::triggered_data_writer(
                                                 writer,
                                                 jack_port_short_name(port_trig),
                                                 options.pretrigger_size_s * client->sampling_rate(),
                                                 options.posttrigger_size_s * client->sampling_rate()));
                }
                else {
                        LOG << "recording will be continuous";
                        arf_thread.reset(new dsp::buffered_data_writer(writer));
                }
                /* bind socket for storing messages in arf file */
                arf_thread->bind_logger(options.server_name);

                /* register input ports */
                if (options.count("in")) {
                        int name_index = 0;
                        svec const & plist = options.vmap["in"].as<svec>();
                        for ( svec_iterator it = plist.begin(); it!= plist.end(); ++it) {
                                jack_port_t *p = client->get_port(*it);
                                if (p==0) {
                                        LOG << "error registering port: source port \""
                                                      << *it << "\" does not exist";
                                        throw Exit(-1);
                                }
                                else if (!(jack_port_flags(p) & JackPortIsOutput)) {
                                        LOG << "error registering port: source port \""
                                                      << *it << "\" is not an output port";
                                        throw Exit(-1);
                                }
                                else {
                                        char buf[16];
                                        if (strcmp(jack_port_type(p),JACK_DEFAULT_AUDIO_TYPE)==0)
                                                sprintf(buf,"pcm_%03d",name_index);
                                        else
                                                sprintf(buf,"evt_%03d",name_index);
                                        LOG << "startup connection: " << *it << " -> " << buf;
                                        name_index++;
                                        client->register_port(buf, jack_port_type(p),
                                                              JackPortIsInput | JackPortIsTerminal, 0);
                                        port_connections[buf] = *it;
                                }
                        }
                }

                if (options.count("in-pcm")) {
                        svec const & plist = options.vmap["in-pcm"].as<svec>();
                        client->register_ports(plist.begin(), plist.end(), JACK_DEFAULT_AUDIO_TYPE,
                                               JackPortIsInput | JackPortIsTerminal, 0);
                }

                if (options.count("in-evt")) {
                        svec const & plist = options.vmap["in-evt"].as<svec>();
                        client->register_ports(plist.begin(), plist.end(), JACK_DEFAULT_MIDI_TYPE,
                                               JackPortIsInput | JackPortIsTerminal, 0);
                }

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                // register callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_port_connect_callback(jack_portcon);
                client->set_process_callback(process);
                client->set_buffer_size_callback(jack_bufsize);

                // start disk thread and activate process callback
                client->activate();
                arf_thread->start();

		/* connect ports */
                if (options.count("trig")) {
                        svec const & plist = options.vmap["trig"].as<svec>();
                        client->connect_ports(plist.begin(), plist.end(), "trig_in");
                }
                for (map<string,string>::const_iterator it = port_connections.begin();
                     it != port_connections.end(); ++it) {
                        if (!it->second.empty()) client->connect_port(it->second, it->first);
                }

                arf_thread->join();

	}
	catch (Exit const &e) {
		ret = e.status();
	}
	catch (exception const &e) {
                LOG << "ERROR: " << e.what();
		ret = EXIT_FAILURE;
	}

        // manually deactivating the client ensures shutdown events get logged
        if (client) client->deactivate();
        return ret;
}


/** implementation of jrecord_options */
jrecord_options::jrecord_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name)->default_value("default"),
                 "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<svec>(),
                 "connect to an input port: must be the name of an existing port")
                ("in-pcm,I",  po::value<svec>(), "create an input port for sampled data")
                ("in-evt,E",  po::value<svec>(), "create an input port for event data")
                ("trig,t",    po::value<svec>()->multitoken()->zero_tokens(),
                 "record in triggered mode (optionally specify inputs)")
                ("buffer",     po::value<float>(&buffer_size_s)->default_value(2.0),
                 "minimum ringbuffer size (s)");

        po::options_description tropts("Capture options");
        tropts.add_options()
                ("attr,a",     po::value<svec>(),
                 "set additional attributes for recorded entries (key=value)")
                ("pretrigger", po::value<float>(&pretrigger_size_s)->default_value(1.0),
                 "duration to record before onset trigger (s)")
                ("posttrigger", po::value<float>(&posttrigger_size_s)->default_value(0.5),
                 "duration to record after offset trigger (s)")
                ("compression", po::value<int>(&compression)->default_value(0),
                 "set compression in output file (0-9)");

        // command-line options
        cmd_opts.add(jillopts).add(tropts);
        cmd_opts.add_options()
                ("output-file,f", po::value<string>(), "output filename");
        pos_opts.add("output-file", -1);
        visible_opts.add(jillopts).add(tropts);
}


void
jrecord_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [output-file]\n"
                  << visible_opts << std::endl
                  << "Ports (all are recorded):\n"
                  << " * pcm_NNN:    sampled input ports\n"
                  << " * evt_NNN:    event input ports\n"
                  << " * trig_in:    MIDI port to receive events triggering recording"
                  << std::endl;
}


void
jrecord_options::process_options()
{
        program_options::process_options();
        if (!assign(output_file, "output-file")) {
                LOG << "ERROR: missing required output file name " << std::endl;
                throw Exit(EXIT_FAILURE);
        }
        parse_keyvals(additional_options, "attr");
}
