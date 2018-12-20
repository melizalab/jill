/*
 * Provides stimulus playback in response to requests through a zmq socket
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <czmq.h>
//#include "jill/zmq.hh"
#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jstimserver"
#define REQ_STIMLIST "STIMLIST"
#define REQ_PLAYSTIM "PLAY"
#define REP_BADCMD "BADCMD"
#define REP_BADSTIM "BADSTIM"
#define REP_OK "OK"

using namespace jill;
namespace fs = boost::filesystem;
using std::string;

class jstim_options : public program_options {

public:
        jstim_options(string const &program_name);

        string server_name;
        string client_name;

        /** Ports to connect to */
        std::vector<string> output_ports;
        std::vector<string> trigout_ports;
        midi::data_type trigout_chan;

        std::vector<string> stimuli; // this is postprocessed

protected:

        virtual void print_usage();

}; // jstim_options


jstim_options options(PROGRAM_NAME);
boost::shared_ptr<jack_client> client;
boost::ptr_map<std::string, stimulus_t> _stimuli;
// current stimulus. Use __sync_bool_compare_and_swap to set from main thread
stimulus_t const * _stim = 0;
// mutex/cond to signal main thread that stimulus is done
pthread_mutex_t _lock;
pthread_cond_t  _ready;
// zmq variables
static int _running = 1;

jack_port_t *port_out, *port_trigout;

static int xruns = 0;                  // xrun counter

/**
 * The realtime process loop for jstim. The logic is complicated. The process
 * can be in two states:
 *
 * 1. currently playing stimulus. copy data from the stimulus buffer to the
 * output buffer, and update the static variable that tracks position in the
 * stimulus buffer. If an xrun was flagged, terminate the stimulus at the
 * beginning of the period. If the stimulus ends before the end of the period,
 * save the stop time to another static variable.
 *
 * 2. not playing stimulus. check for (a) trigger event, if we're in triggered
 * mode or (b) satisfaction of one of the minimum time constraints (interval =
 * time since last start; gap = time since last stop). Checking (b) is
 * complicated by the fact that the frame counter is unsigned and may overflow.
 * If either condition is true, copy data from the stimulus buffer (if
 * available) into the output buffer, starting with the onset time. Advance the
 * stimulus buffer tracking variable to reflect the number of samples played.
 *
 */
int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        // NB static variables are initialized to 0
        static nframes_t stim_offset; // current position in stimulus buffer
        static nframes_t last_start;  // last stimulus start time
        static nframes_t last_stop;   // last stimulus stop

        void * trig = client->events(port_trigout, nframes);
        sample_t * out = client->samples(port_out, nframes);
        // zero the output buffer - somewhat inefficient but safer
        memset(out, 0, nframes * sizeof(sample_t));

        // handle xruns
        if (xruns) {
                // playing stimuli are truncated
                if (stim_offset > 0 && _stim) {
                        stim_offset = _stim->nframes();
                }
                // reset timers because running time has changed
                last_start = last_stop = time;
                // indicate we've handled the xrun
                __sync_add_and_fetch(&xruns, -1);
        }

        // if no stimulus queued do nothing
        if (_stim == 0) return 0;

        // copy samples, if there are any
        nframes_t nsamples = std::min(_stim->nframes() - stim_offset, nframes);
        // notify monitor
        if (stim_offset == 0 && pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
        DBG << "stim_offset=" << stim_offset << ", nsamples=" << nsamples;
        if (nsamples > 0) {
                memcpy(out,
                       _stim->buffer() + stim_offset,
                       nsamples * sizeof(sample_t));
                stim_offset += nsamples;
        }
        // did the stimulus end?
        if (stim_offset >= _stim->nframes()) {
                last_stop = time + nsamples;
                midi::write_message(trig, nsamples,
                                    midi::stim_off, _stim->name());
                DBG << "playback ended: time=" << last_stop << ", _stim=" << _stim->name();
                stim_offset = 0;

                // set ptr to null once done
                _stim = 0;
                // signal main thread if it's blocked
                if (pthread_mutex_trylock (&_lock) == 0) {
                        pthread_cond_signal (&_ready);
                        pthread_mutex_unlock (&_lock);
                }
        }

        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        return 0;
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        // we use the xruns counter to notify the process thread that an
        // interruption in the audio stream has occurred
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        _running = 0;
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}

