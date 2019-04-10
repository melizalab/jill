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
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <signal.h>
#include <boost/filesystem.hpp>
#include <boost/ptr_container/ptr_map.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "jill/zmq.hh"
#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/dsp/ringbuffer.hh"

constexpr char PROGRAM_NAME[] = "jstimserver";

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
boost::ptr_map<std::string, stimulus_t> _stimuli;
// current stimulus
stimulus_t const * _stim = 0;
// mutex/cond to signal zmq thread when various things happen
std::mutex _lock;
std::condition_variable  _ready;
static bool _running = true;
static bool _xrun = false;
static bool _interrupt = false;
// jack ports
jack_port_t *port_out, *port_trigout;


/** The realtime process loop for jstimserver. */
int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        // NB static variables are initialized to 0
        static nframes_t stim_offset; // current position in stimulus buffer

        void * trig = client->events(port_trigout, nframes);
        sample_t * out = client->samples(port_out, nframes);
        // zero the output buffer
        for(nframes_t i = 0; i < nframes; ++i)
                out[i] = 0.0f;

        // if no stimulus queued stop
        if (!_stim) {
                return 0;
        }
        else if (_interrupt) {
                midi::write_message(trig, 0, midi::stim_off, _stim->name());
                stim_offset = 0;
                _stim = nullptr;
                _ready.notify_one();
                return 0;
        }

        // notify if stimulus started
        if (stim_offset == 0) {
                midi::write_message(trig, 0,
                                    midi::stim_on, _stim->name());
                _ready.notify_one();
        }

        // copy samples, if there are any
        nframes_t nsamples = std::min(_stim->nframes() - stim_offset, nframes);
        auto * stimptr = _stim->buffer() + stim_offset;
        nframes_t i = 0;
        for (; i < nsamples; ++i)
                out[i] = stimptr[i];
        for (; i < nframes; ++i)
                out[i] = 0.0f;
        stim_offset += nsamples;

        // did the stimulus end?
        if (stim_offset >= _stim->nframes()) {
                midi::write_message(trig, nsamples,
                                    midi::stim_off, _stim->name());
                // reset counter
                stim_offset = 0;
                // set ptr to null once done
                _stim = nullptr;
                // notify condition variable
                _ready.notify_one();
        }

        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        std::lock_guard<std::mutex> lock(_lock);
        _xrun = true;
        _ready.notify_one();
        return 0;
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        // we use the xruns counter to notify that an interruption in the audio
        // stream has occurred
        std::lock_guard<std::mutex> lock(_lock);
        _xrun = true;
        _ready.notify_one();
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        std::lock_guard<std::mutex> lock(_lock);
        _running = false;
        _ready.notify_one();
}

void
signal_handler(int sig)
{
        std::lock_guard<std::mutex> lock(_lock);
        _running = false;
        _ready.notify_one();
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
                        stim_node.put("duration", stim->duration());
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
void
stim_monitor()
{
        // set up zeromq socket
        fs::path path{"/tmp/org.meliza.jill"};
        path /= options.server_name;
        path /= options.client_name;
        path /= "pub";
        std::ostringstream endpoint;
        endpoint << "ipc://" << path.string();
        void * socket = zmq::context::socket(ZMQ_PUB);
        if (zmq_bind(socket, endpoint.str().c_str()) < 0) {
                LOG << "unable to bind to endpoint " << endpoint.str();
                return;
        }
        else {
                INFO << "publishing start/stop events at " << endpoint.str();
        }
        // notify any waiting clients that we are alive. This is a pretty
        // fragile system for doing heartbeats.
        std::unique_lock<std::mutex> lck(_lock);
        zmq::send(socket, "STARTING");
        while (_running) {
                _ready.wait(lck);
                if (_xrun)
                        zmq::send(socket, "XRUN");
                else if (!_running)
                        zmq::send(socket, "STOPPING");
                else if (_stim) {
                        std::ostringstream o;
                        o << "PLAYING " << _stim->name();
                        zmq::send(socket, o.str());
                }
                else if (__sync_bool_compare_and_swap(&_interrupt, true, false)) {
                        zmq::send(socket, "INTERRUPTED");
                }
                else {
                        zmq::send(socket, "DONE");
                }
        }
        zmq_close(socket);
}

constexpr char REQ_STIMLIST[] = "STIMLIST";
constexpr char REQ_PLAYSTIM[] = "PLAY";
constexpr char REQ_INTERRUPT[] = "INTERRUPT";
constexpr char REP_BADCMD[] = "BADCMD";
constexpr char REP_BADSTIM[] = "BADSTIM";
constexpr char REP_OK[] = "OK";
constexpr char REP_BUSY[] = "BUSY";
constexpr char REP_NOTPLAYING[] = "NOTPLAYING";

int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                auto client = std::make_unique<jack_client>(options.client_name,
                                                            options.server_name);
                options.trigout_chan &= midi::chan_nib;

                if (options.stimuli.size() == 0) {
                        LOG << "no stimuli; quitting";
                        throw Exit(0);
                }
                /* load the stimuli */
                std::string stimlist = init_stimset(options.stimuli, client->sampling_rate());
                DBG << "stimlist: " << stimlist;

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

                void * req_socket = zmq::context::socket(ZMQ_ROUTER);
                if (zmq_bind(req_socket, endpoint.str().c_str()) < 0) {
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

                std::thread monitor_thread(stim_monitor);
                LOG << "waiting for requests";
                while (_running) {
                        // blocking call to receive messages
                        std::vector<std::string> messages = zmq::recv(req_socket);
                        if (messages.empty())
                                break; // interrupted; exit the loop
                        auto data = messages.back();
                        if (data.compare(REQ_STIMLIST) == 0) {
                                DBG << "sent playlist to client";
                                messages.back() = stimlist;
                        }
                        else if (data.compare(0, strlen(REQ_PLAYSTIM), REQ_PLAYSTIM) == 0) {
                                auto stim = data.substr(strlen(REQ_PLAYSTIM) + 1);
                                auto it = _stimuli.find(stim);
                                if (it == _stimuli.end()) {
                                        LOG << "client requested invalid stimulus: " << stim;
                                        messages.back() = REP_BADSTIM;
                                }
                                else if (__sync_bool_compare_and_swap(&_stim, 0, it->second)) {
                                        LOG << "playing stimulus: " << stim;
                                        messages.back() = REP_OK;
                                }
                                else {
                                        LOG << "client requested stimulus while busy";
                                        messages.back() = REP_BUSY;
                                }
                        }
                        else if (data.compare(REQ_INTERRUPT) == 0) {
                                if (!_stim) {
                                        LOG << "client requested interrupt while nothing playing";
                                        messages.back() = REP_NOTPLAYING;
                                }
                                else if (__sync_bool_compare_and_swap(&_interrupt, false, true)) {
                                        LOG << "interrupting stimulus playback";
                                        messages.back() = REP_OK;
                                }
                                else {
                                        LOG << "client requested interrupt while nothing playing";
                                        messages.back() = REP_NOTPLAYING;
                                }
                        }
                        else {
                                LOG << "invalid client request";
                                messages.back() = REP_BADCMD;
                        }
                        zmq::send_n(req_socket, messages.begin(), messages.size());
                }
                LOG << "stopping";

                client->deactivate();
                if (monitor_thread.joinable()) monitor_thread.join();
                zmq_close(req_socket);

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
