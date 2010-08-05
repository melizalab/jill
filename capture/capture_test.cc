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
 * and we'd like to be able to test it offline as well as running it
 * as a JACK client.  Therefore, rather than write the process and
 * main loop functions here, we specify them in a separate class,
 * which can be used by any client.
 *
 * At this point, open trigger_writer.hh to see how to define the
 * class.
 * 
 * Another task common to both the offline and online versions of this
 * program is parsing the options that control the behavior of the
 * window discriminator.  As in writer.cc, we subclass Options to add
 * our custom options.  This subclass is defined in
 * capture_options.hh; open this file to see how the options are
 * parsed.
 *
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>


/*
 * The JILL framework provides several alternative headers that should
 * be used in an offline application. Instead of jill_application.hh
 * and jill_options.hh, we include offline_application.hh and
 * offline_options.hh.  We also include headers for the logger and
 * soundfile writer, as well as the headers for the custom classes we
 * wrote.
 */
#include "jill/offline_application.hh"
#include "jill/offline_options.hh"
#include "jill/util/logger.hh"
#include "jill/util/multisndfile.hh"
#include "capture_options.hh"
#include "trigger_writer.hh"
using namespace jill;

/*
 * This section is similar to writer.cc, except the app object is of
 * type OfflineApplication instead of JillApplication.  We also
 * declare a pointer to our custom TriggerWriter class (which will be
 * initialized after we read in the options.
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
 * the Application class, but in many cases writing a wrapper function
 * is much simpler.
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
		 * Here we use our custom CaptureOptions class. As
		 * noted in capture_options, this class uses
		 * parameterized inheritance, so we have to specify
		 * the base class from which it will inherit.  For an
		 * offline application, we use OfflineOptions (defined
		 * in offline_options.hh).  Also, note that we pass an
		 * additional argument to parse(), which checks for a
		 * file "capture.ini" in the current directory and
		 * parses it if it exists.
		 */
		CaptureOptions<OfflineOptions> options("capture_test", "1.0.0rc3");
		options.parse(argc,argv,"capture.ini");

		/*
		 * The offline client reads data from a sound file
		 * instead of from the JACK server.  Therefore, it's
		 * necessary for the user to specify this file on the
		 * command line.
		 */
		if (options.input_file == "") {
			cerr << "Error: input file (-i) required" << endl;
			throw Exit(EXIT_FAILURE);
		}

		logv.set_program("capture_test");
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
		 * The TriggeredWriter class requires references to a
		 * WindowDiscriminator and a multisndfile. We
		 * instantiate these objects first, using the values
		 * in the options.  Recall that twriter is a smart
		 * pointer, so the object it points to is allocated on
		 * the heap (with new), in contrast to the wd and
		 * writer objects, which are allocated on the stack.
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
		 * passed by reference - otherwise
		 * set_process_callback tries to copy it (which will
		 * raise a compiler error because TriggeredWriter is
		 * noncopyable)
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

/*
 * To test the capture client, run it as follows:
 *
 * ./capture_test -i <test.wav>
 *
 * where <test.wav> is a recording of the kinds of signals you expect
 * to get. The logged output will indicate where the window
 * discriminator opened and closed. Adjust the gate options until it
 * opens at the beginning of the signals of interest, and closes when
 * they end.  The options can be stored in capture.ini.
 *
 * At this point, open capture.cc to see how to adapt the client to
 * online use.
 */
