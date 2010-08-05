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
 * a separate file. This program runs the class offline, with a wave
 * file as input.  Adapting the class to use live JACK input once it's
 * debugged is trivial.
 *
 * This program's task is similar to writer.cc, but instead of just
 * writing every sample to disk, we want to only write data when
 * something is going on.  We'd like to store each recording episode
 * as a separate file and name them sequentially.
 *
 * Looking through the JILL classes, we see that there's a class
 * WindowDiscriminator in filters/window_discriminator.hh, but it
 * doesn't have any mechanism for storing the data or writing it to
 * disk.  So we need to write a custom class.  At this point, open
 * trigger_classes.hh.
 *
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Instead of application.hh, we include offline_application.hh, which
 * has several offline equivalents of the usual JILL classes. We also
 * include the header for the base triggered-writer classes.
 */
#include "jill/offline_application.hh"
#include "jill/offline_options.hh"
#include "jill/util/logger.hh"
#include "jill/util/multisndfile.hh"
#include "twriter_options.hh"
#include "trigger_writer.hh"
using namespace jill;

/*
 * The Application is derived from OfflineApplication instead of
 * JillApplication. We also declare a pointer to our custom
 * TriggerWriter class (which will be initialized after we read in the
 * options.
 */
static boost::scoped_ptr<OfflineApplication> app;
static boost::shared_ptr<TriggeredWriter> twriter;
static util::logstream logv;
static int ret = EXIT_SUCCESS;

/*
 * Note that the process function isn't here any more; it's
 * encapsulated in the TriggeredWriter class.  However, we do still
 * need to write a function that will be called in the main loop.  In
 * theory, we could pass a pointer to the flush() function directly to
 * the Application class, but there's some logging to be done.
 */
static int
mainloop()
{
	std::string outfile = twriter->flush();
	return 0;
}

static void signal_handler(int sig)
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

		/*
		 * This is somewhat complicated.  We use the
		 * TriggerOptions class from trigger_writer.hh, which
		 * handles parsing the twriter-specific options.
		 * However, we want to use the options specific to the
		 * offline version of the application, which derive
		 * from OfflineOptions.  This is why the
		 * TriggerOptions class has parameterized inheritance.
		 */
		TriggerOptions<OfflineOptions> options("twriter_test", "1.0.0rc3");
		options.parse(argc,argv,"twriter.ini");
		if (options.input_file == "") {
			cerr << "Error: input file (-i) required" << endl;
			throw Exit(EXIT_FAILURE);
		}

		logv.set_program("twriter_test");
		logv.set_stream(options.logfile);

		/*
		 * Now we need to set up the client. The interface is
		 * AudioInterfaceOffline instead of AudioInterfaceJack
		 * because this is the offline test program.  Instead
		 * of connecting to input ports, we specify the input
		 * file.
		 */
		AudioInterfaceOffline client(options.blocksize, options.samplerate);
		client.set_input(options.input_file);
		logv << logv.allfields << "Opened input file " << options.input_file 
		     << " (size = " << client.frames() << "; Fs = " << client.samplerate() << ")" 
		     << endl;

		/*
		 * Now we initialize our custom processor and all of its
		 * subsidiary objects.
		 */
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
		/*
		 * Here's where we pass the processing object to the
		 * client. The semantics are a little tricky, because
		 * twriter is a shared pointer (which we need in order
		 * to dynamically allocate it here).  The * operator
		 * dereferences the pointer, which we then wrap in
		 * boost::ref in order to ensure that the object is
		 * passed by reference - otherwise it gets copied
		 */
		client.set_process_callback(boost::ref(*twriter));

		/* Log parameters */
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

		/*
		 * Finally, the Appplication is created and
		 * started. For this offline version,
		 * OfflineApplication is used instead of
		 * JillApplication.  The run() function will exit when
		 * there is no more data in the input file to process.
		 */
 		app.reset(new OfflineApplication(client, logv)); 
		app->set_mainloop_callback(mainloop);
 		app->run();
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
