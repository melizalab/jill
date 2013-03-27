#ifndef _STREAM_LOGGER_HH
#define _STREAM_LOGGER_HH

#include <string>
#include "../event_logger.hh"

namespace jill { namespace util {

/** Implements a simple event logger that prints messages to stdout */
class stream_logger : public event_logger {

public:
        stream_logger(std::ostream & os, std::string const & source);
        ~stream_logger() {}
        std::ostream & log();
        std::ostream & msg();
        std::ostream & operator<< (std::string const & source);
        std::string & source() { return _source; }

protected:
        std::string timestamp() const;

private:
        std::ostream & _os;
        std::string _source;
};

}}

#endif
