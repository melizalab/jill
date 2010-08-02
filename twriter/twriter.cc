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
 * a separate file. This allows us to test the processor code as a separate
 * module without needing to run JACK.
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
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers. We've added a new
 * header, bufered_sndfile.hh, which gives us access to the
 * BufferedSndfile class.
 */
#include "jill/main.hh"
#include "jill/application.hh"
#include "jill/buffered_sndfile.hh"
#include "jill/util/logger.hh"
using namespace jill;


/*
 * We've added an additional module-scope variable, a BufferedSndfile
 * instance. This object needs to be at this scope so that it can be
 * accessed by the real-time and main threads.  We initialize it with
 * enough room for 500k samples, which is enough to buffer over 10s of
 * sound at CD-quality rates.
 */
static util::logstream logv;
static boost::scoped_ptr<Application> app;
static int ret = EXIT_SUCCESS;
static BufferedSndfile<sample_t> sndfile(500000);
static filters::WindowDiscriminator<sample_t,nframes_t> wd(0.5, 5, 10, 0.5, 5, 10, 100);

/**
 * The process function is similar to the ones in the previous
 * chapters. However, because this module is a Sink, the out buffer is
 * not used. The incoming samples are passed to the BufferedSndfile,
 * which saves them in the ringbuffer.  If the ringbuffer is full, it
 * will not be able to write all the samples.  This is a serious
 * error, so we throw an exception.  This will be caught by the caller
 * of this function and the application will exit gracefully.
 */
void
process(sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	std::cout << nframes << " @ " << time << std::endl;
}


/* 
 * We now introduce another callback function.  This function is
 * called by the Application class, which runs in the main thread of
 * the program.  Any actions that don't require realtime priority
 * should be placed in this loop.  In this program, we use the main
 * thread to write data from the BufferedSndfile to disk, which is
 * done by calling the () operator on the buffer.  Note that this
 * makes the sndfile object itself a function object, so it could be
 * used as the callback (see below).
 * 
 * The function returns 0 if writing was successful, and a nonzero
 * code if it doesn't.  Returning anything other than 0 causes the
 * main loop to terminate.
 */
int 
mainloop()
{
	return 0;//return sndfile();
}


/*
 * This is yet another callback. It gets called whenever there is an
 * xrun.  An xrun can be an overrun, when the real-time thread is not
 * able to deal with all the samples before it gets new ones, or an
 * underrun, when the thread is not able to supply enough output
 * samples in time.  Xruns tend to mean skips in the sound, and in the
 * case of this application, missing data.  We use this callback to report xruns to the logfile so that we can later figure out if and where bad data were generated.
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
		cout << wd << endl;
		// parse options, using our custom options class
		WriterOptions options("writer", "1.0.0rc2");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * Start up the client. One key difference is that
		 * this client doesn't have an output port, so we
		 * specify it as a Sink.
		 */
		logv << logv.allfields << "Starting client" << endl;
		AudioInterfaceJack client(options.client_name, AudioInterfaceJack::Sink);
		client.set_process_callback(process);

		/*
		 * Open the output file here. As in the previous
		 * chapter, we use the JACK server's samplerate to
		 * specify the samplerate of the output file.
		 */
		logv << logv.allfields << "Opening " << options.output_file 
		     << " for output; Fs = " << client.samplerate() << endl;
		sndfile.open(options.output_file.c_str(), client.samplerate());

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		app.reset(new Application(client, logv));
		app->connect_inputs(options.input_ports);
		app->set_mainloop_callback(mainloop);

		/*
		 * Equivalently, we can pass the sndfile object to the
		 * set_mainloop_callback function.  However, as noted
		 * in the audio_interface.hh header, the function
		 * object is *copied*.  The BufferedSndFile won't
		 * allow us to copy it, so it would throw an error.
		 * To get around this we need to use a reference to
		 * the object.
		 */
		// app->set_mainloop_callback(boost::ref(sndfile));

		/*
		 * Writing data is fairly intensive and benefits from
		 * lots of buffering, so we specify a nice long delay
		 * between main loops.
		 */
		app->run(1000000);
		logv << logv.allfields << "Total frames written: " << sndfile.nframes() << endl;
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
