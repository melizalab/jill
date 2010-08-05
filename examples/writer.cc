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
 * This example is the third chapter in the JILL tutorial.  We will
 * learn how to implement a function that runs in the main loop.  The
 * main loop runs with a lower priority, and any tasks that do not
 * need realtime priority should be run here.  This includes any file
 * or network I/O, which can take an indeterminate amount of time.  
 * 
 * As in the previous chapter, we need somewhere to store the data,
 * but in this case we need a structure that can be safely accessed by
 * multiple threads.  This is called a ringbuffer, which has separate
 * read and write pointers.  The realtime thread uses the write
 * pointer to store data, and the slower I/O thread uses the write
 * pointer.  As long as the write process can deal with large chunks
 * of data and keep up with the faster process, the buffer will
 * prevent underruns.
 *
 * We'll also see how to subclass Options to handle positional
 * arguments.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * Here we include the relevant JILL headers. We've added a new
 * header, ringbuffer.hh, which gives us access to the
 * RingbufferAdapter class, and sndfile.hh, which gives use access to
 * sound-file writing backends.
 */
#include "jill/jill_application.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"

#include "jill/util/buffer_adapter.hh"
#include "jill/util/ringbuffer.hh"
#include "jill/util/sndfile.hh"
using namespace jill;


/**
 * First, we need to subclass Options to handle the additional
 * command-line options for this module.  The additional option is
 * positional; that is, it doesn't have any flags, which means we have
 * to add it to the positional options list.
 */
class WriterOptions : public JillOptions {

public:
	WriterOptions(const char *program_name, const char *program_version)
		: JillOptions(program_name, program_version) { // this calls the superclass constructor
		cmd_opts.add_options()
			("output_file", po::value<std::string>(), "set output file");
		pos_opts.add("output_file", -1);
	} 

	std::string output_file;

protected:

	void process_options() {
		if (!assign(output_file, "output_file")) {
			std::cerr << "Error: missing required output file name " << std::endl;
			throw Exit(EXIT_FAILURE);
		}
	}

	void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] output_file\n\n"
			  << "output_file can be any file format supported by libsndfile\n"
			  << visible_opts;
	}
};

/*
 * We've added an additional module-scope variable, the buffered
 * output.  This class is a wrapper around a backend, which is a
 * sndfile object that stores sample_t's. We could plug in any other
 * class that implements the same interface (as we'll see in Chapter
 * 4). The buffer needs to be at this scope so that it can be accessed
 * by the real-time and main threads.  We initialize it with enough
 * room for 50k samples, which is enough to buffer about 1s of sound
 * at CD-quality rates.  We connect it to an actual backend in main()
 * below.
 */
static util::logstream logv;
static boost::scoped_ptr<JillApplication> app;
static int ret = EXIT_SUCCESS;
static util::BufferAdapter<util::Ringbuffer<sample_t>, util::sndfile > output(50000);


/**
 * The process function is similar to the ones in the previous
 * chapters. However, because this module is a Sink, the out buffer is
 * not used. The incoming samples are passed to the RingBufferAdapter,
 * which saves them in the ringbuffer.  If the ringbuffer is full, it
 * will not be able to write all the samples.  This is a serious
 * error, so we throw an exception.  This will be caught by the caller
 * of this function and the application will exit gracefully.
 */
void
process(sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	nframes_t nf = output.push(in, nframes);
	if (nf < nframes)
		throw std::runtime_error("ringbuffer filled up");
}


/* 
 * We now introduce another callback function.  This function is
 * called by the Application class, which runs in the main thread of
 * the program.  Any actions that don't require realtime priority
 * should be placed in this loop.  In this program, we use the main
 * thread to write data from the RingbufferAdapter to disk, which is
 * done by calling the () operator on the buffer.
 * 
 * The function returns 0 if writing was successful, and a nonzero
 * code if it doesn't.  Returning anything other than 0 causes the
 * main loop to terminate.
 */
int 
mainloop()
{
	output.flush();
	return 0;
}


/*
 * This is yet another callback. It gets called whenever there is an
 * xrun.  An xrun can be an overrun, when the real-time thread is not
 * able to deal with all the samples before it gets new ones, or an
 * underrun, when the thread is not able to supply enough output
 * samples in time.  Xruns tend to mean skips in the sound, and in the
 * case of this application, missing data.  We use this callback to
 * report xruns to the logfile so that we can later figure out if and
 * where bad data were generated.
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
		// parse options, using our custom options class
		WriterOptions options("writer", "1.0.0rc3");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * Start up the client. One key difference is that
		 * this client doesn't have an output port, so we
		 * specify it as a Sink.  We also register our
		 * xrun-logging function using set_xrun_callback.
		 */
		logv << logv.allfields << "Starting client" << endl;
		AudioInterfaceJack client(options.client_name, AudioInterfaceJack::Sink);
		logv << logv.allfields << "Started client; samplerate " << client.samplerate() << endl;
		client.set_process_callback(process);
		client.set_xrun_callback(log_xrun);

		/*
		 * Open the output file here. As in the previous
		 * chapter, we use the JACK server's samplerate to
		 * specify the samplerate of the output file. We then
		 * pass a pointer to the output file to the
		 * RingbufferAdapter so it has somewhere to write the
		 * data.
		 */
		logv << logv.allfields << "Opening " << options.output_file 
		     << " for output; Fs = " << client.samplerate() << endl;
		util::sndfile outfile(options.output_file.c_str(), client.samplerate());
		output.set_sink(&outfile);

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		app.reset(new JillApplication(client, logv));
		app->connect_inputs(options.input_ports);
		app->set_mainloop_callback(mainloop);

		/*
		 * We start the main loop as before, but this time we
		 * explicitly specify the amount of time to wait
		 * between running the callback function.  Larger
		 * values reduce the load, but the ringbuffer has
		 * enough room for about 1s of data, so we need to
		 * make sure the data get flushed frequently enough to
		 * avoid an overrun.
		 */
		app->run(250000);
		logv << logv.allfields << "Total frames written: " << outfile.nframes() << endl;
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
