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
#ifndef _ZMQ_HELPERS_H_
#define _ZMQ_HELPERS_H_

#include <zmq.h>
#include <string>
#include <cstring>

// TODO shims for zmq 2.2 vs 3.3

namespace jill { namespace zmq {

inline std::string
recv_str (void *socket)
{
        zmq_msg_t message;
        zmq_msg_init (&message);
        if (zmq_recv (socket, &message, 0))
                return (NULL);
        int size = zmq_msg_size (&message);
        char string[size + 1];
        memcpy (string, zmq_msg_data (&message), size);
        zmq_msg_close (&message);
        string [size] = 0;
        return string;
}

inline int
send_msg (void *socket, std::string const & string, int flags=0)
{
        zmq_msg_t message;
        zmq_msg_init_size (&message, string.length());
        memcpy (zmq_msg_data (&message), string.c_str(), string.length());
        int rc = zmq_send (socket, &message, flags);
        zmq_msg_close (&message);
        return rc;
}

}}

#endif /* _ZMQ_HELPERS_H_ */
