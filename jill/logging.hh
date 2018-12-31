/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _LOGGING_HH
#define _LOGGING_HH

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <iosfwd>

#ifndef DEBUG
#define DEBUG 0
#endif

#define LOG log_msg()
#define INFO log_msg()

#define DBG \
        if (DEBUG < 2) ; \
        else log_msg() << "D: "

namespace jill {

/** Define timestamp type */
using timestamp_t = boost::posix_time::ptime;

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
        explicit log_msg(timestamp_t const & utc);
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
        timestamp_t _creation;    // in utc
        std::ostringstream _stream;
};

}

#endif
