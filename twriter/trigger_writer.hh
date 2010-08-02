/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *! @file
 *! @brief Helper classes for the triggered_writer
 *! 
 */
#ifndef _TRIGGER_CLASSES_HH
#define _TRIGGER_CLASSES_HH

/*
 * We need to import some classes from JILL. We'll be using the window
 * discriminator, the ringbuffer, and the soundfile writer.
 */
#include "jill/audio.hh"
#include "jill/options.hh"
#include "jill/filters/window_discriminator.hh"
#include "jill/util/sndfile.hh"
#include "jill/util/ringbuffer.hh"

/*
 * These include statements bring in some other classes we need.
 */
#include <boost/noncopyable.hpp>

/* 
 * We place the triggered writer into the jill namespace to simplify
 * access to other members of that namespace
 */
namespace jill {

/*
 * Here is where we define our processing class.  We're going to take
 * advantage of the fact that we can pass a function *object* to the
 * JACK client; this allows us to encapsulate all the variables needed
 * for the processing logic into a single structure.
 *
 * Our process loop needs to pass the data to the window
 * discriminator, which will let us know when to start and stop
 * recording.  We need a ringbuffer to store data for output to disk.
 * We also want to be able to pre-buffer data; that is, to be able to
 * look back some time in the past before the trigger was activated,
 * and write those samples to disk, too.
 *
 * The actual implementations of these functions will be in
 * trigger_classes.cc
 */
class TriggeredWriter : boost::noncopyable {

public:
	/**
	 * The constructor of the class allocates memory and sets up
	 * any internal variables.  The arguments to the constructor are as follows:
	 *
	 * @param wd      A reference to a window discriminator. Rather
	 *                than creating the object here, which would require the 
	 *                this class to know all the constructor parameters, we
	 *                initialize the object elsewhere and store a reference to it.
	 * @param writer  An reference to a file output object. We use one that allows
	 *                us to write to multiple files.
	 * @param prebuffer_size  The size of the prebuffer, in samples
	 * @param buffer_size     The size of the process buffer, in samples
	 */
	TriggeredWriter(filters::WindowDiscriminator<sample_t> &wd, 
			util::multisndfile &writer,
			nframes_t prebuffer_size, nframes_t buffer_size);

	/**
	 * This function overloads the () operator; that is, if we
	 * create an object "tw" of type TriggeredWriter, and call
	 * tw(), this function gets called.  It has the same signature
	 * as the process functions we wrote in previous examples.
	 */
	void operator() (sample_t *in, sample_t *out, nframes_t nframes, nframes_t time);

	/**
	 * We also need a function that can be run in the main loop of
	 * the program to flush data to disk.  Note that because
	 * different threads are going to be using this class, we need
	 * to be certain that all of the public functions are
	 * "re-entrant" - if they are called concurrently by two
	 * different threads, the internal state of the object will
	 * remain consistent.
	 *
	 * @returns the name of the file we wrote to
	 */
	const std::string &flush();

private:
	/*
	 * Variables declared in this section are not visible outside
	 * of the class member functions; this is where we store any
	 * data we want to encapsulate in the class.  Any objects that
	 * will be accessed by both operator() and flush() need to be
	 * threadsafe.
	 */

	/// This is our reference to the window discriminator
	filters::WindowDiscriminator<sample_t> &_wd;

	/// This is our reference to the soundfile writer
	util::multisndfile &_writer;
	// a typedef for pointers to the writer's write function
	typedef util::multisndfile::size_type (util::multisndfile::*writefun_t)(const sample_t *,
										util::multisndfile::size_type);

	/// A ringbuffer that the process thread can write to
	util::Ringbuffer<sample_t> _ringbuf;
	/// The prebuffer
	util::Prebuffer<sample_t> _prebuf;
};

/**
 * Another way of unifying our testing and the final module is to have
 * a common options class. As in the previous chapter, we'll derive
 * from Options, but in this case the class will be defined here.
 *
 * Note that unlike in the previous chapter, we're only declaring the
 * class. The implementations of the functions are found in
 * trigger_classes.cc
 */
class TriggerOptions : public Options {

public:
	TriggerOptions(const char *program_name, const char *program_version);

	std::string output_file_tmpl;

	sample_t open_threshold;
	sample_t close_threshold;

	float open_crossing_rate;
	float close_crossing_rate;

	nframes_t period_size;

	nframes_t open_crossing_periods;
	nframes_t close_crossing_periods;

protected:

	void process_options();
	void print_usage();

};

}

#endif
