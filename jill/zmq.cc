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
#include <cstring>
#include <cassert>

#include "zmq.hh"

namespace zmq {

static void
msg_close(zmq_msg_t * message)
{
        zmq_msg_close(message);
        delete message;
}


msg_ptr_t
msg_init()
{
        msg_ptr_t message(new zmq_msg_t, msg_close);
        int rc = zmq_msg_init (message.get());
        assert(rc == 0);
        return message;
}


msg_ptr_t
msg_init(std::size_t size)
{
        msg_ptr_t message(new zmq_msg_t, msg_close);
        int rc = zmq_msg_init_size (message.get(), size);
        assert(rc == 0);
        return message;
}


std::string
msg_str(msg_ptr_t const & message)
{
        zmq_msg_t * msg = const_cast<zmq_msg_t *>(message.get());
        return std::string((char *) zmq_msg_data(msg), zmq_msg_size(msg));
}

int
send(void *socket, std::string const & string, int flags)
{
        zmq_msg_t message;
        zmq_msg_init_size (&message, string.length());
        memcpy (zmq_msg_data (&message), string.c_str(), string.length());
        int rc = zmq_send (socket, &message, flags);
        zmq_msg_close (&message);
        return rc;
}

}
