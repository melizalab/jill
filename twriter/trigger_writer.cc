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
#include "trigger_writer.hh"
#include "jill/util/logger.hh"
#include "jill/util/sndfile.hh"
#include <boost/bind.hpp>
using namespace jill;

/*
 * This is the file where we define the implementations of the classes
 * declared in in trigger_writer.hh (the interface).  The advantage
 * of separating the implementation from the interface is that the end
 * user doesn't need to see the implementation details.
 *
 * For example, we only ask the user to pass us a reference to a
 * soundfile writer. All the buffering is taken care of internally,
 * and we can change the details of how it's done without affecting
 * the interface.
 */

/// a typedef for pointers to the writer's write function
typedef util::multisndfile::size_type (util::multisndfile::*writefun_t)(const sample_t *,
									util::multisndfile::size_type);

/*
 * This is the constructor for the TriggeredWriter class. We have to
 * assign the references in the initialization list (it's not
 * otherwise possible to reseat them).  We also initialize the
 * ringbuffer and store the size of the prebuffer.
 */
TriggeredWriter::TriggeredWriter(filters::WindowDiscriminator<sample_t> &wd, 
				 util::multisndfile &writer, util::logstream &logger,
				 nframes_t prebuffer_size, nframes_t buffer_size)
	: _wd(wd), _writer(writer), _logv(logger), _ringbuf(buffer_size, 0), _prebuf(prebuffer_size) {}

/*
 * The function called by the process thread is quite simple. We don't
 * need to make real-time decisions about the state of the window, so
 * we just dump the data into the ringbuffer. This function is
 * essentially copied from writer.cc
 */
void 
TriggeredWriter::operator() (sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	nframes_t nf = _ringbuf.push(in, nframes, time);
	// as in writer, we throw an error for buffer overruns. It may
	// be preferable to signal that an xrun has occurred and
	// simply invalidate the current file.
	if (nf < nframes)
		throw std::runtime_error("ringbuffer filled up");
}

/*
 * The flush function is called by the main thread. It has several
 * jobs to do:
 *
 * 1. read samples from the ringbuffer
 * 2. push them to the window discriminator 
 * 3. depending on the state of the window discriminator, write data to disk
 * 4. advance the ringbuffer read pointer
 *
 * The prebuffering introduces some complications, because at the
 * moment the gate opens we need to access the samples before the
 * trigger point.  It would be nice to use the process ringbuffer, but
 * we run into issues at the boundary when the write pointer resets to
 * the beginning.  So we use a second buffer for the prebuffer data.
 *
 */
const std::string&
TriggeredWriter::flush()
{
	// read samples from buffer. We allocate a pointer and then
	// get the ringbuffer to point us at the memory where the data
	// are located.  
	sample_t *buf = 0;
	nframes_t frames = _ringbuf.peek(&buf);

	// pass samples to window discriminator; its state may change,
	// in which case we will need to inspect the return value
	int offset = _wd.push(buf, frames);
	//std::cout << _prebuf << std::endl;
	//std::cout << _wd << std::endl;
	if (_wd.open()) {
		// gate is open; data before offset goes into
		// prebuffer. Some unnecessary copying in the interest
		// of simplicity.
		if (offset > -1) {
			_prebuf.push(buf, offset);
			// get timestamp and adjust for prebuffer
			next_entry(_ringbuf.get_time()+offset, _prebuf.read_space());
			// Here we use the pop_fun function of the
			// prebuffer to write the entire prebuffer to
			// disk.  There is some jiggery-pokery in
			// passing the write function to pop_fun()
			// because it's a member function.  The
			// alternative would be to either copy the
			// prebuffer, or make two passes to get the
			// data out to file.
 			_prebuf.pop_fun(boost::bind(static_cast<writefun_t>(&util::multisndfile::write),
 						    boost::ref(_writer), _1, _2));
			_writer.write(buf+offset, frames-offset);
		}
		else
			_writer.write(buf, frames);
	}
	else {	
		// gate is closed; Data before offset goes to file;
		// rest goes to prebuffer
		if (offset > -1) {
			_writer.write(buf, offset);
			close_entry(_ringbuf.get_time()+offset);
			_prebuf.push(buf+offset, frames-offset);
		}
		else
			_prebuf.push(buf, frames);
	}
	_ringbuf.advance(frames);
	return _writer.current_file();
}

void
TriggeredWriter::next_entry(nframes_t time, nframes_t prebuf)
{
	_logv << _logv.allfields << "Signal start @" << time << "; begin entry @" << time - prebuf;
	std::string s = _writer.next();
	if (s != "")
		_logv << "; writing to file " << s;
	_logv << std::endl;
}


void
TriggeredWriter::close_entry(nframes_t time)
{
	_logv << _logv.allfields << "Signal stop  @" << time << std::endl;
	_writer.close();
}
