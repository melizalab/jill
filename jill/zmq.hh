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

#include <zmq.h>
#include <boost/shared_ptr.hpp>
#include <string>

// TODO shims for zmq 2.2 vs 3.3

namespace zmq {

/** Smart pointer container for zmq message */
typedef boost::shared_ptr<zmq_msg_t> msg_ptr_t;

/** Create an empty zmq message */
msg_ptr_t msg_init();

/** Create a zmq message of a specified size */
msg_ptr_t msg_init(std::size_t size);

/** Extract the contents of a message as a string, copying to a new string */
std::string msg_str (msg_ptr_t const & message);

/** Send an object as a message */
template<typename T>
int send(void *socket, T const & data, int flags=0)
{
        msg_ptr_t message = msg_init(sizeof(T));
        memcpy (zmq_msg_data (message.get()), &data, sizeof(T));
        int rc = zmq_send (socket, message.get(), flags);
        return rc;
}

}
#endif /* _ZMQ_HELPERS_H_ */
