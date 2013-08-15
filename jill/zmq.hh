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
#include <cstdio>

// TODO shims for zmq 2.2 vs 3.3

namespace jill { namespace zmq {

inline char *
recv_str (void *socket)
{
        zmq_msg_t message;
        zmq_msg_init (&message);
        if (zmq_recv (socket, &message, 0))
                return 0;
        int size = zmq_msg_size (&message);
        // char string[size + 1];
        char * string = new char[size + 1];
        memcpy (string, zmq_msg_data (&message), size);
        zmq_msg_close (&message);
        string[size] = 0;
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

// Receives all message parts from socket, prints neatly
//
inline void
s_dump (void *socket)
{
    puts ("----------------------------------------");
    while (1) {
        // Process all parts of the message
        zmq_msg_t message;
        zmq_msg_init (&message);
        zmq_recv (socket, &message, 0);

        // Dump the message as text or binary
        char *data = (char*) zmq_msg_data (&message);
        int size = zmq_msg_size (&message);
        int is_text = 1;
        int char_nbr;
        for (char_nbr = 0; char_nbr < size; char_nbr++)
            if ((unsigned char) data [char_nbr] < 32
            || (unsigned char) data [char_nbr] > 127)
                is_text = 0;

        printf ("[%03d] ", size);
        for (char_nbr = 0; char_nbr < size; char_nbr++) {
            if (is_text)
                printf ("%c", data [char_nbr]);
            else
                printf ("%02X", (unsigned char) data [char_nbr]);
        }
        printf ("\n");

        int64_t more; // Multipart detection
        size_t more_size = sizeof (more);
        zmq_getsockopt (socket, ZMQ_RCVMORE, &more, &more_size);
        zmq_msg_close (&message);
        if (!more)
            break; // Last message part
    }
}

}}

#endif /* _ZMQ_HELPERS_H_ */
