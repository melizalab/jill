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
#include "jill_client.hh"
#include <jack/jack.h>
#include <jack/statistics.h>

using namespace jill;


JillClient::JillClient(const char * name)
	: _mainloop_delay(10000), _quit(false)
{
	if ((_client = jack_client_open(name, JackNullOption, NULL)) == 0)
		throw AudioError("can't connect to jack server");

	jack_set_xrun_callback(_client, &xrun_callback_, static_cast<void*>(this));
	jack_on_shutdown(_client, &shutdown_callback_, static_cast<void*>(this));
}

JillClient::~JillClient()
{
	jack_client_close(_client);
}


void 
JillClient::set_timebase_callback(const TimebaseCallback &cb)
{
	if (cb) {
		if (jack_set_timebase_callback(_client, 0,
					       &timebase_callback_, static_cast<void*>(this)) != 0) {
			throw AudioError("failed to become jack transport master");
		}
	}
	else {
		if (_timebase_cb) {
			jack_release_timebase(_client);
		}
	}
	_timebase_cb = cb;
}

// pthread_t 
// JillClient::client_thread() const
// {
// 	return jack_client_thread_id(_client);
// }

nframes_t 
JillClient::samplerate() const
{
	return jack_get_sample_rate(_client);
}

std::string 
JillClient::client_name() const
{
	return std::string(jack_get_client_name(_client));
}


bool 
JillClient::transport_rolling() const
{
	return (jack_transport_query(_client, NULL) == JackTransportRolling);
}


position_t 
JillClient::position() const
{
	position_t pos;
	jack_transport_query(_client, &pos);
	return pos;
}


nframes_t 
JillClient::frame() const
{
	return jack_get_current_transport_frame(_client);
}


bool 
JillClient::set_position(position_t const & pos)
{
	// jack doesn't modify pos, should have been const anyway, i guess...
	return (jack_transport_reposition(_client, const_cast<position_t*>(&pos)) == 0);
}


bool 
JillClient::set_frame(nframes_t frame)
{
	return (jack_transport_locate(_client, frame) == 0);
}

void 
JillClient::set_freewheel(bool on)
{
	int ret = jack_set_freewheel(_client, on);
	if (ret != 0)
		throw AudioError("Unable to set freewheel mode");
}

void 
JillClient::timebase_callback_(jack_transport_state_t /*state*/, nframes_t /*nframes*/,
			       position_t *pos, int /*new_pos*/, void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);

	if (this_->_timebase_cb) {
		this_->_timebase_cb(pos);
	}
}


void 
JillClient::shutdown_callback_(void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);
	// this needs to be done async-safe, so I just set a flag and let _run handle it
	this_->_status_msg = "shut down by server";
	this_->_quit = true;
}


int 
JillClient::xrun_callback_(void *arg)
{
	JillClient *this_ = static_cast<JillClient*>(arg);
	if (this_->_xrun_cb)
		return this_->_xrun_cb(jack_get_xrun_delayed_usecs(this_->_client));
	return 0;
}


int
JillClient::_run()
{
	_status_msg = "running";
	_quit = false;
	for (;;) {
		::usleep(_mainloop_delay);
		if (_quit)
			return 0;
		else if (_mainloop_cb) {
			int ret = _mainloop_cb();
			if(ret!=0) {
				_status_msg = "terminated by main loop callback";
				return ret;
			}
		}
	}
}


void
JillClient::_stop(const char *reason)
{
	_status_msg = (reason==0) ? "main loop terminated" : reason;
	_quit = true;
}



