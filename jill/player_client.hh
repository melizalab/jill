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
 */
#ifndef _PLAYER_JILL_CLIENT_HH
#define _PLAYER_JILL_CLIENT_HH

#include "client.hh"
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>


namespace jill {

/**
 * @ingroup clientgroup
 * @brief refinement of Client specialized for output-only
 *
 * This class is a further refinement of jill::Client that is
 * specialized for outputting a bunch of samples to its output
 * port. What distinguishes it from SimpleClient and other derivations
 * of Client is that all the data are generated/stored internally, and
 * there is no process callback.
 *
 * Deriving classes need to implement fill_buffer(), which is called to
 * fill the output buffer
 *
 */
class PlayerClient : public Client {

public:
	PlayerClient(const char * client_name);

protected:

	/**
	 * This function is called to fill the output buffer. Deriving
	 * classes must implement this function.
	 *
	 * @param buffer     the output buffer, zeroed out
	 * @param frames     the number of items in the buffer
	 */
	virtual void fill_buffer(sample_t *buffer, nframes_t frames) = 0;

private:

	/** This is the JACK callback; it calls fill_buffer() */
	static int process_callback_(nframes_t, void *);

	jack_port_t *_output_port;

	// allow user to connect client to an output, which can be useful if it's the only client
	void _connect_output(const char * port, const char * output=0);
	void _disconnect_all();

	// this has no effect
	void _connect_input(const char * port, const char * input=0) {}

};

} // namespace jill

#endif