void
signal_handler(int sig)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        _running = 0;
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
}


/* parse the list of stimuli */
static std::string
init_stimset(std::vector<string> const & stims, nframes_t sampling_rate)
{
        // serialize the stimulus list here as well. It's sort of a shitty JSON
        // serializer (floats get cast to strings, etc)
        namespace pt = boost::property_tree;
        namespace fs = boost::filesystem;
        pt::ptree root;
        pt::ptree stim_list;

        for (size_t i = 0; i < stims.size(); ++i) {
                fs::path p(stims[i]);
                try {
                        jill::stimulus_t * stim = new file::stimfile(p.string());
                        std::string name(stim->name());
                        stim->load_samples(sampling_rate);
                        _stimuli.insert(name, stim);

                        pt::ptree stim_node;
                        stim_node.put("name", name);
                        //stim_node.put("duration", stim->duration());
                        stim_list.push_back(std::make_pair("", stim_node));
                }
                catch (jill::FileError const & e) {
                        LOG << "invalid stimulus " << p << ": " << e.what();
                }
        }
        root.add_child("stimuli", stim_list);
        std::stringstream ss;
        pt::write_json(ss, root, false);
        return ss.str();
}

// This thread looks for start and stop events and publishes them
static void *
stim_monitor(void * arg)
{
        // set up zeromq socket
        fs::path path("/tmp/org.meliza.jill");
        path /= options.server_name;
        path /= options.client_name;
        path /= "pub";
        std::ostringstream endpoint;
        endpoint << "ipc://" << path.string();
        zsock_t * pub_socket = zsock_new_pub(endpoint.str().c_str());
        if (pub_socket == 0) {
                LOG << "unable to bind to endpoint " << endpoint.str();
                return 0;
        }
        else {
                INFO << "publishing start/stop events at " << endpoint.str();
        }
        // notify any waiting clients that we are alive. This is a pretty
        // fragile system for doing heartbeats.
        zstr_send(pub_socket, "STARTING");
        pthread_mutex_lock(&_lock);
        while (_running) {
                pthread_cond_wait(&_ready, &_lock);
                if (_stim)
                        zstr_sendf(pub_socket, "PLAYING %s", _stim->name());
                else if (!_running)
                        zstr_send(pub_socket, "STOPPING");
                else
                        zstr_send(pub_socket, "DONE");
        }
        pthread_mutex_unlock(&_lock);
        zsock_destroy(&pub_socket);
        return 0;
}


