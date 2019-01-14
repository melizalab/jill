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
#ifndef _ZMQ_HELPERS_H_
#define _ZMQ_HELPERS_H_

#include <memory>
#include <string>
#include <vector>
#include <boost/noncopyable.hpp>

#include <zmq.h>

namespace zmq {

//using socket_t = std::shared_ptr<void>;

/** Smart pointer container for zmq message */
using msg_ptr_t = std::shared_ptr<zmq_msg_t>;


/** global singleton class for context */
class context : boost::noncopyable {

public:
        static context & instance() {
                static context _instance;
                return _instance;
        }

        static void * socket(int type);

private:
        context();
        ~context();
        void * _context;
};



/** Create an empty zmq message */
msg_ptr_t msg_init();

/** Create a zmq message of a specified size */
msg_ptr_t msg_init(std::size_t size);

/** Extract the contents of a message as a string, copying to a new string */
std::string msg_str (msg_ptr_t const & message);

/** Send a string as a message */
int send(void * socket, std::string const & data, int flags=0);
int send(void * socket, char const * data, int flags=0);


/** Send a sequence of objects */
template <typename Iterator>
int send_n(void * socket, Iterator it, size_t n, int flags=0)
{
        size_t i;
        for (i = 0; i < n - 1; ++i) {
                int rc = send(socket, *it, flags | ZMQ_SNDMORE);
                if (rc < 0) return rc;
                ++it;
        }
        if (i < n)
                return send(socket, *it, flags);
        else
                return 0;
}


/** receive a series of messages and parse them into a vector of strings */
std::vector<std::string> recv(void * socket, int flags=0);

}





#endif /* _ZMQ_HELPERS_H_ */
