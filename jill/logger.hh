/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _LOGGER_HH
#define _LOGGER_HH

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <pthread.h>
#include <iosfwd>

#ifndef DEBUG
#define DEBUG 0
#endif

#define LOG log_msg()

#define INFO \
        if (DEBUG < 1) ; \
        else log_msg() << "I: "

#define DBG \
        if (DEBUG < 2) ; \
        else log_msg() << "D: "

namespace jill {

/** Define timestamp type */
typedef boost::posix_time::ptime timestamp_t;

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
        void log(timestamp_t const & utc, std::string const & msg);
        void set_sourcename(std::string const & name);
        void connect(std::string const & server_name);
        void * get_context() { return _context; }
private:
        std::string _source;
        pthread_mutex_t _lock;  // mutex for zmq socket access
        void * _context;
        void * _socket;

};

}

#endif
