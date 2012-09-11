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
 * This is the file where we define the implementations of the classes
 * declared in in triggered_writer.hh (the interface).  The advantage
 * of separating the implementation from the interface is that the end
 * user doesn't need to see the implementation details.
 *
 * For example, we only ask the user to pass us a reference to a
 * soundfile writer. All the buffering is taken care of internally,
 * and we can change the details of how it's done without affecting
 * the interface.
 *
 * A second advantage of defining the implementation separately is
 * that it can form an independent compilation module.  We include the
 * header that defines the classes we want to implement, as well as
 * headers for any classes needed by the implementation (including
 * classes that were only forward-declared in the header)
 *
 */
#include "triggered_writer.hh"
#include "jill/util/logger.hh"
/* The 'using' directive allows us to avoid prefixing all the objects in the jill namespace */
using namespace jill;

/** Prototype for a time function we'll define later */
static void adjusted_entry_time(std::vector<boost::int64_t> & timestamp, double diff);

/*
 * This is the constructor for the TriggeredWriter class. The
 * constructor is called whenever a new object of type TriggerWriter
 * is created.  In addition to having a normal function body (which is
 * empty in this case), constructors can have an "initialization
 * list", which consists of a set of calls to the constructors of any
 * member variables.  It is generally preferable to use the
 * initialization list, as this avoids the creation of temporary
 * objects, and any member references (e.g.  the window discriminator,
 * the sound file writer, and the logger) have to be assigned here.
 * We also initialize the ringbuffer and prebuffer.
 */
TriggeredWriter::TriggeredWriter(util::Sndfile &writer, util::logstream &logger,
				 nframes_t prebuffer_size, nframes_t buffer_size,
				 nframes_t sampling_rate, sample_t trig_thresh,
				 std::map<std::string, std::string> *entry_attrs)
	: _data(buffer_size), _trig(buffer_size), _buffer_size(buffer_size),
	  _prebuf(prebuffer_size), _writer(writer),
	  _logv(logger), _trig_thresh(trig_thresh), _last_state(false),
	  _samplerate(sampling_rate), _entry_attrs(entry_attrs)
{
	_databuf.reset(new sample_t[buffer_size]);
	_trigbuf.reset(new sample_t[buffer_size]);
}


/*
 * The function called by the process thread is quite simple. We don't
 * need to make real-time decisions about the state of the window, so
 * we just dump the data into the ringbuffers. This function is
 * essentially copied from writer.cc
 */
void
TriggeredWriter::operator() (sample_t *in, sample_t *, sample_t *trig, nframes_t nframes, nframes_t time)
{
	nframes_t nf1, nf2;
	{
		boost::timed_mutex::scoped_lock lock(_mux);
		nf1 = _data.push(in, nframes);
		nf2 = _trig.push(trig, nframes);
		_time = time + nframes;
	}

	// as in writer, we check for overrun (not enough room in the
	// ringbuffer for all the samples. However, rather than
	// throwing an error we log the overrun.  This is a little
	// dangerous to do in the realtime thread.
	if ((nf1 < nframes) || (nf2 < nframes))
		_logv << _logv.allfields << "ringbuffer overrun at frame " << time << std::endl;
}


/*
 * The flush function is called by the main thread. It runs through
 * the trigger and data buffers and determines whether to write to
 * disk.  Prebuffering is implemented with a separate object, which
 * does add some extra copying.
 *
 */
