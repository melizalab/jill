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
#ifndef _JACK_CLIENT_HH
#define _JACK_CLIENT_HH

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <string>
#include <list>
#include "types.hh"

/**
 * @defgroup clientgroup Creating and controlling JACK clients
 *
 * JACK clients all have to perform a series of common tasks:
 *  - connect to the JACK server as a client
 *  - create input and/or output ports
 *  - register callback functions to handle various events
 *     - process callback: handles input and output of samples
 *     - xrun callback: handles overrun and underrun events
 *     - timebase callback: handle maintaining extended time information
 *     - sync callback: handle repositioning requests
 *     - shutdown callback: handle disconnection or server shutdown
 *  - connect ports to other clients
 *
 */

namespace jill {

/**
 * @ingroup clientgroup
 * @brief Handle JACK client setup and shutdown
 *
 * This class handles the most basic aspects of JACK client setup and shutdown.
 * On construction, it registers a new client with the JACK server. It provides
 * functions to register process, xrun, shutdown, timebase, and sync callbacks.
 * It also provides a variety of functions for inspecting the client and server,
 * and for issuing transport reposition requests.
 *
 * Client provides an interface for registering and unregistering, connecting
 * and disconnecting input and output ports, and takes care of maintaining a a
 * list of the client's ports. The process callback has access to the Client
 * object and fucntions for retrieving event (MIDI) and sample data.
 *
 */
class JackClient : boost::noncopyable {

public:
	/** Type for jack errors */
	struct JackError : public std::runtime_error {
		JackError(std::string const & w) : std::runtime_error(w) { }
	};

	/**
	 * Type of the process callback. Provides a pointer to a client object
         * and information about buffer size and the current time.
	 *
	 * @param client the current client object
	 * @param size the number of samples in the buffers
	 * @param time the time elapsed (in samples) since the client started
	 */
	typedef boost::function<int (JackClient* client, nframes_t size, nframes_t time)> ProcessCallback;

	/**
	 * Initialize a new JACK client. All clients are identified to the JACK
	 * server by an alphanumeric name, which is specified here. Creates the
	 * client and connects it to the server.
	 *
	 * @param name   the name of the client as represented to the server.
	 *               May be changed by the server if not unique.
	 */
	JackClient(std::string const & name);
	virtual ~JackClient();

        /**
         * Register a new port for the client.
         */
        jack_port_t* register_port(std::string const & name, std::string const & type,
                           unsigned long flags, unsigned long buffer_size=0);

        void unregister_port(std::string const & name);
        void unregister_port(jack_port_t *port);

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
	void set_process_callback(ProcessCallback const & cb) {
		_process_cb = cb;
	}

        /** Activate the client. Do this before attempting to connect ports */
        void activate();

        /** Deactivate the client. Disconnects all ports */
        void deactivate();


	/**
	 * Connect one the client's ports to another port. The long name (e.g.
	 * client:in) is looked up first, and if this fails then it attempts to
	 * find it on the local client. Fails silently if the ports are already
	 * connected.
	 *
	 * @param src   The name of the source port (client:port or port).
	 * @param dest  The name of the destination port.
	 *
	 */
	void connect_port(std::string const & src, std::string const & dest);

	/**
	 * Disconnect the client from all its ports.
	 */
	void disconnect_all();

        /**
         * Get buffer for port
         */
        sample_t* buffer(std::string const & name, nframes_t nframes);
        sample_t* buffer(jack_port_t *port, nframes_t nframes);

	/* -- Inspect state of the client or server -- */

        /** The JACK client object */
        jack_client_t* client() { return _client;}

        /** The JACK client ports */
        std::list<jack_port_t*>& ports() { return _ports;}

	/** The sample rate of the client */
	nframes_t samplerate() const;

	/**  JACK client name */
	std::string name() const;

	/** The current frame */
	nframes_t frame() const;

	/** Request a new frame position from the transport master */
	bool frame(nframes_t);

	/// Return true if the client is receiving data
	bool transport_rolling() const;

	/**
	 *  Get the current position of the transport. The value
	 *  returned is a special JACK structure that may contain
	 *  extended information about the position (e.g. beat, bar)
	 */
	position_t position() const;

	/** Request a new position from the transport master */
	bool position(position_t const &);



protected:

	/** Pointer to the JACK client */
	jack_client_t *_client;

        /** Ports owned by this client */
        std::list<jack_port_t*> _ports;

private:

	ProcessCallback _process_cb;

	/*
	 * These are the callback functions that are actually
	 * registered with the JACK server. The registration functions
	 * take function pointers, not member function pointers, so
	 * these functions have to be static.  A pointer to the object
	 * is passed as a void pointer, and used to access the
	 * callback functions registered by the user.
	 */
	static int process_callback_(nframes_t, void *);


};


} //namespace jill

#endif
