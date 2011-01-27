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

#include "sndfile_player_client.hh"
#include "util/logger.hh"
#include <iostream>

using namespace jill;

SndfilePlayerClient::SndfilePlayerClient(const char * client_name, util::logstream * logger)
	: SimpleClient(client_name, 0, "out"), _sounds(samplerate())
{
	set_process_callback(boost::ref(_sounds));
	_sounds.set_logger(logger);
}

SndfilePlayerClient::~SndfilePlayerClient() {}

void
SndfilePlayerClient::_stop(const char *reason)
{
	_sounds.reset(false);
	_status_msg = reason;
}
