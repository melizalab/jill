#ifndef _EVENT_LOGGER_HH
#define _EVENT_LOGGER_HH

// #include <iosfwd>
#include <boost/noncopyable.hpp>
#include <sstream>

// #include <boost/iostreams/categories.hpp>  // sink_tag

namespace jill {

struct log_proxy;

/**
 * ABC for classes that can log messages. It provides a stream interface and an
 * option to log the time and source, or just the source.  May be implemented
 * directly using an external stream or using a friend stream proxy.
 */
struct event_logger : boost::noncopyable {
        friend class log_proxy;

        virtual ~event_logger() {}
        /** log an event with time and default source information */
        virtual log_proxy log() = 0;

private:
        /** write a raw message to the log */
        virtual std::streamsize log(const char *s, std::streamsize n) = 0;
};

struct log_proxy {
        log_proxy(event_logger &w) : _w(w) {}
        ~log_proxy() {
// send message
        }
        std::streamsize write(char const * s, std::streamsize n) {
                return _w.log(s, n);
        }
private:
        event_logger &_w;
};

}

#endif
