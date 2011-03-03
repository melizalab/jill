/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This example is designed to test triggered soundfile output.
 */
#include <boost/scoped_ptr.hpp>
#include <boost/lexical_cast.hpp>
#include <iostream>
#include <algorithm>
#include <signal.h>

#include "jill/sndfile_player_client.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
using namespace jill;
using namespace std;

static boost::shared_ptr<SndfilePlayerClient> client;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

static void signal_handler(int sig)
{
	// use ret code to signal loops
	ret = (sig != SIGINT) ? EXIT_FAILURE : 2;
	client->stop("Received interrupt");
}

struct PlayerOptions : public JillOptions {

	PlayerOptions(const char *program_name, const char *program_version)
		: JillOptions(program_name, program_version, true) {

		po::options_description opts("Player options");
		opts.add_options()
			("trigger,t", po::value<string>()->default_value("none"), 
			 "trigger type (none|key|dio|ctrl)")
			("random,r", "randomize playback order")
			("count,C", po::value<int>()->default_value(1), "number of times to repeat (0=infinite)")
			("gap,g", po::value<int>()->default_value(100), "ms to pause between files")
			("fast,F", "play files in faster than realtime")
			("dio-line", po::value<int>(), "digital io line for trigger")
			("trig-thresh", po::value<float>()->default_value(0.6), 
			 "set threshold for trigger signal (-1.0-1.0)");
		cmd_opts.add(opts);
		visible_opts.add(opts);
		cmd_opts.add_options()
			("sndfile", po::value<std::vector<std::string> >(), "soundfile paths");
		pos_opts.add("sndfile", -1);
	}

	virtual void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] [sound files]\n"
			  << visible_opts << std::endl
			  << "Playback behavior depends on the trigger option as follows:\n\n"
			  << "      none:  play each supplied sound file, in order or\n"
			  << "             randomly (--random), some number of times\n"
			  << "             (--count), separated by gaps (gap option)\n"
			  << "             With --fast, attempts to play faster than\n"
			  << "             realtime.  Somewhat experimental.\n"
			  << "      key:   wait for user to select which file to play\n"
			  << "      dio:   start playback when a digital line (dio-line) goes high\n"
			  << "             order is controlled by (random)\n"
			  << "      ctrl:  start playback when control line goes above (trig-thresh)\n\n"
			  << " There may be some delay after starting the program while files are resampled"
			  << std::endl;
	}
};

PlayerOptions options("jill_play", "1.1.0rc3");

void
play_untriggered(vector<string> &soundfiles) 
{
	int gap, count;
	bool random, fast;
	vector<string>::const_iterator it;
	options.assign(gap,"gap");
	options.assign(count,"count");
	options.assign(random,"random");
	options.assign(fast, "fast");
	for (int i = 0; (count <= 0) || (i < count); ++i) {
		if (random) std::random_shuffle(soundfiles.begin(), soundfiles.end());
		for (it = soundfiles.begin(); it!= soundfiles.end(); ++it) {
			client->select(*it);
			if (fast) {
				logv << logv.allfields << "start freewheel mode " << endl;
				client->set_freewheel(true);
			}
			logv << logv.allfields << "start playback of " << *it << endl;
			client->run(); // blocks
			if (fast) {
				logv << logv.allfields << "end freewheel mode " << endl;
				client->set_freewheel(false);
			}
			if (ret != EXIT_SUCCESS) return;
			::usleep(gap * 1000);
		}
	}
}

void
play_keyboard(const vector<string> &soundfiles)
{
	vector<string>::const_iterator it;
	string val;
	for (;;) {
		int i = 0;
		cout << "\nSelect next sound to play:" << endl;
		for (it = soundfiles.begin(); it!= soundfiles.end(); ++it, ++i)
			cout << i << ": " << *it << endl;
		cout << "Any other value: exit" << endl;
		cin >> val;
		try {
			i = boost::lexical_cast<int>(val);
			client->select(soundfiles.at(i));
			logv << logv.allfields << "start playback of " << soundfiles.at(i) << endl;
		}
		catch (std::exception const &e) {
			logv << logv.allfields << "Exiting" << endl;
			break;
		}
		client->run();
	}

}


int
main(int argc, char **argv)
{
	try {
		options.parse(argc,argv);

		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		client.reset(new SndfilePlayerClient(options.client_name.c_str(), &logv));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;
		
		vector<string>::const_iterator it;
		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client->connect_port("out",it->c_str());
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		/* load soundfiles into client */
		vector<string> soundfiles;
		options.assign(soundfiles,"sndfile");
		for (vector<string>::iterator it = soundfiles.begin(); it != soundfiles.end(); ++it)
			client->load_file(*it, it->c_str());

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		string mode;
		options.assign(mode, "trigger");
		if (mode.compare("none")==0)
			play_untriggered(soundfiles);
		else if (mode.compare("key")==0)
			play_keyboard(soundfiles);
		else {
			std::cerr << "The trigger type " << mode << " is unknown or unimplemented" << endl;
			ret = EXIT_FAILURE;
		}
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
