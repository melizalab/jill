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
#ifndef _JILL_CLIENT_HH
#define _JILL_CLIENT_HH

#include "types.hh"
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <string>

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
 * These common tasks, with the exception of port creation and data
 * handling, are handled by Client.
 */

namespace jill {

/**
 * @ingroup clientgroup
 * @brief Handle JACK client setup and shutdown
 *
 * This class handles the most basic aspects of JACK client setup and
 * shutdown.  On construction, it registers a new client with the JACK
 * server. It provides functions to register xrun, shutdown, timebase,
 * and sync callbacks. It also provides a variety of functions for
 * inspecting the client and server, and for issuing transport
 * reposition requests.
 *
 * Client::run, by default, will loop endlessly while the client
 * processes data. The user can modify this behavior by registering a
 * MainloopCallback, which will be called on each loop of
 * Client::run. It is also possible to customize this behavior by
 * overriding various virtual functions.
 *
 * Client provides an interface for connecting input and output ports,
 * but deriving classes are required to implement _connect_input and
 * _connect_output, and to register the client's ports in order to
 * have to something to connect to.
 *
 * Client does not provide any facility for handling the process
 * callback, since this will depend on the number of input and output
 * ports.  Deriving classes are expected to set up this callback if
 * they actually want to process any data.
 */
class Client : boost::noncopyable {

public:
	/**
	 * Objects of this class are emitted by Client and
	 * derived classes for a variety of errors
	 */
	struct AudioError : public std::runtime_error {
		AudioError(std::string const & w) : std::runtime_error(w) { }
	};

	/**
	 * The timebase callback is called when the transport system
	 * is activated and a new position is specified.
	 *
	 * @param pos  a structure with information about the
	 *             position of the transport
	 */
	typedef boost::function<void (position_t *pos)> TimebaseCallback;

	/**
	 * The xrun callback is called whenever there is an overrun or underrun.
	 *
	 * @param delay_usecs   the size of the xrun, in usecs
	 */
	typedef boost::function<int (float delay_usecs)> XrunCallback;

	/**
	 * The main thread callback is called by the run() loop, and
	 * should be used for any low-priority tasks. It takes no
	 * arguments. If it returns a nonzero value the run() loop
	 * will terminate.
	 */
	typedef boost::function<int(void)> MainLoopCallback;

	/**
	 * Initialize a new Jill client. All clients are identified to
	 * the JACK server by an alphanumeric name, which is specified
	 * here. The constructor creates the client and connects it to
	 * the server.
	 *
	 * @param name   the name of the client as represented to the server
	 */
	Client(const char * name);
	virtual ~Client();

	/**
	 * Register a timebase callback. This causes the client to
	 * become the 'JACK transport master'; an error is thrown if
	 * this fails.  The callback is called after the process
	 * callback returns, or whenever a client requests a new
	 * position.  For example, a client doing playback of a file
	 * can be repositioned to any point in the file.  Most clients
	 * will not need to use this.
	 *
	 * @param cb    a callback of type @a TimebaseCallback
	 */
	void set_timebase_callback(const TimebaseCallback &cb);

	/**
	 * Register an xrun callback. This function is called whenever
	 * an overrun or underrun occurs.  Registering a callback is
	 * only really necessary if the user wants to handle these
	 * events (i.e. by invalidating an output file)
	 *
	 * @param cb   a callback of type @a XrunCallback
	 */
	void set_xrun_callback(const XrunCallback &cb) {
		_xrun_cb = cb;
	}

	/**
	 * Register a main loop callback. This function is called by
	 * the @a run() member function, which has the default
	 * behavior of looping endlessly until the client terminates,
	 * the callback returns a nonzero value, or the @a stop()
	 * function is called by some other thread.
	 *
	 * @param cb    a callback of type MainLoopCallback
	 */
	void set_mainloop_callback(const MainLoopCallback &cb) {
		_mainloop_cb = cb;
	}

	/**
	 * Connect one the client's ports to another
	 * port. Implemention depends on a virtual function, but by
	 * default the long name (e.g. client:in) is looked up first, and
	 * if this fails then it attempts to find it on the local client.
	 *
	 * @param src   The name of the source port.
	 * @param dest  The name of the destination port.
	 *
	 */
	void connect_port(const char * src, const char * dest) {
		_connect_port(src, dest);
	}

	/**
	 * Disconnect the client from all its ports.
	 */
	void disconnect_all() {
		_disconnect_all();
	}

	/**
	 * Set the delay, in microseconds, between loops in @a
	 * run().
	 */
	void set_mainloop_delay(unsigned int usec_delay) { _mainloop_delay = usec_delay; }

