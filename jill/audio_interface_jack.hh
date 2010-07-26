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
#ifndef _AUDIO_INTERFACE_JACK_HH
#define _AUDIO_INTERFACE_JACK_HH

#include "audio_interface.hh"

#include <string>
#include <vector>
#include <jack/transport.h>

namespace jill {

/**
 * An audio interface for the JACK audio toolkit. A JACK module can
 * connect to one or more input or output ports, and is expected to
 * provide a callback function that will process small chunks of audio
 * data in real time.
 *
 */
class AudioInterfaceJack : public AudioInterfaceTransport
{
public:

	/**
	 * Initialize a JACK client
	 * @param name The name of the client
	 * @param port_flags Indicate what types of ports the client has.
	 *                   JackPortIsInput - register an input port
	 *                   JackPortIsOutput - register an output port
	 *                   JackPortIsInput|JackPortIsOutput - register both
	 */
	AudioInterfaceJack(const std::string & name, int port_type);
	virtual ~AudioInterfaceJack();

	virtual void set_timebase_callback(TimebaseCallback cb);

	/// Get JACK client name
	std::string client_name() const;
	/// Get id of JACK audio processing thread
	pthread_t client_thread() const;

	virtual nframes_t samplerate() const;
	virtual bool is_shutdown() const;
	virtual const std::string &get_error() const { return _err_msg; }

	// JACK connections
	/**
	 * Connect the client's input to an output port
	 *
	 * @param port The name of the port to connect to
	 */
	void connect_input(const std::string & port);
	/**
	 * Connect the client's output to an input port
	 *
	 * @param port The name of the port to connect to
	 */
	void connect_output(const std::string & port);
	/// Disconnect the client from all its ports
	void disconnect_all();
	/**
	 * Return a vector of all available ports
	 * @param port_type Indicate the type of the port
	 */
	std::vector<std::string> available_ports(JackPortFlags port_type=JackPortIsOutput);
	/**
	 * Return a vector of all connected ports
	 * @param port_type Indicate the type of the port
	 */
	//std::vector<std::string> connected_ports(JackPortFlags port_type=JackPortIsOutput);

	// JACK transport
	virtual bool transport_rolling() const;
	virtual position_t position() const;
	virtual nframes_t frame() const;
	virtual bool set_position(const position_t &);
	virtual bool set_frame(nframes_t);

private:

	static int process_callback_(nframes_t, void *);
	static void timebase_callback_(jack_transport_state_t, nframes_t, position_t *, int, void *);
	static void shutdown_callback_(void *);

	jack_client_t *_client;
	jack_port_t *_output_port;
	jack_port_t *_input_port;

	volatile bool _shutdown;
	std::string _err_msg;
};

} // namespace jill

#endif // _AUDIO_INTERFACE_JACK_HH
