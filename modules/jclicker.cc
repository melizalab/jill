/*
 * A simple module that transforms events into clicks.
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <csignal>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/midi.hh"
#include "jill/program_options.hh"

#define PROGRAM_NAME "jclicker"

using namespace jill;
using std::string;
using stringvec = std::vector<string>;

class jclicker_options : public program_options {

public:
        jclicker_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        bool click_onset;
        bool click_offset;

protected:

        void print_usage() override;

}; // jclicker_options

static jclicker_options options(PROGRAM_NAME);
jack_port_t *port_in, *port_out;
static int ret = EXIT_SUCCESS;
static int running = 1;

int
process(jack_client *client, nframes_t nframes, nframes_t)
{
        void *in = client->events(port_in, nframes);
        sample_t *out = client->samples(port_out, nframes);

        memset(out, 0, nframes * sizeof(sample_t));

        jack_midi_event_t event;
        nframes_t nevents = jack_midi_get_event_count(in);
        for (nframes_t i = 0; i < nevents; ++i) {
                jack_midi_event_get(&event, in, i);
                if (event.size < 1) continue;
                midi::data_type t = event.buffer[0] & midi::type_nib;
                switch(t) {
                case midi::stim_on:
                case midi::note_on:
                        if (options.count("no-onset")==0) {
                                out[event.time] = options.click_onset * 1.0f;
                        }
                        break;
                case midi::stim_off:
                case midi::note_off:
                        if (options.count("no-offset")==0) {
                                out[event.time] = options.click_offset * 1.0f;
                        }
                default:
                        break;
                }
        }

        // this just copies data from input to output - replace with something
        // more interesting

        return 0;
}

/** handle server shutdowns */
void
jack_shutdown(jack_status_t code, char const *)
{
        ret = -1;
        running = 0;
}

/** handle POSIX signals */
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
                // parse options
                options.parse(argc,argv);

                // start client
                auto client = std::make_unique<jack_client>(options.client_name,
                                                            options.server_name);

                // register ports
                port_in = client->register_port("in",JACK_DEFAULT_MIDI_TYPE,
                                                JackPortIsInput, 0);
                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                // register jack callbacks
                client->set_shutdown_callback(jack_shutdown);
                client->set_process_callback(process);

                // activate client
                client->activate();

                // connect ports
                if (options.count("in")) {
                        auto const & portlist = options.vmap["in"].as<stringvec>();
                        client->connect_ports(portlist.begin(), portlist.end(), "in");
                }
                if (options.count("out")) {
                        auto const & portlist = options.vmap["out"].as<stringvec>();
                        client->connect_ports("out", portlist.begin(), portlist.end());
                }

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
                LOG  << "ERROR: " << e.what();
                return EXIT_FAILURE;
        }

}

/** configure commandline options */
jclicker_options::jclicker_options(string const &program_name)
        : program_options(program_name)
{

        // this section is for general JILL options. try to maintain consistency
        // with other modules
        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<stringvec>(), "add connection to input port")
                ("out,o",     po::value<stringvec>(), "add connection to output port");
        cmd_opts.add(jillopts);
        visible_opts.add(jillopts);


        // add section(s) for module-specific options
        po::options_description opts("jclicker options");
        opts.add_options()
                ("no-onset", "don't generate clicks for onset events")
                ("no-offset", "don't generate clicks for offset events");

        cmd_opts.add(opts);
        visible_opts.add(opts);
}

/** provide the user with some information about the ports */
void
jclicker_options::print_usage()
{
        std::cout << _program_name << ": generate audible clicks for events\n\n"
                  << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:        input event port\n"
                  << " * out:       output audio port\n"
                  << std::endl;
}
