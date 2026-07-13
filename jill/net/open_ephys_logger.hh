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
#ifndef _OPEN_EPHYS_NETWORK_HH
#define _OPEN_EPHYS_NETWORK_HH

#include "../data_writer.hh"

namespace jill { namespace net {

/** A data_writer that sends messages to an open-ephys NetworkEvents plugin */
class open_ephys_logger : public data_writer {
public:
	open_ephys_logger(std::string const & endpoint);
	~open_ephys_logger() override;

        /* data_writer overrides. Many of these are no-ops */
	// not overridden: log, flush
        bool ready() const override { return true; }
        void new_entry(nframes_t) override {}
        void close_entry() override {}
        void xrun() override {}
        void write(data_block_t const *, nframes_t, nframes_t) override;

	void send(char const * message);
	
private:
	std::string _endpoint;
	void * _socket;       // zmq socket
};


}} // jill::net


#endif
