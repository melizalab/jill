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
 * This program is used to test the window discriminator
 * offline. Instead of taking input from the JACK server, it processes
 * a wav file supplied on the command line.
 */
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Instead of application.hh, we include offline_application.hh, which
 * has several offline equivalents of the usual JILL classes. We also
 * include the header for the base triggered-writer classes.
 */
#include "jill/offline_application.hh"
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
		options.parse(argc,argv);
		if (options.input_file == "") {
			std::cerr << "Error: input file (-i) required" << std::endl;
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
		     << std::endl;

		/*
		 * Now we initialize our custom processor and all of its
		 * subsidiary objects.
		 */
		options.adjust_values(client.samplerate());
		std::cout << options << std::endl;
		filters::WindowDiscriminator<sample_t> wd(options.open_threshold,
							  options.open_count_thresh, 
							  options.open_crossing_periods,
							  options.close_threshold,
							  options.close_count_thresh, 
							  options.close_crossing_periods,
							  options.period_size);
		
		util::multisndfile writer(options.output_file_tmpl, client.samplerate());
		twriter.reset(new TriggeredWriter(wd, writer, 
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
// #ifndef NDEBUG
// 		wd.print_counters(std::cout);
// #endif
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
