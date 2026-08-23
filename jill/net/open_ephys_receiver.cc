/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "../types.hh"
#include "../logging.hh"
#include "../util/string.hh"
#include "../midi.hh"
#include "zmq.hh"

#include "open_ephys_receiver.hh"

using namespace jill::net;
using jill::util::make_string;

open_ephys_receiver::open_ephys_receiver(std::string const & endpoint) :
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

open_ephys_receiver::~open_ephys_receiver()
{
        DBG << "disconnecting from open-ephys NetworkPlugin at " << _endpoint;
	zmq::disconnect(_socket, _endpoint);
	INFO << "disconnected from open-ephys at " << _endpoint;
	// We don't need to wait for queued messages with a REQ socket
	zmq::close(_socket);
}

void
open_ephys_receiver::write(data_block_t const * block, nframes_t, nframes_t)
{
	// decode events
        if (block->sz_data == 0) return;
	if (block->dtype == SAMPLED) return;
	auto * buffer = block->data<char>();
	auto status = midi::status_type(buffer[0]);
	char const * verb;
	switch (status.status()) {
	case midi::status_type::stim_on:
		verb = "start";
		break;
	case midi::status_type::stim_off:
		verb = "stop";
		break;
	default:
		return;
	}
	/**
	 * The channel is part of the verb rather than a field of its own,
	 * because the stimulus name is last and may contain spaces -- so a
	 * reader splits once and takes the remainder, and any token added
	 * after the name would be ambiguous. Channel 0 keeps the bare verb, so
	 * a reader that predates the other channels is still correct about the
	 * stimulus stream rather than quietly wrong about it.
	 *
	 * Without this the channel was masked off and every stream produced
	 * the same text: with a trial window and a condition marker in use,
	 * open-ephys recorded three identical "start" events per trial and no
	 * way to tell which was which.
	 */
	std::ostringstream message;
	if (auto channel = status.channel()) {
		switch (*channel) {
		case midi::channel::stim:                          break;
		case midi::channel::trial:     message << "trial_";     break;
		case midi::channel::condition: message << "condition_"; break;
		default: message << "channel" << int(*channel) << "_";  break;
		}
	}
	message << verb << ' ' << (buffer + 1);
	DBG << "event: t=" << block->time << " message=" << message.str();
	send(message.str());
}

void
open_ephys_receiver::send(std::string const & message)
{
	int rc = zmq::send(_socket, message);
	if (rc < 0) {
		throw NetworkError(make_string() << "error sending zmq message to networkevents plugin");
	}
	// REQ messages always have a reply, which we discard
	zmq::recv(_socket, [](zmq::msg_ptr_t const &) {});
}
