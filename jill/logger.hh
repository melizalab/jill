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
#ifndef _LOGGER_HH
#define _LOGGER_HH

#include <boost/noncopyable.hpp>
#include <mutex>

namespace jill {

/**
 * Logs messages to console and optionally to an external logger. This is used by
 * the log_msg class to actually write the log messages. Users generally don't
 * have to interact with this directly except to set the source name, which is
 * used in formatting log messages, and to connect to the external logger.
 *
 * This class is a singleton and can only be accessed through instance()
 */
class logger : boost::noncopyable {
public:

        /** Access the instance of the logger */
        static logger & instance() {
                // threadsafe in gcc to initialize static locals
                static logger _instance;
                return _instance;
        }

        /** Log a timestamped message */
        void log(timestamp_t const & utc, std::string const & msg);

        /**
         * Set the source name, which is used to label log entries. The name of
         * the process is a good choice.
         */
        void set_sourcename(std::string const & name);

        /**
         * Connect the logger to a zeromq socket. Messages will be sent to this
         * socket. Usually the program binding to the remote socket will store
         * the messages in a centralized log file.
         *
         * @param server_name  the name of the jack server. All the clients of
         *                     the server must log to the same socket.
         *
         * @note once the logger is connected, subsequent calls to this function
         *       do nothing.
         */
        void connect(std::string const & server_name);

private:
        logger();
        ~logger();

        std::string _source;
        std::mutex _lock;       // mutex for zmq socket access
        void * _socket;         // zmq socket
        bool _connected;        // was connection successful?
};

}

#endif
