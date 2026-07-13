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
#ifndef _OPEN_EPHYS_RECEIVER_HH
#define _OPEN_EPHYS_RECEIVER_HH

#include "../data_writer.hh"

namespace jill { namespace net {

/** A data_writer that sends messages to an open-ephys NetworkEvents plugin */
class open_ephys_receiver : public data_writer {
public:
	open_ephys_receiver(std::string const & endpoint);
	~open_ephys_receiver() override;

        /* override the write method to send messages over zmq */
        void write(data_block_t const *, nframes_t, nframes_t) override;

	/** send a message to the remote endpoint (ignores reply) */
	void send(std::string const & message);
private:
	std::string _endpoint;
	void * _socket;       // zmq socket

};


}} // jill::net


#endif
