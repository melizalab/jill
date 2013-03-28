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
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/util/stream_logger.hh"
#include "jill/util/stimset.hh"
#include "jill/util/stimqueue.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jstim"
#define PROGRAM_VERSION "1.3.0"

using namespace jill;

class jstim_options : public program_options {

public:
	jstim_options(std::string const &program_name, std::string const &program_version);

	/** The client name (used in internal JACK representations) */
	std::string client_name;

	/** Ports to connect to */
	std::vector<std::string> output_ports;
        std::vector<std::string> trigout_ports;
        midi::data_type trigout_chan;
	std::vector<std::string> trigin_ports;

        std::vector<std::string> stimuli; // this is postprocessed

        size_t nreps;           // default set by reps flag
        float min_gap_sec;      // min gap btw sound, in sec
        float min_interval_sec; // min interval btw starts, in sec
        nframes_t min_gap;
        nframes_t min_interval;

protected:

	virtual void print_usage();

}; // jstim_options


jstim_options options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<util::stream_logger> logger;
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<util::stimset> stimset;
jack_port_t *port_out, *port_trigout, *port_trigin;
int ret = EXIT_SUCCESS;

util::stimqueue queue;          // manages queue of stimuli
int xruns = 0;
nframes_t last_start = 0;       // last stimulus start time
nframes_t last_stop = 0;        // last stimulus stop

