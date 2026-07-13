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
	char const * message = nullptr;	
	auto * buffer = reinterpret_cast<char const *>(data->data());
	std::uint8_t status = buffer[0];
	if (midi::status_type(status).is_standard_midi()) {
		// hex-encode standard midi events
		message = midi::to_hex(buffer + 1, data->sz_data - 1);
	}
	else {
		message = buffer + 1;
	}
	DBG << "event: t=" << data->time << " status=" << int(status)
	    << " message=" << message;
	
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
