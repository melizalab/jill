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
 */
#ifndef _APPLICATION_HH
#define _APPLICATION_HH

#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>

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

	Application() {}
	virtual ~Application() {}

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


} // namespace jill


#endif // _APPLICATION_HH
