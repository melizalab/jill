#include <boost/date_time/posix_time/posix_time.hpp>

#include "stream_logger.hh"

using namespace std;
using namespace boost::posix_time;
using namespace jill::util;

stream_logger::stream_logger(string const & source, ostream & os)
        : _os(os), _source(source) {}

void
stream_logger::log(jill::timestamp const & t, std::string const & msg)
{
        _os << to_iso_string(t) << ' ' << msg << endl;
}
