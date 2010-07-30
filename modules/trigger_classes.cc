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
#include "trigger_classes.hh"

/*
 * This is the file where we define the implementations of the classes
 * declared in in trigger_classes.hh (the interface).  The advantage
 * of separating the implementation from the interface is that the end
 * user doesn't need to see the implementation details.
 *
 * For example, we only ask the user to pass us a reference to a
 * soundfile writer. All the buffering is taken care of internally,
 * and we can change the details of how it's done without affecting
 * the interface.
 *
 *
 */

/*
 * This is the constructor for the TriggeredWriter class. We have to
 * assign the references in the initialization list (it's not
 * otherwise possible to reseat them).  We also initialize the
 * ringbuffer and store the size of the prebuffer.
 */
TriggeredWriter::TrigerredWriter(filters::WindowDiscriminator<sample_t> &wd, 
				 util::multisndfile &writer,
				 nframes_t prebuffer_size, nframes_t buffer_size)
	: _wd(wd), _writer(writer), _ringbuf(buffer_size), _prebuf(prebuffer_size) {}

/*
 * The function called by the process thread is quite simple. We don't
 * need to make real-time decisions about the state of the window, so
 * we just dump the data into the ringbuffer. This function is
 * essentially copied from writer.cc
 */
void 
TriggeredWriter::operator() (sample_t *in, sample_t *out, nframes_t nframes, nframes_t time)
{
	nframes_t nf = _rb.push(in, nframes);
	// as in writer, we throw an error for buffer overruns. It may
	// be preferable to signal that an xrun has occurred and
	// simply invalidate the current file.
	if (nf < nframes)
		throw std::runtime_error("ringbuffer filled up");
	// should do something with timestamps
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
 * the beginning.  We don't know how many samples we'll get, which
 * makes using a normal buffer complicated.  I've elected to use
 * another ringbuffer here.  We don't need the reentrancy but the
 * semantics are fairly simple to use.
 *
 */
const std::string&
TriggeredWriter::flush()
{
	// read samples from buffer. We allocate a pointer and then
	// get the ringbuffer to point us at the memory where the data
	// are located.  
	sample_t *buf;
	nframes_t frames = _ringbuf.peek(&buf);

	// pass samples to window discriminator; its state may change,
	// in which case we will need to inspect the return value
	int offset = _wd.push(buf, frames);
	if (_wd.open()) {
		// calculate offset into prebuffer
		// write prebuf to disk 
		// write buf to disk
	}
	else {	
		// write to prebuffer - the most I will write is _prebuf_size
		int space = _prebuf_size - _prebuf.read_space(); // 
		int o = frames - _prebuf.size();
		if (o > 0) {
			// too many frames; only push most recent
			_prebuf.advance();
			_prebuf.push(buf+o, _prebuf.size());
		}
		else {
			

		_prebuf.push(buf, frames);
	}
	// advance ringbuffer
		
}


TriggerOptions::TriggerOptions(const char *program_name, const char *program_version)
	: Options(program_name, program_version) // this calls the superclass constructor
{
	cmd_opts.add_options()
		("output_file", po::value<std::string>(), "set output file");
	pos_opts.add("output_file", -1);
} 

void 
TriggerOptions::process_options() 
{
	if (vmap.count("output_file"))
		output_file = get<std::string>("output_file");
	else {
		std::cerr << "Error: missing required output file name " << std::endl;
		throw Exit(EXIT_FAILURE);
	}
}

void 
TriggerOptions::print_usage() 
{
	std::cout << "Usage: " << _program_name << " [options] output_file\n\n"
		  << "output_file can be any file format supported by libsndfile\n"
		  << visible_opts;
}



