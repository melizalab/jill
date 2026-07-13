/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include "../types.hh"
#include "../logging.hh"
#include "../util/string.hh"
#include "../midi.hh"
#include "zmq.hh"

#include "open_ephys_logger.hh"

using namespace jill::net;
using jill::util::make_string;

open_ephys_logger::open_ephys_logger(std::string const & endpoint) :
	_endpoint(endpoint), _socket(zmq::context::socket(ZMQ_REQ))
{
        DBG << "connecting to open-ephys NetworkPlugin at " << _endpoint;
	if (zmq::connect(_socket, endpoint) != 0) {
		throw NetworkError(make_string() << "error connecting to endpoint " << _endpoint);
        }
        else {
                INFO << "connected to open-ephys at " << _endpoint;
        }
}

open_ephys_logger::~open_ephys_logger()
{
        DBG << "disconnecting from open-ephys NetworkPlugin at " << _endpoint;
	zmq::disconnect(_socket, _endpoint);
	INFO << "disconnected from open-ephys at " << _endpoint;
	// We don't need to wait for queued messages with a REQ socket
	zmq::close(_socket);
}

void
open_ephys_logger::write(data_block_t const * data, nframes_t, nframes_t)
{
	// decode events
        if (data->sz_data == 0) return;
	if (data->dtype == SAMPLED) return;
	auto * buffer = reinterpret_cast<char const *>(data->data());
	auto status = midi::status_type(buffer[0]);
	std::ostringstream message;
	switch (status.status()) {
	case midi::status_type::stim_on:
		message << "start ";
		break;
	case midi::status_type::stim_off:
		message << "stop ";
		break;
	default:
		return;
	}
	message << (buffer + 1);
	DBG << "event: t=" << data->time << " message=" << message.str();
	
}

void
open_ephys_logger::send(char const * message)
{
	int rc = zmq::send(_socket, message);
	if (rc < 0) {
		throw NetworkError(make_string() << "error sending zmq message to networkevents plugin");
	}
	// REQ messages always have a reply, which we ignore
	while (true) {
		zmq::msg_ptr_t message = zmq::msg_init();
                int rc = zmq_msg_recv (message.get(), _socket, 0);
                if (rc < 0) break;
	}
}
