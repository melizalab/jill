/*
 * Provides stimulus playback
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dmeliza@uchicago.edu>
 */
#include <iostream>
#include <signal.h>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/util/readahead_stimqueue.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jstim"

using namespace jill;
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
	std::vector<string> trigin_ports;

        std::vector<string> stimuli; // this is postprocessed

        size_t nreps;           // default set by reps flag
        float min_gap_sec;      // min gap btw sound, in sec
        float min_interval_sec; // min interval btw starts, in sec
        nframes_t min_gap;
        nframes_t min_interval;

protected:

	virtual void print_usage();

}; // jstim_options


jstim_options options(PROGRAM_NAME);
boost::shared_ptr<jack_client> client;
boost::shared_ptr<util::readahead_stimqueue> queue;
boost::ptr_vector<stimulus_t> _stimuli;
std::vector<stimulus_t *> _stimlist;
jack_port_t *port_out, *port_trigout, *port_trigin;

int xruns = 0;                  // xrun counter

/* the realtime process loop. some complicated logic follows! */
int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        // NB static variables are initialized to 0
        static nframes_t stim_offset; // current position in stimulus buffer
        static nframes_t last_start;  // last stimulus start time
        static nframes_t last_stop;   // last stimulus stop

        nframes_t period_offset;      // the offset in the period to start copying

        void * trig = client->events(port_trigout, nframes);
        sample_t * out = client->samples(port_out, nframes);
        // zero the output buffer - somewhat inefficient but safer
        memset(out, 0, nframes * sizeof(sample_t));

        // the currently playing stimulus (or nullptr)
        jill::stimulus_t const * stim = queue->head();

        // handle xruns
        if (xruns) {
                // playing stimuli are truncated
                if (stim_offset > 0 && stim) {
                        stim_offset = stim->nframes();
                }
                // reset timers because running time has changed
                last_start = last_stop = time;
                // indicate we've handled the xrun
                __sync_add_and_fetch(&xruns, -1);
        }

        // if no stimulus queued do nothing
        if (stim == 0) return 0;

        // am I playing a stimulus?
        if (stim_offset > 0) {
                period_offset = 0;
        }
        // is there an external trigger?
        else if (port_trigin) {
                void * midi_buffer = client->events(port_trigin, nframes);
                period_offset = midi::find_trigger(midi_buffer, true);
                if (period_offset > nframes) return 0; // no trigger
                last_start = time + period_offset;
                midi::write_message(trig, period_offset, midi::stim_on, stim->name());
                DBG << "playback triggered: time=" << last_start << ", stim=" << stim->name();
        }
        // has enough time elapsed since the last stim?
        else {
                // deltas with last events - overflow is correct because time >= lastX
                nframes_t dstart = time - last_start;
                nframes_t dstop = time - last_stop;
                // now check for overflow
                nframes_t ostart = (dstart > options.min_interval) ? 0 : options.min_interval - dstart;
                nframes_t ostop = (dstop > options.min_gap) ? 0 : options.min_gap - dstop;
                period_offset = std::max(ostart, ostop);
                if (period_offset >= nframes) return 0; // not time yet
                last_start = time + period_offset;
                midi::write_message(trig, period_offset, midi::stim_on, stim->name());
                DBG << "playback started: time=" << last_start << ", stim=" << stim->name();
        }
        // sanity check - will be optimized out
        assert(period_offset < nframes);

        // copy samples, if there are any
        nframes_t nsamples = std::min(stim->nframes() - stim_offset, nframes);
        if (nsamples > 0) {
                memcpy(out + period_offset,
                       stim->buffer() + stim_offset,
                       nsamples * sizeof(sample_t));
                stim_offset += nsamples;
        }
                // did the stimulus end?
        if (stim_offset >= stim->nframes()) {
                queue->release();
                last_stop = time + period_offset + nsamples;
                midi::write_message(trig, period_offset + nsamples,
                                    midi::stim_off, stim->name());
                DBG << "playback ended: time=" << last_stop << ", stim=" << stim->name();
                stim_offset = 0;
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
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        if (queue) queue->stop();
}

