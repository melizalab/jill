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
#include "jack_client.hh"
#include "util/string.hh"
#include <jack/statistics.h>
#include <jack/midiport.h>
#include <cerrno>
#include <algorithm>

using namespace jill;
using std::string;

jack_client::jack_client(string const & name, boost::shared_ptr<event_logger> logger)
        : _nports(0), _log(logger)
{
        start_client(name.c_str(), 0);
        set_callbacks();
}

jack_client::jack_client(string const & name, boost::shared_ptr<event_logger> logger, string const & server)
        : _nports(0), _log(logger)
{
        if (!server.empty())
                start_client(name.c_str(), server.c_str());
        else
                start_client(name.c_str(), 0);
        set_callbacks();

}

jack_client::~jack_client()
{
	if (_client) {
                jack_client_close(_client);
        }
}

void
jack_client::start_client(char const * name, char const * server_name)
{
	jack_status_t status;
        if (server_name == 0)
                _client = jack_client_open(name, JackNoStartServer, &status);
        else
                _client = jack_client_open(name, jack_options_t(JackNoStartServer|JackServerName),
                                           &status, server_name);

        if (_client == NULL) {
                util::make_string err;
                err << "unable to start client (status=" << int(status);
                if (status & JackServerFailed) err << "; couldn't connect to server";
                err << ")";
                throw JackError(err);
        }
        _log->log() << "created client: " << jack_get_client_name(_client)
              << " (load=" << jack_cpu_load(_client) << "%)" ;
}

void
jack_client::set_callbacks()
{
        jack_set_process_callback(_client, &process_callback_, static_cast<void*>(this));
        jack_set_port_registration_callback(_client, &portreg_callback_, static_cast<void*>(this));
        jack_set_port_connect_callback(_client, &portconn_callback_, static_cast<void*>(this));
        jack_set_sample_rate_callback(_client, &sampling_rate_callback_, static_cast<void*>(this));
        jack_set_buffer_size_callback(_client, &buffer_size_callback_, static_cast<void*>(this));
        jack_set_xrun_callback(_client, &xrun_callback_, static_cast<void*>(this));
        jack_on_info_shutdown(_client, &shutdown_callback_, static_cast<void*>(this));
}

jack_port_t*
jack_client::register_port(string const & name, string const & type,
                           unsigned long flags, unsigned long buffer_size)
{
        jack_port_t *port = jack_port_register(_client, name.c_str(), type.c_str(), flags, buffer_size);
        if (port == NULL) {
                throw JackError(util::make_string() << "unable to allocate port " << name);
        }
        _ports.push_back(port);
        _nports += 1;
        _log->log() << "port registered: " << jack_port_name(port)
              << " (" << jack_port_type(port) << ")" ;
        return port;
}

void
jack_client::unregister_port(string const & name)
{
        unregister_port(jack_port_by_name(_client, name.c_str()));
}

void
jack_client::unregister_port(jack_port_t *port)
{
        int ret = jack_port_unregister(_client, port);
        if (ret)
                throw JackError(util::make_string() << "unable to unregister port (err=" << ret << ")");
        _ports.remove(port);
        _nports += -1;
        _log->log() << "port unregistered: " << jack_port_name(port) ;
}

void
jack_client::activate()
{
        int ret = jack_activate(_client);
        if (ret)
                throw JackError(util::make_string() << "unable to activate client (err=" << ret << ")");
        _log->log() << "activated client (load=" << jack_cpu_load(_client) << "%)" ;
}

void
jack_client::deactivate()
{
        int ret = jack_deactivate(_client);
        if (ret)
                throw JackError(util::make_string() << "unable to deactivate client (err=" << ret << ")");
        _log->log() << "deactivated client" ;
}

void
jack_client::connect_port(string const & src, string const & dest)
{
	// simple name-based lookup
	jack_port_t *p1, *p2;
	p1 = jack_port_by_name(_client, src.c_str());
	if (p1==0) {
		string n = util::make_string() << jack_get_client_name(_client) << ":" << src;
		p1 = jack_port_by_name(_client, n.c_str());
		if (p1==0)
			throw JackError(util::make_string() << "the port " << n << " does not exist");
	}

	p2 = jack_port_by_name(_client, dest.c_str());
	if (p2==0) {
		string n = util::make_string() << jack_get_client_name(_client) << ":" << dest;
		p2 = jack_port_by_name(_client, n.c_str());
		if (p2==0)
			throw JackError(util::make_string() << "the port " << n << " does not exist");
	}

        // check that types are the same (FIXME use strcmp if custom types?)
        if (jack_port_type(p1)!=jack_port_type(p2)) {
                throw JackError(util::make_string() << jack_port_name(p1) << " (" << jack_port_type(p1) << ")"
                                << " doesn't match " << jack_port_name(p2) << " (" << jack_port_type(p2) << ")");
        }
	int error = jack_connect(_client, jack_port_name(p1), jack_port_name(p2));
	if (error && error != EEXIST) {
		// no easy way to trap error message; it gets printed to stdout
		throw JackError(util::make_string() << "can't connect "
				 << src << " to " << dest);
        }
}

