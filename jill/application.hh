/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * A generic JILL application. Handles argument parsing and any main
 * loop activity. Subclass if you need to do anything in the main
 * loop.
 *
 */
class Application : boost::noncopyable {
public:
	
	/** 
	 * Initialize the application with a client and options
	 * @param client The JACK client. Needs to be initialized and any callbacks set
	 * @param options An options class, which controls initialization.
	 */
	Application(AudioInterfaceJack *client, const Options *options, util::logstream *logv);
	~Application() {}

	void setup();
	void run();
	void signal_quit() { _quit = true; }

private:
	AudioInterfaceJack *_client;
	const Options *_options;
	util::logstream *_logv;
	volatile bool _quit;
};

} // namespace jill


#endif // _APPLICATION_HH