void
signal_handler(int sig)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        if (queue) queue->stop();
}


/* parse the list of stimuli */
static void
init_stimset(std::vector<string> const & stims, size_t const default_nreps)
{
        using namespace boost::filesystem;

        size_t nreps;
        for (size_t i = 0; i < stims.size(); ++i) {
                path p(stims[i]);
                if ((i+1) < stims.size()) {
                        if (sscanf(stims[i+1].c_str(),"%zd",&nreps) == 0) {
                                nreps = default_nreps;
                        }
                }
                else nreps = default_nreps;
                try {
                        jill::stimulus_t *stim = new file::stimfile(p.string());
                        _stimuli.push_back(stim);
                        for (size_t j = 0; j < nreps; ++j)
                                _stimlist.push_back(stim);
                }
                catch (jill::FileError const & e) {
                        LOG << "invalid stimulus " << p << ": " << e.what();
                }
        }
}


int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                client.reset(new jack_client(options.client_name, options.server_name));
                options.min_gap = options.min_gap_sec * client->sampling_rate();
                options.min_interval = options.min_interval_sec * client->sampling_rate();
                options.trigout_chan &= midi::chan_nib;

                if (!options.count("trig")) {
                        LOG << "minimum gap: " << options.min_gap_sec << "s ("
                                      << options.min_gap << " samples)";
                        LOG << "minimum interval: " << options.min_interval_sec << "s ("
                                      << options.min_interval << " samples)";
                }
		if (options.stimuli.size() == 0) {
		        LOG << "no stimuli; quitting";
			throw Exit(0);
		}

                /* stimulus queue */
                init_stimset(options.stimuli, options.nreps);
                if (options.count("shuffle")) {
                        LOG << "shuffled stimuli";
                        random_shuffle(_stimlist.begin(), _stimlist.end());
                }
                queue.reset(new util::readahead_stimqueue(_stimlist.begin(), _stimlist.end(),
                                                          client->sampling_rate(),
                                                          options.count("loop")));

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput | JackPortIsTerminal, 0);
                port_trigout = client->register_port("trig_out",JACK_DEFAULT_MIDI_TYPE,
                                                     JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("trig")) {
                        LOG << "triggering playback from trig_in";
                        port_trigin = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                            JackPortIsInput | JackPortIsTerminal, 0);
                }

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

                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());
                client->connect_ports("trig_out", options.trigout_ports.begin(), options.trigout_ports.end());
                client->connect_ports(options.trigin_ports.begin(), options.trigin_ports.end(), "trig_in");

                // wait for stimuli to finish playing
                queue->join();
                client->deactivate();

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
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output audio port")
                ("event,e",   po::value<vector<string> >(&trigout_ports), "add connection to output event port")
                ("chan,c",    po::value<midi::data_type>(&trigout_chan)->default_value(0),
                 "set MIDI channel for output messages (0-16)")
                ("trig,t",    po::value<vector<string> >(&trigin_ports)->multitoken()->zero_tokens(),
                 "add connection to input trigger port");

        // tropts is a group of options
        po::options_description opts("Stimulus options");
        opts.add_options()
                ("shuffle,S", "shuffle order of presentation")
                ("loop,l",    "loop endlessly")
                ("repeats,r", po::value<size_t>(&nreps)->default_value(1), "default number of repetitions")
                ("gap,g",     po::value<float>(&min_gap_sec)->default_value(2.0),
                 "minimum gap between sound (s)")
                ("interval,i",po::value<float>(&min_interval_sec)->default_value(0.0),
                 "minimum interval between stimulus start times (s)");

        cmd_opts.add(jillopts).add(opts);
        cmd_opts.add_options()
                ("stim", po::value<vector<string> >(&stimuli)->multitoken(), "stimulus file");
        pos_opts.add("stim", -1);
        visible_opts.add(jillopts).add(opts);
}

void
jstim_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [stimfile [nreps]] [stim [nreps]] ...\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       sampled output of the presented stimulus\n"
                  << " * trig_out:  event port reporting stimulus onset/offsets\n"
                  << " * trig_in:   (optional) event port for triggering playback"
                  << std::endl;
}

