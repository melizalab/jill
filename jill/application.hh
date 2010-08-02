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
 *
 * This header is intended to be included by any application
 */
#ifndef _APPLICATION_HH
#define _APPLICATION_HH

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include "audio.hh"
#include "options.hh"
#include "util/logger.hh"

namespace jill {

/**
 * An abstract base class for applications.  An application is
 * reponsible for running a client's process function and the main
 * thread loop, if there is one.
 */
class Application : boost::noncopyable {
public:
	/// The main loop callback type.
	typedef boost::function<int(void)> MainLoopCallback;

	/** 
	 * Specify the callback to use in the main loop. The argument
	 * can be anything that matches the MainLoopCallback type - a
	 * function pointer or a function object.  The argument is
	 * copied; if this is undesirable, use a boost::ref.
	 *
	 * If the callback function returns anything other than 0 the
	 * application will terminate.
	 */
	virtual void set_mainloop_callback(const MainLoopCallback &cb) {  _mainloop_cb = cb; }
		

        /// Start the main loop running, sleeping usec_delay between loops
	virtual void run(unsigned int usec_delay) = 0;

	/// Terminate the application at the end of the next main loop
	virtual void signal_quit() = 0;

protected:
	/// The function called in the main loop of the application
	MainLoopCallback _mainloop_cb;
};	


/**
 * A generic JILL application. It handles connecting the client to any
 * input or output ports supplied in the Options argument to the
 * constructor, and calls a main loop callback while the client is
 * running. Handles some error handling and logging functions as well.
 */
class JillApplication : public Application {
public:
	/**
	 * Initialize the application with a client.
	 *
	 * @param client The JACK client. Needs to be initialized and any callbacks set
	 * @param logv   This logstream is used to provide feedback on what's happening
	 */
	JillApplication(AudioInterfaceJack &client, util::logstream &logv);
	virtual ~JillApplication();

	/**
	 * Connect the client to a set of inputs. This is just a
	 * wrapper around the client's connect_input() function, but
	 * it also logs this.
	 * @param ports: a vector of names of the input ports
	 */
	virtual void connect_inputs(const std::vector<std::string> &ports);

	/**
	 * Connect the client to a set of outputs. This is just a
	 * wrapper around the client's connect_output() function, but
	 * it also logs this.
	 * @param ports: a vector of names of the output ports
	 */
	virtual void connect_outputs(const std::vector<std::string> &ports);

        /// Start the main loop running, sleeping usec_delay between loops
	virtual void run(unsigned int usec_delay=100000);

	/// Terminate the application at the end of the next main loop
	virtual void signal_quit() { _quit = true; }

protected:
	/// A stream for producing log messages.
	util::logstream &_logv;

private:

	AudioInterfaceJack &_client;
	volatile bool _quit;
};


} // namespace jill


#endif // _APPLICATION_HH
