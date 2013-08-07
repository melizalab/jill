
#include "logger.hh"
#include <sstream>
#include <boost/date_time/c_local_time_adjustor.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using namespace jill;


log_msg::log_msg()
        : _creation(boost::posix_time::microsec_clock::universal_time())
{}

log_msg::log_msg(timestamp const & utc)
        : _creation(utc)
{}

log_msg::~log_msg()
{
        logger::instance().log(_creation, _stream.str());
}

void
logger::log(timestamp const & utc, std::string const & msg) const
{
        typedef boost::date_time::c_local_adjustor<timestamp> local_adj;
        timestamp local = local_adj::utc_to_local(utc);
        std::cout << boost::posix_time::to_iso_string(local) << " [" << _source << "] "
                  << msg << std::endl;
}

void
logger::set_sourcename(std::string const & name)
{
        _source = name;
}
