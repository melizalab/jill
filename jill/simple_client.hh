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
#ifndef _SIMPLE_JILL_CLIENT_HH
#define _SIMPLE_JILL_CLIENT_HH

#include "client.hh"

namespace jill {

/**
 * @ingroup clientgroup
 * @brief a simple Client with up to one input and one output port
 *
 * This class implements the Client interface, providing up to one
 * input and one output port.  It also provides the appropriate
 * callback for this configuration, which is passed an input and and
 * output buffer of equal size.  It can be used for almost all simple
 * applications.
 */
class SimpleClient : public Client {

public:
	/**
	 * Type of the process callback.
	 *
	 * @param in the input buffer
	 * @param out the output buffer
	 * @param size the number of samples in each buffer
	 * @param time the time elapsed (in samples) since the client started
	 */
	typedef boost::function<void (sample_t *in, sample_t *out, nframes_t size, nframes_t time)> ProcessCallback;

	/**
	 * Instantiate a new client.  Connects the client to the JACK
	 * server and, optionally, registers an input and an output
	 * port.
	 *
	 * @param client_name  the name of the client (as represented to JACK)
	 * @param input_name  the name of the input port. If 0, none is registered
	 * @param output_name the name of the output port. If 0, none is registered
	 */
	SimpleClient(const char * client_name,
		     const char * input_name=0,
		     const char * output_name=0);
	virtual ~SimpleClient();

	/**
	 * Set the process callback. This can be a raw function
	 * pointer or any function object that matches the signature
	 * of @a ProcessCallback. The argument is copied; if this is
	 * undesirable use a boost::ref.
	 *
	 * @param cb The function object that will process the audio
	 * data [type @a ProcessCallback]
	 *
	 */
	void set_process_callback(const ProcessCallback &cb) {
		_process_cb = cb;
	}

protected:

	jack_port_t *_output_port;
	jack_port_t *_input_port;

private:

	ProcessCallback _process_cb;

	static int process_callback_(nframes_t, void *);

	virtual void _connect_input(const char * port, const char * input=0);
	virtual void _connect_output(const char * port, const char * output=0);
	virtual void _disconnect_all();

};

} // namespace jill
#endif
