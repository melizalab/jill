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

        std::string const & source() { return _source; }

protected:
        std::string timestamp() const;

private:
        std::streamsize log(const char *s, std::streamsize n) { return 0; }
        std::ostream & _os;
        std::string _source;
};

}}

#endif
