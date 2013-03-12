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
#include <pthread.h>
#include <boost/scoped_ptr.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/util/stimset.hh"
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
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<util::stimset> stimset;
util::stimqueue queue = {};
jack_port_t *port_out, *port_trigout, *port_trigin;
int ret = EXIT_SUCCESS;

// synchronization for stimqueue
static pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  data_needed = PTHREAD_COND_INITIALIZER;

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        void *trig = client->events(port_trigout, nframes);
        jack_midi_clear_buffer(trig);

        // check that stimulus is queued
        if (!queue.ready()) return 0;
        bool just_started = false;

        nframes_t offset;
        // am I playing a stimulus?
        if (queue.playing()) {
                offset = 0;
        }
        // is there an external trigger?
        else if (port_trigin) {
                just_started = true;
        }
        // has enough time elapsed since the last stim?
        else {
                offset = queue.offset(time, options.min_interval, options.min_gap);
                just_started = true;
        }

        if (offset >= nframes) return 0;

        std::string const & name = queue.current_stim->name();
        // generate onset event if not already playing
        if (just_started) {
                queue.start(time + offset);
                jack_midi_data_t *buf = jack_midi_event_reserve(trig, offset, name.length() + 2);
                buf[0] = midi::stim_on;
                strcpy(reinterpret_cast<char*>(buf)+1, name.c_str());
        }

        sample_t *out = client->samples(port_out, nframes);
        nframes_t nsamples = std::min(queue.nsamples(), nframes);
        memcpy(out + offset, queue.buffer(), nsamples);
        queue.advance(nsamples);

        // did the stimulus end?
        if (queue.nsamples() == 0) {
                // signal disk thread we are done with this stimulus
                queue.stop(time + offset + nsamples);
                if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
                        pthread_cond_signal (&data_needed);
                        pthread_mutex_unlock (&disk_thread_lock);
                }

                jack_midi_data_t *buf = jack_midi_event_reserve(trig, offset + nsamples, name.length() + 2);
                buf[0] = midi::stim_off;
                strcpy(reinterpret_cast<char*>(buf)+1, name.c_str());
        }

        return 0;
}


void
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
                client->log() << "stimulus: " << p.stem() << " (" << stim->duration() << " s)" << std::endl;
        }
}


int
main(int argc, char **argv)
{
	using namespace std;
	try {
		options.parse(argc,argv);
                cout << "[" << options.client_name << "] " <<  PROGRAM_NAME ", version " PROGRAM_VERSION << endl;

                client.reset(new jack_client(options.client_name));
                options.min_gap = options.min_gap_sec * client->sampling_rate();
                options.min_interval = options.min_interval_sec * client->sampling_rate();
                client->log(false) << "minimum gap: " << options.min_gap_sec << "s ("
                                   << options.min_gap << " samples)" << endl;
                client->log(false) << "minimum interval: " << options.min_interval_sec << "s ("
                                   << options.min_interval << " samples)" << endl;

                stimset.reset(new util::stimset(client->sampling_rate()));
                init_stimset(stimset.get(), options.stimuli, options.nreps);
                if (options.count("shuffle")) {
                        client->log() << "shuffled stimuli" << endl;
                        stimset->shuffle();
                }

                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput | JackPortIsTerminal, 0);
                port_trigout = client->register_port("trig_out",JACK_DEFAULT_MIDI_TYPE,
                                                     JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("trig")) {
                        port_trigin = client->register_port("trig_in",JACK_DEFAULT_MIDI_TYPE,
                                                           JackPortIsOutput | JackPortIsTerminal, 0);
                }

                client->set_process_callback(process);
                client->activate();

                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());
                client->connect_ports("trig_out", options.trigout_ports.begin(), options.trigout_ports.end());
                client->connect_ports(options.trigin_ports.begin(), options.trigin_ports.end(), "trig_in");

                /*
                 * main thread: queue up current and next stimulus. wait on
                 * process thread for signal to move to next stimulus
                 */
                pthread_mutex_lock (&disk_thread_lock);
                queue.current_stim = stimset->next();
                while(queue.current_stim) {
                        client->log() << "stim: " << queue.current_stim->name() << endl;
                        queue.next_stim = stimset->next(); // 0 = all played
                        // check if process finished playing the current
                        // stimulus while we were loading the next one; if not,
                        // wait for condition variable
                        if (queue.current_stim)
                                pthread_cond_wait (&data_needed, &disk_thread_lock);
                        queue.current_stim = queue.next_stim;
                }

                pthread_mutex_unlock (&disk_thread_lock);
                client->log() << "end of stimulus queue" << endl;
                // give the process loop a chance to clear the port buffer
                usleep(2e6 * client->buffer_size() / client->sampling_rate());

		return ret;
	}
	/*
	 * These catch statements handle two kinds of exceptions.  The
	 * Exit exception is thrown to terminate the application
	 * normally (i.e. if the user asked for the app version or
	 * usage); other exceptions are typically thrown if there's a
	 * serious error, in which case the user is notified on
	 * stderr.
	 */
	catch (Exit const &e) {
		return e.status();
	}
	catch (std::exception const &e) {
                std::cerr << "Error: " << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	/* Because we used smart pointers and locally scoped variables, there is no cleanup to do! */
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
                ("trig,t",    po::value<vector<string> >(&trigin_ports), "add connection to input trigger port");

        // tropts is a group of options
        po::options_description opts("Stimulus options");
        opts.add_options()
                ("shuffle,s", "shuffle order of presentation")
                ("repeats,r", po::value<size_t>(&nreps)->default_value(1), "default number of repetitions")
                ("gap,g",     po::value<float>(&min_gap_sec)->default_value(1.0),
                 "minimum gap between sound (s)")
                ("interval,i",po::value<float>(&min_interval_sec)->default_value(0.0),
                 "minimum interval between stimulus start times (s)");

        cmd_opts.add(jillopts).add(opts);
        cmd_opts.add_options()
                ("stim", po::value<vector<string> >(&stimuli)->multitoken(), "output filename");
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

