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
 * This example is the fourth chapter in the JILL tutorial.  We will
 * learn how to write a custom processing class, which we'll define in
 * a separate file. We'll also learn how to write an offline test
 * module which can be easily converted to an online module once the
 * logic is debugged.
 *
 * The capture client's task is similar to writer.cc, but instead of
 * just writing every sample to disk, we want to only write data when
 * something is going on; i.e., when the signal exceeds a certain
 * threshold.  The user should be able to specify the options for the
 * signal detector on the command line.  Each time the signal exceeds
 * the threshold, we write a new file to disk.
 *
 * The code for the signal detector is likely to be fairly complex,
 * and we'd like to be able to use it in other
 * applications. Therefore, rather than write the process and main
 * loop functions here, we specify them in a separate class, which can
 * be used by any client. At this point, open trigger_writer.hh to see
 * how to define the class.
 * 
 * Another important task is parsing command-line options. The class
 * that handles this has also been split off into a separate file,
 * capture_options.hh; open this file to see how the options are
 * parsed.
 *
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>


/*
 * Here we include some headers from the JILL framework. Note
 * that we're using angle-brackets to reference the headers stored in
 * standard locations. This will be necessary when developing
 * standalone clients.
 */
#include <jill/simple_client.hh>
#include <jill/sndfile_player_client.hh>
#include <jill/util/logger.hh>
#include <jill/util/multi_sndfile.hh>

/*
 * These are headers for the custom classes we're using in this
 * application.
 */
#include "sapsucker_options.hh"
#include "trigger_writer.hh"
#include "switch_tracker.hh"
#include "switch.hh"
#include "keypress_switch.hh"
#include "file_playback_recorder.hh"
#include "sapsucker_multi_sndfile.hh"
#ifdef DIO_SUPPORT
#include "nidaq_dio_switch.hh"
#else
#warning Building sapsucker without DIO_SUPPORT
#endif

namespace capture {
using namespace jill;

/*
 * This section is similar to writer.cc and mixer.cc. The
 * PlayerJillClass will be used in offline testing. We also declare a
 * pointer to our custom TriggerWriter class (which will be
 * initialized after we read in the options.
 */
static boost::scoped_ptr<SimpleClient> client;
static boost::shared_ptr<SndfilePlayerClient> pclient;
static boost::shared_ptr<SndfilePlayerClient> tutor_client;
static boost::shared_ptr<TriggeredWriter> twriter;

static boost::shared_ptr<SwitchTracker> switch_tracker;

static util::logstream logv;
static int ret = EXIT_SUCCESS;
static bool using_switch = false;

//static int end_time_of_trigger_delay, end_time_of_song_playback;

/*
 * Note that the process function isn't here any more; it's
 * encapsulated in the TriggeredWriter class.  However, we do still
 * need to write a function that will be called in the main loop.  In
 * theory, we could pass a pointer to the flush() member function
 * directly to the Client class, but this requires some extra
 * work, and in many cases writing a wrapper function is much simpler.
 */
static int
mainloop()
{
	std::string outfile = twriter->flush();

	// check nidaq
	if (using_switch) {
		if (switch_tracker->trigger()) {
			twriter->enable(false);
		} // if

		if (switch_tracker->song_ended()) {
			twriter->enable(true);
		} // if
	} // if (using switch)

	if (pclient && !pclient->is_running()) {
		logv << logv.allfields << pclient->client_name() << ": completed playback" << std::endl;
		twriter->close_entry();
		return 1;
	}
	else
		return 0;
}

static void signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	client->stop("Received interrupt");
	if (pclient) pclient->stop("Received interrupt");
	if (switch_tracker) switch_tracker->stop_playback("Received interrupt");
	if (tutor_client) tutor_client->stop("Received interrupt");
}

} // capture

using namespace capture;
using namespace jill;

