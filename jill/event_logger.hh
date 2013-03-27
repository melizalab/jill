#ifndef _EVENT_LOGGER_HH
#define _EVENT_LOGGER_HH

#include <boost/noncopyable.hpp>
#include <iosfwd>

namespace jill {

/**
 * ABC for classes that can log messages. It provides a stream interface and an
 * option to log the time and source, or just the source.  May be implemented
 * directly using an external stream or using a friend stream proxy.
 */
class event_logger : boost::noncopyable {

        friend class logger_proxy;

public:
        virtual ~event_logger() {}

        /** log an event with time and source information */
        virtual std::ostream & log() = 0;

        /** log an event (with an alternate source) */
        virtual std::ostream & operator<< (std::string const & source) = 0;

private:
        /** actually write the log message - may be called by logger_proxy */
        virtual std::streamsize log(char const * msg, std::streamsize n) { return 0; }
};

}

#endif
