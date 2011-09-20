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
 * Data can be stored to an ARF file, or to any file format supported
 * by libsndfile. These formats are supplied by different classes that
 * inherit from Sndfile.  Because of this inheritance relationship, we
 * can use polymorphism, which was introduced in jill_mixer. We access
 * the class through a pointer to the base class and then decide at
 * runtime which actual class will be used.
 *
 * We'll also see how to subclass Options to handle positional
 * arguments.
 */
#include <boost/scoped_ptr.hpp>
#include <iostream>
#include <signal.h>

/*
 * As before, include the relevant JILL headers
 */
#include "jill/simple_client.hh"
#include "jill/jill_options.hh"
#include "jill/util/logger.hh"
#include "jill/util/string.hh"

/*
 * After including the JILL headers we used in mixer, we also add some
 * headers related to handling disk IO.  The buffer_adapter.hh header
 * gives us access to the BufferedWriter class; we also need the
 * Ringbuffer class, from ringbuffer.hh, the ArfSndfile class from
 * arf_sndfile.hh, and the SimpleSndfile class from simple_sndfile.hh
 */
#include "jill/util/buffer_adapter.hh"
#include "jill/util/ringbuffer.hh"
#include "jill/util/arf_sndfile.hh"
#include "jill/util/simple_sndfile.hh"
using namespace jill;


/**
 * First, let's set up options. Rather than using the default
 * JillOptions class, we're going to create a subclass.  This allows
 * us to override the process_options() member function so that we can
 * deal with the positional argument, which doesn't have any flags.
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
		// First, we need to call the base class's process_options() function
		JillOptions::process_options();
		// Then, we process our custom option
		if (!assign(output_file, "output_file")) {
			std::cerr << "Error: missing required output file name " << std::endl;
			throw Exit(EXIT_FAILURE);
		}
	}

	void print_usage() {
		std::cout << "Usage: " << _program_name << " [options] output_file\n\n"
			  << "output_file can be an ARF file or any file format supported by libsndfile\n"
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
static boost::scoped_ptr<SimpleClient> client;
static util::logstream logv;
static int ret = EXIT_SUCCESS;
static util::BufferedWriter<util::Ringbuffer<sample_t>, util::Sndfile> output(50000);

/**
 * The process function is similar to the ones in the previous
 * chapters. However, because this module is a Sink, the out buffer is
 * not used. The incoming samples are passed to the RingBufferAdapter,
 * which saves them in the ringbuffer.  If the ringbuffer is full, it
 * will not be able to write all the samples.  This is a serious
 * error, so we throw an exception.  This will be caught by the caller
 * of this function and the application will exit gracefully.  The
 * control port is ignored.
 */
void
process(sample_t *in, sample_t *out, sample_t *, nframes_t nframes, nframes_t time)
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

	client->stop("Received interrupt");
}

int
main(int argc, char **argv)
{
	using namespace std;
	try {
		// parse options, using our custom options class
		WriterOptions options("jill_writer", "1.1.0rc3");
		options.parse(argc,argv);

		// fire up the logger
		logv.set_program(options.client_name);
		logv.set_stream(options.logfile);

		/*
		 * Start up the client. One key difference is that
		 * this client doesn't have an output port, so we can
		 * omit that argument. We also register our
		 * xrun-logging function using set_xrun_callback.
		 */
		client.reset(new SimpleClient(options.client_name.c_str(), "in"));
		logv << logv.allfields << "Started client; samplerate " << client->samplerate() << endl;
		for (map<string,string>::const_iterator it = options.additional_options.begin();
		     it != options.additional_options.end(); ++it)
			logv << logv.program << "  " << it->first << "=" << it->second << endl;
		client->set_xrun_callback(log_xrun);

		/*
		 * Open the output file here. Here's where we make a
		 * runtime decision which class to use. We use the
		 * JACK server's samplerate to specify the samplerate
		 * of the output file. We then pass a pointer to the
		 * output file to the RingbufferAdapter so it has
		 * somewhere to write the data.
		 */
		logv << logv.allfields << "Opening " << options.output_file
		     << " for output; Fs = " << client->samplerate() << endl;
		boost::scoped_ptr<util::Sndfile> outfile;
		if (util::get_filename_extension(options.output_file)=="arf")
			outfile.reset(new util::ArfSndfile(options.output_file, client->samplerate()));
		else
			outfile.reset(new util::SimpleSndfile(options.output_file, client->samplerate()));
		/* Some sndfile classes require next() to be called before writing */
		outfile->next("")->set_attributes(options.additional_options);
		output.set_sink(outfile.get());

		signal(SIGINT,  signal_handler);
		signal(SIGTERM, signal_handler);
		signal(SIGHUP,  signal_handler);

		/* Connect the inputs */
		vector<string>::const_iterator it;
		for (it = options.input_ports.begin(); it != options.input_ports.end(); ++it) {
			client->connect_port(it->c_str(), "in");
			logv << logv.allfields << "Connected input to port " << *it << endl;
		}

		/*
		 * This is where we tell the client what function to
		 * call while running the main loop.  This is
		 * important because it's what flushes samples from
		 * the ringbuffer to disk, and prevents an overrun.
		 * Try commenting out this line and see what happens.
		 */
		client->set_mainloop_callback(mainloop);
		client->set_mainloop_delay(250000);
		client->set_process_callback(process);
		client->run();
		logv << logv.allfields << client->get_status() << endl;
		logv << logv.allfields << "Total frames written: " << outfile->nframes() << endl;
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
