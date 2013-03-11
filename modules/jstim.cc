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
        float min_gap;          // min gap btw sound, in s
        float min_interval;     // min interval btw starts, in s

protected:

	virtual void print_usage();

}; // jstim_options


jstim_options options(PROGRAM_NAME, PROGRAM_VERSION);
boost::scoped_ptr<jack_client> client;
boost::scoped_ptr<util::stimset> stimset;
jack_port_t *port_out, *port_trigout, *port_trigin;
int ret = EXIT_SUCCESS;

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        return 0;
}

// /**
//  * Callback for samplerate changes. However, this function is only called once.
//  */
// int
// samplerate_callback(jack_client *client, nframes_t samplerate)
// {
//         using std::endl;
// 	nframes_t period_size = options.period_size_ms * samplerate / 1000;
// 	int open_crossing_periods = options.open_crossing_period_ms / options.period_size_ms;
// 	int close_crossing_periods  = options.close_crossing_period_ms / options.period_size_ms;
// 	int open_count_thresh = options.open_crossing_rate * period_size / 1000 * open_crossing_periods;
// 	int close_count_thresh = options.close_crossing_rate * period_size / 1000 * close_crossing_periods;

//         trigger.reset(new dsp::crossing_trigger<sample_t>(options.open_threshold,
//                                                          open_count_thresh,
//                                                          open_crossing_periods,
//                                                          options.close_threshold,
//                                                          close_count_thresh,
//                                                          close_crossing_periods,
//                                                          period_size));

//         // Log parameters
//         client->log(false) << "period size: " << options.period_size_ms << " ms, " << period_size << " samples" << endl;
//         client->log(false) << "open threshold: " << options.open_threshold << endl;
//         client->log(false) << "open count thresh: " << open_count_thresh << endl;
//         client->log(false) << "open integration window: " << options.open_crossing_period_ms << " ms, " << open_crossing_periods << " periods " << endl;
//         client->log(false) << "close threshold: " << options.close_threshold << endl;
//         client->log(false) << "close count thresh: " << close_count_thresh << endl;
//         client->log(false) << "close integration window: " << options.close_crossing_period_ms << " ms, " << close_crossing_periods << " periods " << endl;
//         return 0;
// }

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
                sset->add(new file::stimfile(p.string()), nreps);
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

                stimset.reset(new util::stimset(client->sampling_rate()));
                init_stimset(stimset.get(), options.stimuli, options.nreps);
        jill::stimulus_t const *fp = stimset->next();
        while (fp != 0) {
                printf("%s\n", fp->path().c_str());
                assert(fp->buffer() != 0);
                fp = stimset->next();
        }

                // port_in = client->register_port("in", JACK_DEFAULT_AUDIO_TYPE,
                //                                JackPortIsInput | JackPortIsTerminal, 0);
                // port_trig = client->register_port("trig_out",JACK_DEFAULT_MIDI_TYPE,
                //                                 JackPortIsOutput | JackPortIsTerminal, 0);
                // if (options.count("count-port")) {
                //         port_count = client->register_port("count",JACK_DEFAULT_AUDIO_TYPE,
                //                                           JackPortIsOutput | JackPortIsTerminal, 0);
                // }

                // client->set_sample_rate_callback(samplerate_callback);
                // client->set_process_callback(process);
                // client->activate();

		// vector<string>::const_iterator it;
		// for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
		// 	client->connect_port(*it, "in");
		// }

		// for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
		// 	client->connect_port("trig_out",*it);
		// }

                // while(1) {
                //         sleep(1);
                //         trig_times.pop(log_times); // calls visitor function on ringbuffer
                // }

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
                ("gap,g",     po::value<float>(&min_gap)->default_value(1.0), "minimum gap between sound (seconds)")
                ("interval,i",po::value<float>(&min_interval)->default_value(0.0),
                 "minimum interval between stimulus start times (seconds)");

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

