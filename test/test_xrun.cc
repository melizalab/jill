/* generates an xrun by waiting too long in the process loop */
#include <iostream>
#include <boost/shared_ptr.hpp>
#include "jill/jack_client.hh"
#include "jill/logging.hh"

using namespace jill;

boost::shared_ptr<jack_client> client;
long xrun_usec = 0;

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        usleep(xrun_usec * 2);
        return 0;
}

int
main(int argc, char **argv)
{
	using namespace std;
        client.reset(new jack_client("test_xrun"));

        xrun_usec = 1e6 * client->buffer_size() / client->sampling_rate();
        LOG << "period duration: " << xrun_usec << " usec";

        client->set_process_callback(process);
        client->activate();

        usleep(xrun_usec * 10);
        client->deactivate();

}
