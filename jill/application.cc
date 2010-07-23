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

#include "application.hh"

using namespace jill;

Application::Application(AudioInterfaceJack *client, const Options *options, util::logstream *logv)
	: _client(client), _options(options), _logv(logv), _quit(false) {}

void
Application::setup()
{
	
}
