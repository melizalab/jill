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

using namespace jill;

Application::Application(AudioInterfaceJack &client, const Options &options, util::logstream &logv)
	: _logv(logv), _client(client), _options(options), _quit(false) {}


void
Application::setup()
{
	std::vector<std::string>::const_iterator it;
	for (it = _options.input_ports.begin(); it != _options.input_ports.end(); ++it) {
		_client.connect_input(*it);
		_logv << _logv.allfields << "Connected input to port " << *it << std::endl;
	}

	for (it = _options.output_ports.begin(); it != _options.output_ports.end(); ++it) {
		_client.connect_output(*it);
		_logv << _logv.allfields << "Connected output to port " << *it << std::endl;
	}

}

void
Application::run(unsigned int usec_delay)
{
	_logv << _logv.allfields << "Starting main loop with delay " << usec_delay << std::endl;
	for (;;) {
		::usleep(usec_delay);
		if (_quit) {
			_logv << _logv.allfields << "Application terminated" << std::endl;
			return;
		}
		else if (_client.is_shutdown())
			throw std::runtime_error(_client.get_error());

		if (_mainloop_cb)
			if(_mainloop_cb()!=0) {
				_logv << _logv.allfields << "Main loop terminated" << std::endl;
				return;
			}
	}
}
