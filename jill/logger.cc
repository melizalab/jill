
#include "logger.hh"
#include <sstream>
#include <zmq.h>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace jill;

// TODO shims for zmq 2.2 vs 3.3


log_msg::log_msg()
        : _creation(boost::posix_time::microsec_clock::universal_time())
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
{}

logger::~logger()
{
        zmq_close(_socket);
        zmq_term(_context);
}

void
logger::log(timestamp_t const & utc, std::string const & msg) const
{
        typedef boost::date_time::c_local_adjustor<timestamp_t> local_adj;
        timestamp_t local = local_adj::utc_to_local(utc);
        std::cout << boost::posix_time::to_iso_string(local) << " [" << _source << "] "
                  << msg << std::endl;
}

void
logger::set_sourcename(std::string const & name)
{
        _source = name;
}

void
logger::connect(std::string const & server_name)
{
        // this will work even if the server isn't up yet
        std::ostringstream endpoint;
        // TODO set socket identity?
        endpoint << "ipc:///tmp/org.meliza.jill/" << server_name << "/msg";
        zmq_connect(_socket, endpoint.str().c_str());
}