int
main(int argc, char **argv)
{
	using namespace std;
	try {

		/*
		 * Here we use our custom CaptureOptions class. Note
		 * that we pass an additional argument to parse(),
		 * which checks for a file "capture.ini" in the
		 * current directory and parses it if it exists.
		 */
		SapsuckerOptions options("sapsucker", "1.1.0rc1");
		options.parse(argc,argv,"sapsucker.ini");

		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		using_switch = options.switch_active;

		if (using_switch) {
			boost::shared_ptr<Switch> sw;
			if (options.use_keypress) {
				sw.reset((Switch*) new KeypressSwitch());
			} else {
#ifndef DIO_SUPPORT
				logv << logv.allfields << "Built without DIO support." << std::endl;
				logv << logv.allfields << "Either use keypress-activated switches or rebuild with -DDIO_SUPPORT." << std::endl;
				logv << logv.allfields << "Starting with switch triggered playback DISABLED." << std::endl;
				using_switch = false;
#else
				sw.reset((Switch*) new NidaqDioSwitch(options.comedi_device_name.c_str(),
				                                      options.comedi_subdevice_index,
				                                      options.dio_line));
#endif
			} // if
			if (using_switch) {
				sw->initialize();

				boost::shared_ptr<SwitchPlaybackListener> listener;
				listener.reset((SwitchPlaybackListener*) new FilePlaybackRecorder(options.switch_status_file_name.c_str()));

				switch_tracker.reset(new SwitchTracker(options.client_name.c_str(),
				                                       options.tutor_song_file_name.c_str(),
				                                       options.switch_refraction,
				                                       options.quotas,
				                                       listener,
				                                       sw));
			} // if (using switch)
		} // if (using switch)

		/*
		 * Initialize the client. 
		 */
		client.reset(new SimpleClient(options.client_name.c_str(), "in"));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;

		/*
		 * The TriggeredWriter class requires references to a
		 * WindowDiscriminator and a multisndfile. We
		 * instantiate these objects first, using the values
		 * in the options.  Recall that twriter is a smart
		 * pointer, so the object it points to is allocated on
		 * the heap (with new), in contrast to the wd and
		 * writer objects, which are allocated on the stack.
		 */
		options.adjust_values(client->samplerate());
		WindowDiscriminator<sample_t> wd(options.open_threshold,
						 options.open_count_thresh, 
						 options.open_crossing_periods,
						 options.close_threshold,
						 options.close_count_thresh, 
						 options.close_crossing_periods,
						 options.period_size);
		
		SapsuckerMultiSndfile writer(options.output_file_tmpl, 
		                             client->samplerate(),
		                             options.permanent_output_destination,
		                             &logv);
		twriter.reset(new TriggeredWriter(wd, writer, logv,
						  options.prebuffer_size,
						  client->samplerate() * 2));


		/* Log parameters */
		logv << logv.program << "output template: " << options.output_file_tmpl << endl
		     << logv.program << "sampling rate: " << client->samplerate() << endl
		     << logv.program << "prebuffer size (samples): " << options.prebuffer_size << endl
		     << logv.program << "period size (samples): " << options.period_size << endl
		     << logv.program << "open threshold: " << options.open_threshold << endl
		     << logv.program << "open count thresh: " << options.open_count_thresh << endl
		     << logv.program << "open periods: " << options.open_crossing_periods << endl
		     << logv.program << "close threshold: " << options.close_threshold << endl
		     << logv.program << "close count thresh: " << options.close_count_thresh << endl
		     << logv.program << "close periods: " << options.close_crossing_periods << endl;


		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		/* Create client to read input file, if specified, as in mixer */
		pclient = SndfilePlayerClient::from_port_list(options.input_ports, &logv);
		if (pclient)
			logv << logv.allfields << "Input file: " << *pclient << endl;

		/* If the user requests counts, and the program is in offline mode: */
		boost::scoped_ptr<jill::util::Sndfile> countfile;
		if (!options.debug_file_name.empty()) {
			if (pclient) {
				countfile.reset(new jill::util::SimpleSndfile(options.debug_file_name.c_str(),
							         	      client->samplerate() / options.period_size));
				wd.set_file_output(countfile.get());
				logv << logv.allfields << "Opened " << options.debug_file_name
				     << " to store count data" << endl;
			}
			else
				logv << logv.program << "Count file will not be opened in live mode" << endl;
		}
				

		/* connect ports */
		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_input(it->c_str());
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}
		if (using_switch) {
			for (it = options.output_ports.begin(); it != options.output_ports.end(); ++it) {
				switch_tracker->connect_playback_output(it->c_str());
				logv << logv.allfields << "Connected output to port " << *it << endl;
			} // for
		} // if
		/*
		 * Here's where we pass the processing object to the
		 * client. The semantics are a little tricky, because
		 * twriter is a shared pointer (which we need in order
		 * to dynamically allocate it here).  The * operator
		 * dereferences the pointer, which we then wrap in
		 * boost::ref in order to ensure that the object is
		 * passed by reference - otherwise
		 * set_process_callback tries to copy it (which will
		 * raise a compiler error because TriggeredWriter is
		 * noncopyable)
		 */
		client->set_process_callback(boost::ref(*twriter));

		/*
		 * Finally, we're ready to start the client. We have
		 * to make a choice as to which client's run() to
		 * call.  It's a little tricky to use them both,
		 * because the default behavior is to *block* until
		 * the exit conditions are met. One option would be to
		 * use one of the clients in non-blocking mode; the
		 * one we use here is to simply register the mainloop
		 * callback with the SndfilePlayerClient in test mode,
		 * and with the SimpleClient in live mode, taking
		 * advantage of the polymorphism as we did in mixer.cc
		 */
		Client *cl = client.get();
		if (pclient) cl = pclient.get();

		cl->set_mainloop_callback(mainloop);
		cl->run();
		//cl->set_freewheel(true); // try to run faster than realtime
		logv << logv.allfields << cl->get_status() << endl;
		return ret;
	}
	catch (Exit const &e) {
		return e.status();
	}
	catch (exception const &e) {
		cerr << "Error: " << e.what() << endl;
		return EXIT_FAILURE;
	}
}

/*
 * To test the capture client, run it as follows:
 *
 * ./capture -i sndfile:<test.wav>
 *
 * where <test.wav> is a recording of the kinds of signals you expect
 * to get. The logged output will indicate where the window
 * discriminator opened and closed. Adjust the gate options until it
 * opens at the beginning of the signals of interest, and closes when
 * they end.  The options can be stored in capture.ini.
 *
 * When ready to run online, just substitute the live input port for
 * the soundfile.
 */
