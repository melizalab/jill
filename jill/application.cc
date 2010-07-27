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

#include "application.hh"
#include <jack/jack.h>

using namespace jill;

Application::Application(AudioInterfaceJack &client, util::logstream &logv)
	: _logv(logv), _client(client), _quit(false) {}


void
Application::connect_inputs(const std::vector<std::string> &ports)
{
	std::vector<std::string>::const_iterator it;
	for (it = ports.begin(); it != ports.end(); ++it) {
		_client.connect_input(*it);
		_logv << _logv.allfields << "Connected input to port " << *it << std::endl;
	}
}

void
Application::connect_outputs(const std::vector<std::string> &ports)
{
	std::vector<std::string>::const_iterator it;
	for (it = ports.begin(); it != ports.end(); ++it) {
		_client.connect_output(*it);
		_logv << _logv.allfields << "Connected output to port " << *it << std::endl;
	}
}

void
Application::run(unsigned int usec_delay)
{
	_logv << _logv.allfields << "Starting main loop with delay " << usec_delay << " usec" << std::endl;
	for (;;) {
		::usleep(usec_delay);
		if (_quit) {
			_logv << _logv.allfields << "Application terminated" << std::endl;
			return;
		}
		else if (_client.is_shutdown())
			throw std::runtime_error(_client.get_error());

		if (_mainloop_cb) {
			if(_mainloop_cb()!=0) {
				_logv << _logv.allfields << "Main loop terminated" << std::endl;
				return;
			}
		}
	}
}
