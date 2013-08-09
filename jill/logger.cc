
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
        : _context(zmq_init(1)), _socket(zmq_socket(_context, ZMQ_DEALER))
{
}

logger::~logger()
{
        // note: this object may be destroyed before other objects, but
        // apparently it's still okay if the log function gets called. however,
        // generally speaking we shouldn't use destructors to log
        DBG << "cleaning up logger";
        int linger = 1000;
        zmq_setsockopt(_socket, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(_socket);
        zmq_term(_context);
}

void
logger::log(timestamp_t const & utc, std::string const & msg) const
{
        typedef boost::date_time::c_local_adjustor<timestamp_t> local_adj;
        timestamp_t local = local_adj::utc_to_local(utc);
        printf("%s [%s] %s\n", to_iso_string(local).c_str(), _source.c_str(), msg.c_str());

        zmq::send_msg(_socket, to_iso_string(utc), ZMQ_SNDMORE);
        zmq::send_msg(_socket, _source, ZMQ_SNDMORE);
        zmq::send_msg(_socket, msg);

}

void
logger::set_sourcename(std::string const & name)
{
        _source = name;
        zmq_setsockopt(_socket, ZMQ_IDENTITY, _source.c_str(), _source.length());
}

void
logger::connect(std::string const & server_name)
{
        // this will work even if the server isn't up yet
        std::ostringstream endpoint;
        endpoint << "ipc:///tmp/org.meliza.jill/" << server_name << "/msg";
        if (zmq_connect(_socket, endpoint.str().c_str()) < 0) {
                LOG << "error connecting to endpoint " << endpoint.str();
        }
        else {
                INFO << "logging to " << endpoint.str();
        }
}

