
#include <sstream>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "event_logger.hh"

using namespace jill;

log_msg::log_msg(event_logger &w)
        : _w(w),
          _creation(boost::posix_time::microsec_clock::universal_time()),
          _stream(new std::ostringstream)
{}

log_msg::~log_msg()
{
        _w.write_log(_creation, _stream->str());
        delete _stream;
}
