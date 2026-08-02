/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <cstring>
#include <new>
#include <vector>
#include <memory>

#include "zmq.hh"

using namespace jill::net;

zmq::context::context()
        : _context(zmq_init(1))
{}


zmq::context::~context()
{
        zmq_ctx_destroy(_context);
}

void *
zmq::context::socket(int type)
{
        return zmq_socket(instance()._context, type);
}


static void
msg_close(zmq_msg_t * message)
{
        zmq_msg_close(message);
        delete message;
}

int
zmq::bind(void * socket, std::string const & string)
{
	return zmq_bind(socket, string.c_str());
}

int
zmq::connect(void * socket, std::string const & string)
{
	return zmq_connect(socket, string.c_str());
}

int
zmq::disconnect(void * socket, std::string const & string)
{
	return zmq_disconnect(socket, string.c_str());
}

int
zmq::close(void * socket)
{
	return zmq_close(socket);
}


zmq::msg_ptr_t
zmq::msg_init()
{
        zmq::msg_ptr_t message(new zmq_msg_t, msg_close);
        // zmq_msg_init() is documented to always return zero, so there is no
        // failure to check for here.
        zmq_msg_init (message.get());
        return message;
}


zmq::msg_ptr_t
zmq::msg_init(std::size_t size)
{
        // zmq_msg_init_size() allocates and fails with ENOMEM if it cannot.
        // Initialize before handing the message to the shared_ptr: msg_close()
        // must never run against a message that was not initialized.
        std::unique_ptr<zmq_msg_t> message(new zmq_msg_t);
        if (zmq_msg_init_size (message.get(), size) != 0)
                throw std::bad_alloc();
        return zmq::msg_ptr_t(message.release(), msg_close);
}


std::string
zmq::msg_str(zmq::msg_ptr_t const & message)
{
        zmq_msg_t * msg = const_cast<zmq_msg_t *>(message.get());
        return std::string((char *) zmq_msg_data(msg), zmq_msg_size(msg));
}

int
zmq::send(void * socket, std::string const & string, int flags)
{
        return zmq_send(socket, string.c_str(), string.length(), flags);
}

int
zmq::send(void * socket, char const * string, int flags)
{
        return zmq_send(socket, string, strlen(string), flags);
}

// template<typename T>
// int send(void * socket, T const & data, int flags=0)
// {
//         msg_ptr_t message = msg_init(sizeof(T));
//         memcpy (zmq_msg_data (message.get()), &data, sizeof(T));
//         int rc = zmq_msg_send (message.get(), socket, flags);
//         return rc;
// }

std::vector<std::string>
zmq::recv(void * socket, int flags)
{
        std::vector<std::string> messages;
        recv(socket, [&messages](zmq::msg_ptr_t const & msg) {
                messages.push_back(zmq::msg_str(msg));
        }, flags);
        return messages;
}
