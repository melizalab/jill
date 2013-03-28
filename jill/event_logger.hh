#ifndef _EVENT_LOGGER_HH
#define _EVENT_LOGGER_HH

#include <iosfwd>
#include <boost/noncopyable.hpp>
#include <boost/iostreams/categories.hpp>  // sink_tag

namespace jill {

/**
 * ABC for classes that can log messages. It provides a stream interface and an
 * option to log the time and source, or just the source.  May be implemented
 * directly using an external stream or using a friend stream proxy.
 */
struct event_logger : boost::noncopyable {
        friend class logger_proxy;
        virtual ~event_logger() {}
        /** log an event with time and default source information */
        virtual std::ostream & log() = 0;
        /** redirect the log */
        virtual void redirect(event_logger &) {}
private:
        /** write a raw message to the log. only called by logger_proxy */
        virtual std::streamsize log(const char *s, std::streamsize n) = 0;
};

struct logger_proxy {
        typedef char                        char_type;
        typedef boost::iostreams::sink_tag  category;

        logger_proxy(event_logger &w) : _w(w) {}
        std::streamsize write(char const * s, std::streamsize n) {
                return _w.log(s, n);
        }
private:
        event_logger &_w;
};

}

#endif
