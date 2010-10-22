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
 * declared in in trigger_writer.hh (the interface).  The advantage
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
#include "trigger_writer.hh"
#include "jill/util/logger.hh"
/* The 'using' directive allows us to avoid prefixing all the objects in the jill namespace */
using namespace jill;


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
TriggeredWriter::TriggeredWriter(filters::WindowDiscriminator<sample_t> &wd, 
				 util::multisndfile &writer, util::logstream &logger,
				 nframes_t prebuffer_size, nframes_t buffer_size)
	: _wd(wd), _writer(writer), _logv(logger), _ringbuf(buffer_size, 0), 
	  _prebuf(prebuffer_size, &writer), _start_time(0) {}


/*
 * The function called by the process thread is quite simple. We don't
 * need to make real-time decisions about the state of the window, so
 * we just dump the data into the ringbuffer. This function is
 * essentially copied from writer.cc
 */
void 
TriggeredWriter::operator() (sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	if (_start_time==0) _start_time = time;
	nframes_t nf = _ringbuf.push(in, nframes, time - _start_time);
	// as in writer, we check for overrun (not enough room in the
	// ringbuffer for all the samples. However, rather than
	// throwing an error we log the overrun.
	if (nf < nframes)
		_logv << _logv.allfields << "ringbuffer overrun at frame " << time - _start_time << std::endl;
}



/** This typedef is for convenience; it's used in the flush() function */
typedef util::multisndfile::size_type (util::multisndfile::*writefun_t)(const sample_t *,
									util::multisndfile::size_type);

/*
 * The flush function is called by the main thread. It has several
 * jobs to do:
 *
 * 1. read samples from the ringbuffer
 * 2. push them to the window discriminator 
 * 3. depending on the state of the window discriminator, write 
 *    data to disk or to the prebuffer
 * 4. advance the ringbuffer read pointer
 *
 */
const std::string&
TriggeredWriter::flush()
{
	// Step 1. Read samples from the buffer.  Rather than copy the
	// data out of the buffer into another array, we just allocate
	// a pointer and then get the ringbuffer to point us at the
	// memory where the data are located.
	sample_t *buf = 0;
	nframes_t frames = _ringbuf.peek(&buf);
	if (frames==0) 
		return _writer.current_file();

	// Step 2. Pass samples to window discriminator; its state may
	// change, in which case the return value will be > -1 and
	// indicate the frame in which the gate opened or closed.
	int offset = _wd.push(buf, frames);

	// Step 3. Handle the data depending on the state of the
	// window discriminator.
	if (_wd.open()) {
		// 3A: gate is open. Test if the gate was already
		// opend (offset < 0) or opened in this iteration.
		if (offset < 0)
			// 3AA: gate was already open. Continue
			// writing to disk.
			_writer.write(buf, frames);
		else {
			// 3AB: gate opened in this iteration. offset
			// holds the time when it opened. Push data
			// before the open frame into the prebuffer
			// (we're just going to read it out again, but
			// the code is much simpler this way)
			_prebuf.push(buf, offset);

			// call next_entry, which instructs the
			// multisndfile writer to open a new entry,
			// and logs the time when the gate opened.
			next_entry(_ringbuf.get_time()+offset, _prebuf.read_space());

			// The prebuffer is a BufferAdapter and can be flushed to disk. 
 			_prebuf.flush();

			// Write data after the gate opened to the disk
			_writer.write(buf+offset, frames-offset);
		}
	}
	else {	
		// Step 3B: gate is closed; Test if the gate closed in
		// this iteration.
		if (offset < 0)
			// 3BA: gate was already closed. Continue
			// storing data in prebuffer.
			_prebuf.push(buf, frames);
		else {
			// 3BB: gate closed in this iteration. Write
			// remainder of data to disk; close entry;
			// write data after the close frame to the
			// prebuffer.
			_writer.write(buf, offset);
			close_entry(_ringbuf.get_time()+offset);
			_prebuf.push(buf+offset, frames-offset);
		}
	}
	// Step 4: Advance the ringbuffer. Now that we're done with
	// the data, we tell the ringbuffer to free up the space for
	// new samples.
	_ringbuf.advance(frames);
	return _writer.current_file();
}

/*
 * This protected function handles all the tasks associated with the
 * start of a new entry.
 */
void
TriggeredWriter::next_entry(nframes_t time, nframes_t prebuf)
{
	_logv << _logv.allfields << "Signal start @" << time << "; begin entry @" << time - prebuf;
	std::string s = _writer.next();
	if (s != "")
		_logv << "; writing to file " << s;
	_logv << std::endl;
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
		_writer.close();
	}
}

void
TriggeredWriter::close_entry()
{ 
	close_entry(_ringbuf.get_time()); 
}

/*
 * This concludes the implementation of the TriggerWriter class. Now
 * we're ready to use it in the offline test program.  Return to
 * capture_test.cc.
 */