void
jack_client::disconnect_all()
{
        port_list_type::const_iterator it;
        for (it = _ports.begin(); it != _ports.end(); ++it) {
                int ret = jack_port_disconnect(_client, *it);
                if (ret)
                        throw JackError(util::make_string() << "unable to disconnect port (err=" << ret << ")");
        }
}

sample_t*
jack_client::samples(string const & name, nframes_t nframes)
{
        return samples(jack_port_by_name(_client, name.c_str()), nframes);
}


sample_t*
jack_client::samples(jack_port_t *port, nframes_t nframes)
{
        if (port)
                return static_cast<sample_t*>(jack_port_get_buffer(port, nframes));
        else
                return 0;
}

void*
jack_client::events(jack_port_t *port, nframes_t nframes)
{
        if (port) {
                void *buf = jack_port_get_buffer(port, nframes);
                if (jack_port_flags(port) & JackPortIsOutput) jack_midi_clear_buffer(buf);
                return buf;
        }
        else
                return 0;
}

jack_port_t*
jack_client::get_port(string const & name) const
{
        return jack_port_by_name(_client, name.c_str());
}

nframes_t
jack_client::sampling_rate() const
{
	return jack_get_sample_rate(_client);
}

nframes_t
jack_client::buffer_size() const
{
	return jack_get_buffer_size(_client);
}

char const *
jack_client::name() const
{
	return jack_get_client_name(_client);
}

nframes_t
jack_client::frame() const
{
	return jack_frame_time(_client);
}

nframes_t
jack_client::frame(utime_t time) const
{
        return jack_time_to_frames(_client, time);
}

utime_t
jack_client::time(nframes_t frame) const
{
        return jack_frames_to_time(_client, frame);
}

utime_t
jack_client::time() const
{
        return jack_get_time();
}


int
jack_client::process_callback_(nframes_t nframes, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        nframes_t time = jack_last_frame_time(self->_client);
	return (self->_process_cb) ? self->_process_cb(self, nframes, time) : 0;
}

void
jack_client::portreg_callback_(jack_port_id_t id, int registered, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        jack_port_t *port = jack_port_by_id(self->_client, id);
        if (!jack_port_is_mine(self->_client, port)) return;
        if (self->_portreg_cb)
                self->_portreg_cb(self, port, registered);
}

void
jack_client::portconn_callback_(jack_port_id_t a, jack_port_id_t b, int connected, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        jack_port_t *port1 = jack_port_by_id(self->_client, a);
        jack_port_t *port2 = jack_port_by_id(self->_client, b);
        if (!(jack_port_is_mine(self->_client, port1) || jack_port_is_mine(self->_client, port2)))
                return;
        if (jack_port_flags(port2) & JackPortIsOutput)
                std::swap(port1,port2);
        self->_log->log() << "ports " << ((connected) ? "" : "dis") << "connected: "
                    << jack_port_name(port1) << " -> " << jack_port_name(port2) ;

        if (self->_portconn_cb) {
                self->_portconn_cb(self, port1, port2, connected);
        }
}

int
jack_client::sampling_rate_callback_(nframes_t nframes, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        self->_log->log() << "sampling rate (Hz): " << nframes ;
	return (self->_sampling_rate_cb) ? self->_sampling_rate_cb(self, nframes) : 0;
}

int
jack_client::buffer_size_callback_(nframes_t nframes, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        self->_log->log() << "period size (frames): " << nframes ;
	return (self->_buffer_size_cb) ? self->_buffer_size_cb(self, nframes) : 0;
}

int
jack_client::xrun_callback_(void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        float delay = jack_get_xrun_delayed_usecs(self->_client);
        self->_log->log() << "jack xrun (us): " << delay ;
	return (self->_xrun_cb) ? self->_xrun_cb(self, delay) : 0;
}

void
jack_client::shutdown_callback_(jack_status_t code, char const * reason, void *arg)
{
	jack_client *self = static_cast<jack_client*>(arg);
        self->_log->log() << "the server is shutting us down: " << reason ;
	if (self->_shutdown_cb) self->_shutdown_cb(code, reason);
}

void
jack_client::set_sample_rate_callback(SamplingRateCallback const & cb) {
        _sampling_rate_cb = cb;
        if (cb) {
                cb(this, sampling_rate());
        }
}

void
jack_client::set_buffer_size_callback(BufferSizeCallback const & cb) {
        _buffer_size_cb = cb;
}

void
jack_client::set_process_callback(ProcessCallback const & cb) {
        _process_cb = cb;
}

void
jack_client::set_port_registration_callback(PortRegisterCallback const & cb) {
        _portreg_cb = cb;
}

void
jack_client::set_port_connect_callback(PortConnectCallback const & cb) {
        _portconn_cb = cb;
}

void
jack_client::set_xrun_callback(XrunCallback const & cb) {
        _xrun_cb = cb;
}

void
jack_client::set_shutdown_callback(ShutdownCallback const & cb) {
        _shutdown_cb = cb;
}
