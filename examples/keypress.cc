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
#include <signal.h>

#include "jill/sndfile_player_client.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
using namespace jill;
using namespace std;

static boost::shared_ptr<SndfilePlayerClient> client;
static util::logstream logv;
static vector<string> soundfiles;
static int ret = EXIT_SUCCESS;

static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop("Received interrupt");
}

int
main(int argc, char **argv)
{
	try {
		JillOptions options("keypress", "1.1.0rc3");
		po::options_description keyopts("Keypress options");
		keyopts.add_options()
			("sndfile,s", po::value<std::vector<std::string> >(), "soundfile paths");
		options.cmd_opts.add(keyopts);
		options.visible_opts.add(keyopts);
		options.pos_opts.add("sndfile",-1);
		options.parse(argc,argv);

		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		client.reset(new SndfilePlayerClient(options.client_name.c_str()));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		
		vector<string>::const_iterator it;
		for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
			client->connect_port("out",it->c_str());
			logv << logv.allfields << "Connected output to port " << *it << endl;
		}

		/* load soundfiles into client */
		client->_sounds.set_logger(&logv);
		options.assign(soundfiles,"sndfile");
		for (vector<string>::iterator it = soundfiles.begin(); it != soundfiles.end(); ++it)
			client->_sounds.load_file(*it, it->c_str());

		for (;;) {
			string val;
			int i = 0;
			cout << "\nSelect next sound to play:" << endl;
			for (vector<string>::iterator it = soundfiles.begin(); it!= soundfiles.end(); ++it, ++i)
				cout << i << ": " << *it << endl;
			cout << "Any other value: exit" << endl;
			cin >> val;
			try {
				i = boost::lexical_cast<int>(val);
				client->_sounds.select(soundfiles.at(i));
			}
			catch (std::exception const &e) {
				logv << logv.allfields << "Exiting" << endl;
				break;
			}
			cout << client->_sounds << endl;
			client->run();
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