std::string
TriggeredWriter::flush()
{
	// Step 1. Read samples from the buffers.  We need to lock to
	// keep the two ringbuffers and the time variable in sync
	nframes_t frames,nf2,time;
	{
		boost::system_time const timeout= boost::get_system_time() +
			boost::posix_time::seconds(1);
		boost::timed_mutex::scoped_timed_lock lock(_mux, timeout);
		if (lock.owns_lock()) {
			frames = _data.pop(_databuf.get(), _buffer_size);
			nf2 = _trig.pop(_trigbuf.get(), _buffer_size);
			time = _time;
		}
		else
			// jack server probably died
			throw std::runtime_error("jill_capture can't get lock from realtime thread");
	}
	if (frames!=nf2)
		throw std::runtime_error("jill_capture ringbuffers are different sizes");
	if (frames==0)
		return _writer.current_entry()->name();

	// Step 2. Run through the trigger signal and look for state
	// changes.  Whenever the state changes (or at the end of
	// signal), deal with the samples since the last state change.
	// Object stores state from last call to flush().
	nframes_t last_state_change = 0;
	bool state;
	for (nframes_t i = 0; i < frames; ++i) {
		state = (_trigbuf[i] > _trig_thresh);
		if (state != _last_state) {
			// handle changes in gate state
			sample_t * buf = _databuf.get() + last_state_change;
			if (state) {
				// state is true, _last_state is false -> newly opened gate
				// push remaining pre-trigger data to prebuffer
				_prebuf.push(buf, i - last_state_change);

				// call next_entry, which instructs the
				// multisndfile writer to open a new entry,
				// and logs the time when the gate opened.
				next_entry(time+i, _prebuf.read_space());

				// flush the prebuffer
				_prebuf.pop(_writer);
			}
			else {
				// _last_state is true, state is false -> newly closed gate
				// Step 2B: gate is closed. Test if it just closed.
				// 2BB: gate closed in this iteration. Write
				// remainder of data to disk; close entry
				_writer(buf, i - last_state_change);
				close_entry(time+i);
			}
			// update variables for previous state
			last_state_change = i;
			_last_state = state;
		}
	} // for

	// handle end of signal
	sample_t * buf = _databuf.get() + last_state_change;
	if (state) {
		// 2AA: gate was already open. Continue writing to disk
		_writer(buf, frames - last_state_change);
	}
	else {
		// 2BA: gate was already closed. Continue
		// storing data in prebuffer.
		_prebuf.push(buf, frames - last_state_change);
	}
	// update variables for previous state
	last_state_change = frames;
	_last_state = state;

	return _writer.current_entry()->name();
}

/*
 * This protected function handles all the tasks associated with the
 * start of a new entry.
 */
void
TriggeredWriter::next_entry(nframes_t time, nframes_t prebuf)
{
	std::vector<boost::int64_t> timestamp;
	adjusted_entry_time(timestamp, 1.0 / _samplerate * prebuf);
	_logv << _logv.allfields << "Signal start @" << time << "; begin entry @" << time - prebuf;
	util::Sndfile::Entry *entry = _writer.next("");
	entry->set_attribute("sample_count",time);
	entry->set_attribute("signal_trigger",time - prebuf);
	entry->set_attribute("timestamp", timestamp);
	if (_entry_attrs)
		entry->set_attributes(*_entry_attrs);
	_logv << "; writing to " << entry->name() << std::endl;
}

/*
 * This protected function handles all the tasks associated with the
 * end of an entry.
 */
void
TriggeredWriter::close_entry(nframes_t time)
{
	if (_writer) {
		_logv << _logv.allfields << "Signal stop  @" << time << std::endl;
	}
}

void
TriggeredWriter::close_entry()
{
	close_entry(_time);
}

/*
 * This concludes the implementation of the TriggerWriter class. Now
 * we're ready to use it in the offline test program.  Return to
 * capture_test.cc.
 */

/*
 * This concludes the implementation of the TriggerWriter class. Now
 * we're ready to use it in the offline test program.  Return to
 * capture_test.cc.
 */

void adjusted_entry_time(std::vector<boost::int64_t> & timestamp, double diff)
{
	struct timeval tp;
	gettimeofday(&tp,0);
	long sec = long(floor(diff));
	timestamp.resize(2);
	timestamp[0] = tp.tv_sec - sec;
	timestamp[1] = tp.tv_usec - long(floor((diff - sec)*1000000));
	if (timestamp[1] < 0)
	{
		timestamp[1] += 1000000;
		timestamp[0] -= 1;
	}
}
