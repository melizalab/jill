/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef _OPEN_EPHYS_RECEIVER_HH
#define _OPEN_EPHYS_RECEIVER_HH

#include "../data_writer.hh"

namespace jill { namespace net {

/** A data_writer that sends messages to an open-ephys NetworkEvents plugin */
class open_ephys_receiver : public data_writer {
public:
	open_ephys_receiver(std::string const & endpoint);
	~open_ephys_receiver() override;

	/** Owns the socket events are published on. A copy would close it twice. */
	open_ephys_receiver(open_ephys_receiver const &) = delete;
	open_ephys_receiver & operator=(open_ephys_receiver const &) = delete;

        /** override the write method to send messages over zmq */
        void write(data_block_t const *, nframes_t, nframes_t) override;

	/** send a message to the remote endpoint (ignores reply) */
	void send(std::string const & message);
private:
	std::string _endpoint;
	void * _socket;       // zmq socket

};


}} // jill::net


#endif