/* the realtime process loop */
int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        void *trig = client->events(port_trigout, nframes);
        sample_t *out = client->samples(port_out, nframes);
        // zero output buffer - inefficient but simple here
        memset(out, 0, nframes * sizeof(sample_t));

        // handle xruns
        if (xruns) {
                // playing stimuli are truncated
                if (queue.playing()) {
                        std::string s(queue.name());
                        s += " TRUNCATED";
                        midi::write_message(trig, 0, midi::stim_off, s.c_str());
                        queue.advance(queue.nsamples()); // stop playback
                }
                midi::write_message(trig, 1, midi::info, "xrun detected");
                // reset timers because running time has changed
                last_start = last_stop = time;
                // indicate we've handled the xrun
                __sync_add_and_fetch(&xruns, -1);
        }

        // check that stimulus is queued
        if (!queue.ready()) {
                // sort of hacky: if the queue is full and there's no buffer
                // it's because the current stim is empty (or the end-of-queue type)
                if (queue.full()) queue.release();
                return 0;
        }

        nframes_t offset;
        // am I playing a stimulus?
        if (queue.playing()) {
                offset = 0;
        }
        // is there an external trigger?
        else if (port_trigin) {
                void * midi_buffer = client->events(port_trigin, nframes);
                offset = midi::find_trigger(midi_buffer, true);
                if (offset > nframes) return 0;
                last_start = time + offset;
                midi::write_message(trig, offset, midi::stim_on, queue.name());
        }
        // has enough time elapsed since the last stim?
        else {
                // deltas with last events - overflow is correct because time >= lastX
                nframes_t dstart = time - last_start;
                nframes_t dstop = time - last_stop;
                // now we have to check for overflow
                nframes_t ostart = (dstart > options.min_interval) ? 0 : options.min_interval - dstart;
                nframes_t ostop = (dstop > options.min_gap) ? 0 : options.min_gap - dstop;
                offset = std::max(ostart, ostop);
                if (offset >= nframes) return 0;
                last_start = time + offset;
                midi::write_message(trig, offset, midi::stim_on, queue.name());
        }

        // copy samples, if there are any
        nframes_t nsamples = std::min(queue.nsamples(), nframes);
        memcpy(out + offset, queue.buffer(), nsamples * sizeof(sample_t));
        queue.advance(nsamples);

        // is the stimulus stopped?
        if (queue.nsamples() == 0) {
                // did it end in this frame?
                if (nsamples > 0) {
                        last_stop = time + offset + nsamples;
                        midi::write_message(trig, offset + nsamples, midi::stim_off, queue.name());
                }
                // try to release the stimulus. may not have an effect, in which
                // case we'll try again on the next loop
                queue.release();
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
signal_handler(int sig)
{
        __sync_add_and_fetch(&xruns, 1); // gcc specific
        // not strictly async safe
        usleep(2e6 * client->buffer_size() / client->sampling_rate());
        exit(sig);
}


/* parse the list of stimuli */
static void
init_stimset(util::stimset * sset, std::vector<std::string> const & stims, size_t const default_nreps)
{
        using namespace boost::filesystem;

        size_t nreps;
        for (size_t i = 0; i < stims.size(); ++i) {
                path p(stims[i]);
                if (!(exists(p) || is_regular_file(p))) continue;
                try {
			nreps = boost::lexical_cast<int>(stims.at(i+1));
                }
                catch (...) {
                        nreps = default_nreps;
                }
                jill::stimulus_t *stim = new file::stimfile(p.string());
                sset->add(stim, nreps);
                logger->log() << "stimulus: " << p.stem() << " (" << stim->samplerate() << " Hz; "
                              << stim->duration() << " s)" << std::endl;
        }
}


int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                logger.reset(new util::stream_logger(cout, options.client_name));
                logger->log() << PROGRAM_NAME ", version " PROGRAM_VERSION << endl;

                client.reset(new jack_client(options.client_name, *logger));
                options.min_gap = options.min_gap_sec * client->sampling_rate();
                options.min_interval = options.min_interval_sec * client->sampling_rate();
                options.trigout_chan &= midi::chan_nib;

                if (!options.count("trig")) {
                        logger->log() << "minimum gap: " << options.min_gap_sec << "s ("
                                     << options.min_gap << " samples)" << endl;
                        logger->log() << "minimum interval: " << options.min_interval_sec << "s ("
                                << options.min_interval << " samples)" << endl;
                }

                stimset.reset(new util::stimset(client->sampling_rate()));
                init_stimset(stimset.get(), options.stimuli, options.nreps);
                if (options.count("shuffle")) {
                        logger->log() << "shuffled stimuli" << endl;
                        stimset->shuffle();
                }

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput | JackPortIsTerminal, 0);
                port_trigout = client->register_port("trig_out",JACK_DEFAULT_MIDI_TYPE,
                                                     JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("trig")) {
                        logger->log() << "triggering playback from trig_in" << endl;
                        port_trigin = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                            JackPortIsInput | JackPortIsTerminal, 0);
                }

                // register signal handlers
		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->activate();
                // set this after starting the client so it will only be called
                // when the buffer size *changes*
                client->set_buffer_size_callback(jack_bufsize);

                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());
                client->connect_ports("trig_out", options.trigout_ports.begin(), options.trigout_ports.end());
                client->connect_ports(options.trigin_ports.begin(), options.trigin_ports.end(), "trig_in");

                /*
                 * main thread: queue up current and next stimulus. wait on
                 * process thread for signal to move to next stimulus. loop
                 * until the queue is empty
                 */
                queue.enqueue(stimset->next());
                while(options.count("loop") || queue.ready()) {
                        queue.enqueue(stimset->next());
                        logger->log() << "next stim: " << queue.name() << endl;
                }

                // give the process loop a chance to clear the midi output buffer
                usleep(2e6 * client->buffer_size() / client->sampling_rate());

		return ret;
	}

	catch (Exit const &e) {
		return e.status();
	}
	catch (std::exception const &e) {
                std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

}


jstim_options::jstim_options(std::string const &program_name, std::string const &program_version)
        : program_options(program_name, program_version)
{
        using std::string;
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
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
                ("shuffle,s", "shuffle order of presentation")
                ("loop,l",    "loop endlessly")
                ("repeats,r", po::value<size_t>(&nreps)->default_value(1), "default number of repetitions")
                ("gap,g",     po::value<float>(&min_gap_sec)->default_value(1.0),
                 "minimum gap between sound (s)")
                ("interval,i",po::value<float>(&min_interval_sec)->default_value(0.0),
                 "minimum interval between stimulus start times (s)");

        cmd_opts.add(jillopts).add(opts);
        cmd_opts.add_options()
                ("stim", po::value<vector<string> >(&stimuli)->multitoken(), "stimulus file");
        pos_opts.add("stim", -1);
        cfg_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jstim_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [stimfile [nreps]] [stimfile [nreps]] ...\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       sampled output of the presented stimulus\n"
                  << " * trig_out:  event port reporting stimulus onset/offsets\n"
                  << " * trig_in:   (optional) event port for triggering playback"
                  << std::endl;
}

