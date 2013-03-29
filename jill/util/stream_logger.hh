#ifndef _STREAM_LOGGER_HH
#define _STREAM_LOGGER_HH

#include <string>
#include "../event_logger.hh"

namespace jill { namespace util {

/** Implements a simple event logger that prints messages to stdout */
class stream_logger : public event_logger {

public:
        stream_logger(std::string const & source, std::ostream & os);
        ~stream_logger() {}

        std::string const & source() { return _source; }

private:
        void log(timestamp const &, std::string const &);
        std::ostream & _os;
        std::string _source;
};

}}

#endif
