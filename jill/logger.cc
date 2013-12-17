/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include "logging.hh"
#include "logger.hh"
#include "zmq.hh"
#include <sstream>
#include <cstdio>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::posix_time;
using namespace jill;

log_msg::log_msg()
        : _creation(microsec_clock::universal_time())
{}

log_msg::log_msg(timestamp_t const & utc)
        : _creation(utc)
{}

log_msg::~log_msg()
{
        logger::instance().log(_creation, _stream.str());
}

logger::logger()
        // initialize zmq context and socket. Use a dealer socket because log
        // messages are asynchronous (no response from recipient).
        : _context(zmq_init(1)), _socket(zmq_socket(_context, ZMQ_DEALER)),
          _connected(false)
{
        pthread_mutex_init(&_lock, 0);
}

logger::~logger()
{
        // note: this object may be destroyed before other objects, so it's
        // generally not a good idea to log in destructors
        _connected = false;
        DBG << "cleaning up logger";
        // wait 1 s for server to handle any queued messages, then close the
        // socket and context
        int linger = 1000;
        zmq_setsockopt(_socket, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(_socket);
        zmq_ctx_destroy(_context);
        pthread_mutex_destroy(&_lock);
}

void
logger::log(timestamp_t const & utc, std::string const & msg)
{
        typedef boost::date_time::c_local_adjustor<timestamp_t> local_adj;
        timestamp_t local = local_adj::utc_to_local(utc);
        // printf calls are generally threadsafe
        printf("%s [%s] %s\n", to_iso_string(local).c_str(), _source.c_str(), msg.c_str());

        if (_connected) {
                pthread_mutex_lock(&_lock);
                // message consists of the source name, the timestamp (as an iso
                // string), and the actual log message. Note that the zmq dealer socket
                // doesn't prepend an address envelope, so this is what the recipient
                // router socket will see
                zmq::send(_socket, _source, ZMQ_SNDMORE);
                zmq::send(_socket, to_iso_string(utc), ZMQ_SNDMORE);
                zmq::send(_socket, msg);
                pthread_mutex_unlock(&_lock);
        }
}

void
logger::set_sourcename(std::string const & name)
{
        _source = name;
}

void
logger::connect(std::string const & server_name)
{
        if (_connected) {
                DBG << "socket already connected";
                return;
        }
        // If the server hasn't bound to the endpoint, messages will queue up
        // and be transmitted when it does.
        std::ostringstream endpoint;
        endpoint << "ipc:///tmp/org.meliza.jill/" << server_name << "/msg";
        if (zmq_connect(_socket, endpoint.str().c_str()) != 0) {
                LOG << "error connecting to endpoint " << endpoint.str();
        }
        else {
                INFO << "logging to " << endpoint.str();
                _connected = true;
        }
}

