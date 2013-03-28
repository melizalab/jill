#include <boost/date_time/posix_time/posix_time.hpp>

#include "stream_logger.hh"

using namespace std;
using namespace boost::posix_time;
using namespace jill::util;

stream_logger::stream_logger(ostream & os, string const & source)
        : _os(os), _source(source) {}

ostream &
stream_logger::log()
{
        return _os << timestamp() << " [" << _source << "] ";
}

string
stream_logger::timestamp() const
{
        ptime t(microsec_clock::local_time());
        return to_iso_string(t);
}
