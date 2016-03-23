#include <signal.h>
#include <iostream>
#include <sstream>
#include <cstdio>
#include <boost/filesystem.hpp>
#include "jill/zmq.hh"

using namespace std;
namespace fs = boost::filesystem;

// Receives all message parts from socket, prints neatly
//
static void
s_dump (void *socket)
{
    puts ("----------------------------------------");
    while (1) {
        // Process all parts of the message
        zmq_msg_t message;
        zmq_msg_init (&message);
        zmq_msg_recv (&message, socket, 0);

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

static int s_interrupted = 0;
static void s_signal_handler (int signal_value)
{
        s_interrupted = 1;
}

static void s_catch_signals (void)
{
        struct sigaction action;
        action.sa_handler = s_signal_handler;
        action.sa_flags = 0;
        sigemptyset (&action.sa_mask);
        sigaction (SIGINT, &action, NULL);
        sigaction (SIGTERM, &action, NULL);
}

int
main(int argc, char ** argv)
{
        void * _context;
        void * _socket;

        _context = zmq_init(1);
        _socket = zmq_socket(_context, ZMQ_DEALER);

        ostringstream endpoint;
        fs::path path("/tmp/org.meliza.jill/default");
        if (!fs::exists(path)) {
                fs::create_directories(path);
        }
        path /= "msg";
        endpoint << "ipc://" << path.string();
        if (zmq_bind(_socket, endpoint.str().c_str()) < 0) {
                cout << "unable to bind to endpoint " << endpoint.str() << endl;
        }
        else {
                cout << "logger bound to " << endpoint.str() << endl;
        }

        s_catch_signals();
        while (!s_interrupted) {
                s_dump(_socket);
        }

        zmq_close(_socket);
        zmq_term(_context);

}
