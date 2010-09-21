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

#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <jack/types.h>
#include <jack/transport.h>
#include <string>

namespace jill {

typedef jack_default_audio_sample_t sample_t;
typedef jack_nframes_t nframes_t;
typedef jack_position_t position_t;

class JillClient : boost::noncopyable {

public:
	struct AudioError : public std::runtime_error {
		AudioError(std::string const & w) : std::runtime_error(w) { }
	};

	/** 
	 * The timebase callback is called when the transport system
	 * is activated and a new position is specified.
	 *
	 * @param pos  the new position after the timebase changed
	 */
	typedef boost::function<void (position_t *pos)> TimebaseCallback;

	/**
	 * The xrun callback is called whenever there is an overrun or underrun.
	 *
	 * @param delay_usecs   the size of the xrun, in usecs
	 */
	typedef boost::function<int (float delay_usecs)> XrunCallback;

	/**
	 * The main thread callback is called sporadically, and should
	 * be used for any low-priority tasks
	 */
	typedef boost::function<int(void)> MainLoopCallback;

	JillClient(const std::string &name);
	virtual ~JillClient();

	void set_timebase_callback(const TimebaseCallback &cb);

	void set_xrun_callback(const XrunCallback &cb) {
		_xrun_cb = cb;
	}

	void set_mainloop_callback(const MainLoopCallback &cb) {
		_mainloop_cb = cb;
	}

	/**
	 * Connect the client's input to an output port
	 *
	 * @param port The name of the port to connect to
	 * @param input The name of the client's input port, or 0 for default
	 */
	void connect_input(const std::string & port, const std::string * input = 0) {
		_connect_input(port, input);
	}


	/**
	 * Connect the client's output to an input port
	 *
	 * @param port The name of the port to connect to
	 * @param output The name of the client's output port, or 0 for default
	 */
	void connect_output(const std::string & port, const std::string * output = 0) {
		_connect_output(port, output);
	}

	/// Disconnect the client from all its ports
	void disconnect_all() {
		_disconnect_all();
	}

        /// Start the main loop running, waiting some fixed delay between loops
	int run(unsigned int delay) { 
		return _run(delay);
	}

	/// Terminate the application at the end of the next main loop
	void stop() {
		_quit = true;
	}

	/// Return the sample rate of the client
	nframes_t samplerate() const;

	/// Return false if the client is shut down
	bool is_running() const {
		return !_quit;
	}

	/// Get JACK client name
	std::string client_name() const;

	/// Get id of JACK audio processing thread
	pthread_t client_thread() const;

	// transport-related functions
	bool transport_rolling() const;
	position_t position() const;
	nframes_t frame() const;
	bool set_position(position_t const &);
	bool set_frame(nframes_t);

	/// Set the freewheel state - can cause JACK to run faster than RT
	void set_freewheel(bool on);

	/// Return the last error message
	const std::string &get_error() const { 
		return _err_msg; 
	}

protected:

	/// if true, the run() function will exit at the end of the next loop
	volatile bool _quit;
	std::string _err_msg;

	/// Pointer to the JACK client
	jack_client_t *_client;

private:

	TimebaseCallback _timebase_cb;
	XrunCallback _xrun_cb;
	MainLoopCallback _mainloop_cb;

	/// The actual callbacks
	static void timebase_callback_(jack_transport_state_t, nframes_t, position_t *, int, void *);
	static void shutdown_callback_(void *);
	static int xrun_callback_(void *);

	/// These functions have to be overridden by derived classes
	virtual void _connect_input(const std::string & port, const std::string * input=0) = 0;
	virtual void _connect_output(const std::string & port, const std::string * output=0) = 0;
	virtual void _disconnect_all() = 0;
	virtual int _run(unsigned int delay);

};


} //namespace jill

#endif
