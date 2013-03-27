#ifndef _EVENT_LOGGER_HH
#define _EVENT_LOGGER_HH

#include <boost/noncopyable.hpp>
#include <iosfwd>

namespace jill {

/** ABC for classes that can log messages */
class event_logger : boost::noncopyable {

public:
        virtual ~event_logger() {}

        /** log an event with time and source information */
        virtual std::ostream & log() = 0;

        /** log a message (source information only) */
        virtual std::ostream & msg() = 0;

        /** log an event (with an alternate source) */
        virtual std::ostream & operator<< (std::string const & source) = 0;

        /** access or change the source */
        virtual std::string & source() = 0;
};

}

#endif