int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                client.reset(new jack_client(options.client_name, options.server_name));
                options.trigout_chan &= midi::chan_nib;

                if (options.stimuli.size() == 0) {
                        LOG << "no stimuli; quitting";
                        throw Exit(0);
                }
                /* load the stimuli */
                std::string stimlist = init_stimset(options.stimuli, client->sampling_rate());
                DBG << "stimlist: " << stimlist;

                // initialize mutex/cond for signals from process
                pthread_mutex_init(&_lock, 0);
                pthread_cond_init(&_ready, 0);

                // set up zeromq socket
                fs::path path("/tmp/org.meliza.jill");
                path /= options.server_name;
                path /= options.client_name;
                if (!fs::exists(path)) {
                        fs::create_directories(path);
                }
                path /= "req";
                std::ostringstream endpoint;
                endpoint << "ipc://" << path.string();

                zsock_t * req_socket = zsock_new_router(endpoint.str().c_str());
                if (req_socket == 0) {
                        LOG << "unable to bind to endpoint " << endpoint.str();
                        throw Exit(-1);
                }
                else {
                        INFO << "listening for requests at " << endpoint.str();
                }

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput | JackPortIsTerminal, 0);
                port_trigout = client->register_port("trig_out", JACK_DEFAULT_MIDI_TYPE,
                                                     JackPortIsOutput | JackPortIsTerminal, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->activate();
                // set this after starting the client so it will only be called
                // when the buffer size *changes*
                client->set_buffer_size_callback(jack_bufsize);

                client->connect_ports("out",
                                      options.output_ports.begin(), options.output_ports.end());
                client->connect_ports("trig_out",
                                      options.trigout_ports.begin(), options.trigout_ports.end());

                pthread_t monitor_thread_id;
                pthread_create(&monitor_thread_id, NULL, stim_monitor, NULL);

                LOG << "waiting for requests";
                while (_running) {
                        zmsg_t * msg = zmsg_recv(req_socket);
                        if (!msg)
                                break; // interrupted
                        zframe_t * command = zmsg_last(msg);
                        char * data = zframe_strdup(command);
                        if (memcmp(data, REQ_STIMLIST, strlen(REQ_STIMLIST)) == 0) {
                                LOG << "client requested playlist";
                                zframe_reset(command, stimlist.c_str(), stimlist.size());
                        }
                        else if (memcmp(data, REQ_PLAYSTIM, strlen(REQ_PLAYSTIM)) == 0) {
                                std::string stim(data + strlen(REQ_PLAYSTIM) + 1);
                                boost::ptr_map<std::string, stimulus_t>::iterator it = _stimuli.find(stim);
                                if (it == _stimuli.end()) {
                                        LOG << "client requested invalid stimulus: " << stim;
                                        zframe_reset(command, REP_BADSTIM, strlen(REP_BADSTIM));
                                }
                                else if (__sync_bool_compare_and_swap(&_stim, 0, it->second)) {
                                        LOG << "playing stimulus: " << stim;
                                        zframe_reset(command, REP_OK, strlen(REP_OK));
                                }
                                else {
                                        LOG << "client requested stimulus while busy";
                                        zframe_reset(command, "BUSY", 4);
                                }
                        }
                        else {
                                LOG << "invalid client request";
                                zframe_reset(command, REP_BADCMD, strlen(REP_BADCMD));
                        }
                        zstr_free(&data);
                        zmsg_send(&msg, req_socket);
                }
                LOG << "stopping";

                client->deactivate();
                pthread_join(monitor_thread_id, NULL);
                pthread_mutex_destroy(&_lock);
                pthread_cond_destroy(&_ready);
                zsock_destroy(&req_socket);

                return EXIT_SUCCESS;
        }

        catch (Exit const &e) {
                return e.status();
        }
        catch (std::exception const &e) {
                LOG << "ERROR: " << e.what();
                return EXIT_FAILURE;
        }

}


jstim_options::jstim_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name)->default_value("default"),
                 "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("out,o",     po::value<vector<string> >(&output_ports),
                 "add connection to output audio port")
                ("event,e",   po::value<vector<string> >(&trigout_ports),
                 "add connection to output event port")
                ("chan,c",    po::value<midi::data_type>(&trigout_chan)->default_value(0),
                 "set MIDI channel for output messages (0-16)");

        // tropts is a group of options
        po::options_description opts("Stimulus options");
        //opts.add_options()

        cmd_opts.add(jillopts).add(opts);
        cmd_opts.add_options()
                ("_stim", po::value<vector<string> >(&stimuli)->multitoken(), "stimulus file");
        pos_opts.add("_stim", -1);
        visible_opts.add(jillopts).add(opts);
}

void
jstim_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [stimfile] [stimfile] ...\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       sampled output of the presented stimulus\n"
                  << " * trig_out:  event port reporting stimulus onset/offsets\n"
                  << std::endl;
}