	/**
	 * Perform various tasks in the main thread while the process
	 * thread is running.  The default behavior is to loop until
	 * the process callback throws an error, the server terminates
	 * (or the client is disconnected), or some other change to
	 * the internal state of the object.  If the user has
	 * specified a mainloop callback, this is executed on every
	 * loop; if the callback returns a nonzero value this will
	 * also cause run() to exit.
	 *
	 * @return   0 if the client terminates internally; the return value of the
	 *             mainloop callback otherwise
	 */
	int run() { return _run(); }

	/**
	 * Run the client in "nonblocking mode". This is only
	 * meaningful if the client has some well-defined starting
	 * state (set up by the virtual _reset() function).
	 * Otherwise, note that the client is running as soon as the
	 * process callback is registered.
	 */
	void oneshot() { _reset(); }

	/**
	 * Terminate the client at the next opportunity.
	 */
	void stop(const char * reason=0) { _stop(reason); }

	/** Return a reason for the run() loop's termination */
	const std::string &get_status() const { return _status_msg; }

	/* -- Inspect state of the client or server -- */

	/** Return the sample rate of the client */
	nframes_t samplerate() const;

	/**  Return true if the run() loop is in progress */
	bool is_running() const { return _is_running(); }

	/**  Get JACK client name */
	std::string client_name() const;

	/* -- Transport related functions -- */

	/// Return true if the client is receiving data
	bool transport_rolling() const;

	/**
	 *  Get the current position of the transport. The value
	 *  returned is a special JACK structure that may contain
	 *  extended information about the position (e.g. beat, bar)
	 */
	position_t position() const;

	/** Return the current frame */
	nframes_t frame() const;

	/** Request a new position from the transport master */
	bool set_position(position_t const &);

	/** Request a new frame position from the transport master */
	bool set_frame(nframes_t);

	/**
	 * Set the freewheel state. If freewheel is on, the client
	 * graph is processed as fast as possible, which means that
	 * the server may run much faster than real-time.  Potentially
	 * useful for test clients, but not tested yet.
	 *
	 * @param on   the freewheel state to set
	 */
	void set_freewheel(bool on);


protected:
	/** store a description of why the run() function is exiting */
	std::string _status_msg;

	/** The amount of time to sleep between loops in run() */
	unsigned int _mainloop_delay;

	/** Pointer to the JACK client */
	jack_client_t *_client;

private:

	TimebaseCallback _timebase_cb;
	XrunCallback _xrun_cb;
	MainLoopCallback _mainloop_cb;

	/** if true, the run() function will exit at the end of the next loop */
	volatile bool _quit;

	/*
	 * These are the callback functions that are actually
	 * registered with the JACK server. The registration functions
	 * take function pointers, not member function pointers, so
	 * these functions have to be static.  A pointer to the object
	 * is passed as a void pointer, and used to access the
	 * callback functions registered by the user.
	 */
	static void timebase_callback_(jack_transport_state_t, nframes_t, position_t *, int, void *);
	static void shutdown_callback_(void *);
	static int xrun_callback_(void *);

	/* virtual functions! */

	/**
	 * Connect ports. Default implementation is a simple
	 * name-based lookup, but can be customized based on the
	 * number of ports, etc.  Fails silently if the ports are
	 * already connected.
	 *
	 * @param src     the name of the source port
	 * @param dest    the name of the destination port
	 */
	virtual void _connect_port(const char * src, const char * dest);

	/** Disconnect the client from all its connections */
	virtual void _disconnect_all() = 0;

	/**
	 * Run the main loop. Default behavior: can be blocking or
	 * nonblocking depending on _mainloop_delay. In nonblocking
	 * case, calls _reset() and returns 0; In blocking case, calls
	 * _reset(), then loops until one of the following conditions
	 * are met:
	 *
	 * 1. _is_running() returns false
	 * 2. _mainloop_cb, if it exists, returns nonzero
	 *
	 * @return 0 if _is_running() is false, the return value of
	 * mainloop_cb otherwise.
	 */
	virtual int _run();

	/**
	 * Reset the state of the client. The result, if the client is
	 * valid and ready to go, should be that _is_running() returns
	 * true
	 */
	virtual void _reset();

	/**
	 * Cause @a run() to return if it's in blocking mode. At a
	 * minimum, this function should ensure that _is_running()
	 * returns false. It may also be desirable to change the
	 * behavior of the process loop.
	 */
	virtual void _stop(const char *reason);

	/**
	 * Return true if the client is still valid and the run()
	 * function should continue to loop.
	 */
	virtual bool _is_running() const { return !_quit; }
};


} //namespace jill

#endif
