/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is a template of a JILL client, which just copies data
 * from its input to its output.  Adapt it to write your own module.
 */

#include "jill/audio.hh"
#include <boost/scoped_ptr.hpp>
#include <iostream>

using namespace jill;

/// The name of the client;
static const std::string client_name("template");
static const std::string input_port("system:capture_1");
static const std::string output_port("system:playback_1");

/** 
 * This function is the processing loop, which runs in a real-time
 * JACK thread.  Depending on the type of interface, it can receive
 * input data, or be required to generate output data.
 * 
 * @param in Pointer to the input buffer. NULL if the client has no input port
 * @param out Pointer to the output buffer. NULL if no output port
 * @param nframes The number of frames in the data
 */
void 
process(sample_t *in, sample_t *out, nframes_t nframes)
{
	memcpy(out, in, sizeof(sample_t) * nframes);
}

int
main(int argc, char **argv)
{
	using namespace std;
	// initialize the interface
	boost::scoped_ptr<AudioInterfaceJack> client;
	client.reset(new AudioInterfaceJack(client_name, JackPortIsInput|JackPortIsOutput));
	// set the callback
	client->set_process_callback(process);
	// connect the ports
	client->connect_input(input_port);
	client->connect_output(output_port);

	// let the client run for a while
	::sleep(10);

	// cleanup is automatic!
}
