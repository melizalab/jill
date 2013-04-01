#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time/c_local_time_adjustor.hpp>

#include "stream_logger.hh"

using namespace std;
using namespace boost::posix_time;
using namespace jill::util;

stream_logger::stream_logger(string const & source, ostream & os)
        : _os(os), _source(source) {}

void
stream_logger::write_log(jill::timestamp const & utc, std::string const & msg)
{
        typedef boost::date_time::c_local_adjustor<ptime> local_adj;
        ptime t = local_adj::utc_to_local(utc);
        _os << to_iso_string(t) << " [" << _source << "] " << msg << endl;
}
