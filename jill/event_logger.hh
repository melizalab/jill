#ifndef _EVENT_LOGGER_HH
#define _EVENT_LOGGER_HH

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <iosfwd>

namespace jill {

typedef boost::posix_time::ptime timestamp;

class event_logger;

/**
 * Object returned by event_logger::log(). On destruction it passes the message
 * and timestamp back to the event_logger that generated it.
 */
class log_msg {

public:
        log_msg(event_logger &);
        // log_msg & operator= (log_msg &);
        ~log_msg();

        template <typename T>
        log_msg & operator<< (T const& t) {
                *_stream << t;
                return *this;
        }

        log_msg & operator<< (std::ostream & (*pf)(std::ostream &)) {
                pf(*_stream);
                return *this;
        }

private:
        event_logger &_w;
        timestamp _creation;    // in utc
        std::ostringstream * _stream;
};

/**
 * ABC for classes that can log messages. The public log() member function
 * returns a proxy object that behaves like a stream.
 */
class event_logger : boost::noncopyable {
        friend class log_msg;
public:
        virtual ~event_logger() {}
        /** log an event with time and default source information */
        log_msg log() { return log_msg(*this); }

private:
        /**
         * write message to the log. called by log_msg.
         *
         * @param utc  the time of the message, in UTC
         */
        virtual void write_log(timestamp const &utc, std::string const &) = 0;
};

}

#endif
