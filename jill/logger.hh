#ifndef _LOGGER_HH
#define _LOGGER_HH

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/ptime.hpp>
#include <iosfwd>

#define LOG log_msg()

namespace jill {

typedef boost::posix_time::ptime timestamp;

/**
 * Simple atomic logging class with stream support.
 *
 * Usage:
 * log_msg() << "this is a message with " << 7 << " words";
 *
 * This example creates a message that can be constructed with stream operators
 * and that will be written on destruction (here, at the end of the statement).
 *
 */
class log_msg : boost::noncopyable {

public:
        log_msg();
        explicit log_msg(timestamp const & utc);
        ~log_msg();

        template <typename T>
        log_msg & operator<< (T const & t) {
                _stream << t;
                return *this;
        }

        log_msg & operator<< (std::ostream & (*pf)(std::ostream &)) {
                pf(_stream);
                return *this;
        }

private:
        timestamp _creation;    // in utc
        std::ostringstream _stream;
};

/**
 * Singleton class for logging. This is used by the log_msg class to actually
 * write the log messages. Users generally don't have to interact with this
 * directly except to set the source name.
 */
class logger : boost::noncopyable {
public:
        logger();
        ~logger();
        static logger & instance() {
                // threadsafe in gcc to initialize static locals
                static logger _instance;
                return _instance;
        }
        void log(timestamp const & utc, std::string const & msg) const;
        void set_sourcename(std::string const & name);
        void connect(std::string const & server_name);
private:
        std::string _source;
        void * _context;
        void * _socket;
};

}

#endif
