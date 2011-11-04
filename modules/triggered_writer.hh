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
 * This file is a C++ header file.  Its function is to declare the
 * interface of our class, which will be responsible for analyzing
 * incoming samples and deciding whether to write them to disk. The
 * implementation of the class is declared separately, in
 * trigger_writer.cc (which we'll visit later).
 *
 * The client defined here, TriggeredWriter, is a sink that either
 * writes data in the input stream to disk, or stores it in a
 * prebuffer, depending on whether the signal in the control port is
 * greater than some threshold.  Instances of this class can act as
 * function objects, which means they can be registered as callbacks
 * in JILL.
 *
 */
#ifndef _TRIGGERED_WRITER_HH
#define _TRIGGERED_WRITER_HH

/* include various JILL headers */
#include <jill/types.hh>
#include <jill/util/ringbuffer.hh>
#include <jill/util/sndfile.hh>

#include <boost/thread/mutex.hpp>
#include <boost/scoped_array.hpp>
#include <map>

/*
 * This header lets us declare the TriggeredWriter class noncopyable;
 * this prevents downstream users from accidentally creating a copy of
 * any TriggeredWriter objects, an operation we don't want to support.
 */
#include <boost/noncopyable.hpp>

namespace jill {

/*
 * The logstream class is used to indicate when files are created and closed.
 * Rather than include the logger.hh header, which would slow down compilation,
 * we make do with a simple forward declaration.
 */
namespace util {class logstream;}

/*
 * Here is where we define our processing class.  We're going to take
 * advantage of the fact that we can pass a function *object* to the
 * AudioInterface::set_process_callback function; this allows us to
 * encapsulate all the variables needed for the processing logic into
 * a single structure.
 *
 * Note that although this is a definition of the class, we are only
 * declaring the member functions. Their implementations will be in
 * triggered_writer.cc
 *
 * The class derives from boost::noncopyable to prevent copying.
 */
class TriggeredWriter : boost::noncopyable {

/*
 * Members defined as public are accessible from outside the
 * class. This allows us to protect the state of the object
 * and simplify the users' interaction.
 */
public:

	/**
	 * The constructor of the class allocates memory and sets up
	 * any internal variables.  The arguments to the constructor
	 * are as follows:
	 *
	 * @param writer          A reference to a file output object. We
	 *                        need one that can write to multiple files.
         * @param logger          A reference to a logstream object, for logging
         *                        start and stop information.
	 * @param prebuffer_size  The size of the prebuffer, in samples
	 * @param buffer_size     The size of the process buffer, in samples
	 * @param sampling_rate   The client sampling rate
	 * @param trig_thresh     The breakpoint for the trigger signal
	 * @param entry_attrs     If not NULL, set attributes on newly created entries
	 */
	TriggeredWriter(util::Sndfile &writer, util::logstream &logger,
			nframes_t prebuffer_size,
			nframes_t buffer_size,
			nframes_t sampling_rate,
			sample_t trig_thresh,
			std::map<std::string, std::string> * entry_attrs=0);

	/**
	 * This function overloads the () operator; that is, if we
	 * create an object "tw" of type TriggeredWriter, and call
	 * tw(), this function gets called.  Since we'll pass a
	 * TriggeredWriter object to
	 * AudioInterface::set_process_callback, this is what will get
	 * run in the real-time thread. It has the same signature as
	 * the process functions we wrote in previous examples.
	 */
	void operator() (sample_t *in, sample_t *, sample_t *trig, nframes_t nframes, nframes_t time);

	/**
	 * We also need a function that can be run in the main loop of
	 * the program to flush data to disk.  Note that because
	 * different threads are going to be using this class, we need
	 * to be certain that all of the public functions are
	 * "re-entrant" - if they are called concurrently by two
	 * different threads, the internal state of the object will
	 * remain consistent.
	 *
	 * @returns the path of the file we wrote to
	 */
	std::string flush();

	/**
	 * Close the current entry (if open) and log the stop time
	 *
	 * @param time  the time when the gate closed (default the last time data was pushed to the buffer)
	 */
	void close_entry(nframes_t time);
	void close_entry();


/*
 * In the protected part of the class interface, we define
 * functions that perform well-defined operations, but which
 * should not be accessible from outside the class.
 */
protected:

	/**
	 * Open a new entry and log the start time
	 *
	 * @param time  the time when the gate opened
	 * @param prebuf  the number of samples in the prebuffer
	 */
	void next_entry(nframes_t time, nframes_t prebuf);

/*
 * Variables declared in this section are not visible outside
 * of the class member functions; this is where we store any
 * data we want to encapsulate in the class.  Any objects that
 * will be accessed by both operator() and flush() need to be
 * threadsafe.
 */
private:
	/// Ringbuffer for data
	util::Ringbuffer<sample_t> _data;
	/// Ringbuffer for trigger
	util::Ringbuffer<sample_t> _trig;
	/// Store the time of the last sample written to the ringbuffer
	nframes_t _time;
	/// Mutex for synchronizing access to ringbuffers and time variable
	boost::timed_mutex _mux;

	/* all objects below here are only accessed by the non-realtime thread */

	/// these buffers are used to copy out data from the ringbuffer.
	nframes_t _buffer_size;
	boost::scoped_array<sample_t> _databuf, _trigbuf;

	/// Prebuffer for temporary storage of samples before trigger
	util::Prebuffer<sample_t> _prebuf;

	/// Reference to output file
	util::Sndfile &_writer;

	/// Reference to a logstream to provide fine-scaled timestamp info
	util::logstream &_logv;

	sample_t _trig_thresh;
	bool _last_state;
	nframes_t _samplerate;
	std::map<std::string, std::string> *_entry_attrs;
};

/*
 * As you can see, the definition of the class is fairly simple. Users
 * that want to use the class only need to see this file, as it
 * describes the interface for interacting with objects of type
 * TriggeredWriter.  The implementations of the functions are defined in
 * triggered_writer.cc, which you should open at this point.
 */

}

#endif
