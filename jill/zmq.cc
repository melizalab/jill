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
#include <cstring>
#include <cassert>
#include <vector>

#include "zmq.hh"


static void
msg_close(zmq_msg_t * message)
{
        zmq_msg_close(message);
        delete message;
}


zmq::msg_ptr_t
zmq::msg_init()
{
        zmq::msg_ptr_t message(new zmq_msg_t, msg_close);
        int rc = zmq_msg_init (message.get());
        assert(rc == 0);
        return message;
}


zmq::msg_ptr_t
zmq::msg_init(std::size_t size)
{
        zmq::msg_ptr_t message(new zmq_msg_t, msg_close);
        int rc = zmq_msg_init_size (message.get(), size);
        assert(rc == 0);
        return message;
}


std::string
zmq::msg_str(zmq::msg_ptr_t const & message)
{
        zmq_msg_t * msg = const_cast<zmq_msg_t *>(message.get());
        return std::string((char *) zmq_msg_data(msg), zmq_msg_size(msg));
}

template<>
int
zmq::send(void *socket, std::string const & string, int flags)
{
        zmq::msg_ptr_t message = msg_init(string.length());
        memcpy (zmq_msg_data (message.get()), string.c_str(), string.length());
        int rc = zmq_msg_send (message.get(), socket, flags);
        return rc;
}

std::vector<std::string>
zmq::recv(void * socket)
{
        more_t more = 1;
        size_t more_size = sizeof(more);
        std::vector<std::string> messages;
        while (more) {
                zmq::msg_ptr_t message = zmq::msg_init();
                int rc = zmq_msg_recv (message.get(), socket, ZMQ_DONTWAIT);
                if (rc < 0) break;
                messages.push_back(zmq::msg_str(message));
                zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
        }
        return messages;

}
