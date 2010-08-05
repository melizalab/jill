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
 * After testing the offline version of the TriggerWriter, we're ready
 * to build the live version.  This is almost identical to the test
 * version; the differences are noted below.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers (now online), and the
 * headers for our custom class.
 */
#include "jill/jill_application.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
#include "jill/util/multisndfile.hh"
#include "capture_options.hh"
#include "trigger_writer.hh"
using namespace jill;


static boost::scoped_ptr<JillApplication> app; // JillApplication instead of OfflineApplication
static boost::shared_ptr<TriggeredWriter> twriter;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

static int
mainloop()
{
	std::string outfile = twriter->flush();
	return 0;
}


/*
 * When running live, we'll need to keep track of xruns.  This
 * callback is copied from writer.cc
 */
int 
log_xrun(float usec)
{
	logv << logv.allfields << "xrun: " << usec << " us" << std::endl;
	return 0;
}


void 
signal_handler(int sig)
{
	if (sig != SIGINT)
		ret = EXIT_FAILURE;

	app->signal_quit();
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		// Derive CaptureOptions from JillOptions instead of OfflineOptions
		CaptureOptions<JillOptions> options("capture", "1.0.0rc4");
		options.parse(argc,argv,"capture.ini");

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		// AudioInterfaceJack instead of AudioInterfaceOffline; different arguments
		AudioInterfaceJack client(options.client_name, AudioInterfaceJack::Sink);
		logv << logv.allfields << "Started client; samplerate " << client.samplerate() << endl;

		options.adjust_values(client.samplerate());
		filters::WindowDiscriminator<sample_t> wd(options.open_threshold,
							  options.open_count_thresh, 
							  options.open_crossing_periods,
							  options.close_threshold,
							  options.close_count_thresh, 
							  options.close_crossing_periods,
							  options.period_size);
		
		util::multisndfile writer(options.output_file_tmpl, client.samplerate());
		twriter.reset(new TriggeredWriter(wd, writer, logv,
						  options.prebuffer_size,
						  client.samplerate() * 2));

		client.set_process_callback(boost::ref(*twriter));
		client.set_xrun_callback(log_xrun); // register xrun logging function

		logv << logv.program << "output template: " << options.output_file_tmpl << endl
		     << logv.program << "sampling rate: " << client.samplerate() << endl
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


		// JillApplication instead of OfflineApplication; connect to input ports
 		app.reset(new JillApplication(client, logv));
		app->connect_inputs(options.input_ports);
		app->set_mainloop_callback(mainloop);
		app->run(250000); // different arguments to mainloop
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
