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
#include "logging.hh"
#include "logger.hh"
#include "net/zmq.hh"
#include <sstream>
#include <cstdio>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace boost::posix_time;
using namespace jill;
using namespace jill::net;

log_msg::log_msg()
        : _creation(microsec_clock::universal_time())
{}

log_msg::log_msg(timestamp_t const & utc)
        : _creation(utc)
{}

log_msg::~log_msg()
{
        // Every LOG statement ends here, so nothing this does may throw: a
        // destructor that propagates terminates the process, and an unattended
        // recorder would die of a failed log line. The paths that can throw are
        // real rather than theoretical -- the local-time conversion below
        // throws when the C library cannot convert, and the string work can
        // throw bad_alloc.
        //
        // Swallowing is the only option left at this point. Reporting the
        // failure would mean logging it, which arrives back here.
        try {
                logger::instance().log(_creation, _stream.str());
        }
        catch (...) {
                // @intentional: nowhere left to report it, see above
        }
}

logger::logger()
        // initialize zmq socket. Use a dealer socket because log messages are
        // asynchronous (no response from recipient).
        : _socket(zmq::context::socket(ZMQ_DEALER)),
          _connected(false),
          _forwarding(true),
          _dropped(0)
{
        // Line-buffer the console log. stdio picks block buffering whenever
        // stdout is not a terminal, so a module whose output is piped or
        // redirected -- to tee, to grep, to a log file under a supervisor --
        // otherwise shows nothing for kilobytes at a time, and loses the tail
        // entirely if it is killed rather than shut down. That turns the log
        // into something you can only read post mortem, and only sometimes.
        setvbuf(stdout, nullptr, _IOLBF, 0);
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
	zmq::close(_socket);
}

void
logger::log(timestamp_t const & utc, std::string const & msg)
{
        /**
         * Fall back to UTC rather than losing the line. utc_to_local reaches
         * the C library's localtime, which throws if it cannot convert, and
         * that happens before anything is printed -- so a conversion failure
         * would discard the message as well as unwinding.
         */
        using local_adj = boost::date_time::c_local_adjustor<timestamp_t>;
        timestamp_t local = utc;
        try {
                local = local_adj::utc_to_local(utc);
        }
        catch (std::exception const &) {
                // @intentional: keep the UTC stamp rather than lose the line
        }
        // printf calls are generally threadsafe
        printf("%s [%s] %s\n", to_iso_string(local).c_str(), _source.c_str(), msg.c_str());

        if (_connected) {
                // lock here because multiple threads may be calling this
                // function, but only one can access the socket at a time
                std::lock_guard<std::mutex> lock(_lock);
                const bool sent = forward(utc, msg);
                // Report each change of state once rather than per message.
                // printf rather than LOG, because LOG comes back here.
                if (!sent && _forwarding) {
                        printf("%s [%s] log server is not keeping up;"
                               " messages will not be forwarded until it does"
                               " (they are still printed here)\n",
                               to_iso_string(local).c_str(), _source.c_str());
                }
                else if (sent && !_forwarding) {
                        printf("%s [%s] log server is accepting messages again;"
                               " %lu were dropped\n",
                               to_iso_string(local).c_str(), _source.c_str(),
                               _dropped);
                        _dropped = 0;
                }
                if (!sent) _dropped += 1;
                _forwarding = sent;
        }
}

/**
 * The send must not block, and the queue must not be given up.
 *
 * Connecting to an endpoint nothing has bound yet is normal and useful: an
 * orchestration script cannot guarantee jrecord wins the race to start, and
 * zeromq holds up to ZMQ_SNDHWM (1000) messages so that everything said
 * meanwhile is delivered once it arrives. Refusing to queue -- ZMQ_IMMEDIATE
 * would do that -- fixes nothing worth fixing and loses this.
 *
 * A blocking send is the other trap. With the queue full it stops the caller
 * indefinitely while holding _lock, so every thread that logs piles up behind
 * it and the process goes on looking alive while doing nothing. Dropping the
 * remote copy instead costs nothing that is not already on stdout, and the
 * queue drains and starts accepting again by itself once a server appears.
 *
 * A full queue refuses at frame one, because zeromq checks the high water mark
 * when a message begins rather than partway through, so abandoning here cannot
 * leave a half-written message to corrupt the next one. A failure on a later
 * frame should be unreachable; it is reported rather than handled, since it
 * would mean the far end is desynchronized and that must not pass silently.
 */
bool
logger::forward(timestamp_t const & utc, std::string const & msg)
{
        // message consists of the source name, the timestamp (as an iso
        // string), and the actual log message. Note that the zmq dealer socket
        // doesn't prepend an address envelope, so this is what the recipient
        // router socket will see
        if (zmq::send(_socket, _source, ZMQ_SNDMORE | ZMQ_DONTWAIT) < 0) {
                return false;
        }
        if (zmq::send(_socket, to_iso_string(utc), ZMQ_SNDMORE | ZMQ_DONTWAIT) < 0 ||
            zmq::send(_socket, msg, ZMQ_DONTWAIT) < 0) {
                printf("[%s] WARNING: log message truncated in transit;"
                       " the receiving end may be out of step\n", _source.c_str());
                return false;
        }
        return true;
}

void
logger::set_sourcename(std::string const & name)
{
        _source = name;
}

void
logger::connect(std::string const & server_name)
{
        DBG << "connecting logger to " << server_name;
        if (_connected) {
                DBG << "socket already connected";
                return;
        }
        // If the server hasn't bound to the endpoint, messages will queue up
        // and be transmitted when it does. Note: this will eventually cause the
        // program's zeromq context to shut down if the connection is never made.
        std::ostringstream endpoint;
        endpoint << "ipc:///tmp/org.meliza.jill/" << server_name << "/msg";
	if (zmq::connect(_socket, endpoint.str()) != 0) {
                LOG << "error connecting to endpoint " << endpoint.str();
        }
        else {
                INFO << "logging to " << endpoint.str();
                _connected = true;
        }
}

void
logger::disconnect(std::string const & server_name)
{
        DBG << "disconnecting logger from " << server_name;
        if (!_connected) {
                DBG << "socket is not connected";
                return;
        }
        std::ostringstream endpoint;
        endpoint << "ipc:///tmp/org.meliza.jill/" << server_name << "/msg";
	if (zmq::disconnect(_socket, endpoint.str()) != 0) {
                LOG << "error disconnecting from endpoint " << endpoint.str();
        }
        else {
                INFO << "no longer logging to " << endpoint.str();
                _connected = false;
        }
}
