/*
 * A really stripped down jrecord for testing arf_thread.
 *
 */
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <map>
#include <string>

#include "jill/jack_client.hh"
#include "jill/dsp/period_ringbuffer.hh"
#include "jill/file/arf_thread.hh"

#define CLIENT_NAME "test_arf_thread"
#define NSAMPLED 1
#define NEVENT 1
#define COMPRESSION 1

using namespace jill;

boost::scoped_ptr<jack_client> client;
dsp::period_ringbuffer ringbuffer(1024);
boost::scoped_ptr<file::arf_thread> arf_thread;
std::map<std::string, std::string> attrs;

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        sample_t *port_buffer;
        std::list<jack_port_t*>::const_iterator it;

        if (ringbuffer.reserve(time, nframes, client->ports().size()) == 0) {
                arf_thread->xrun();
                return 0;
        }

        for (it = client->ports().begin(); it != client->ports().end(); ++it) {
                port_buffer = client->samples(*it, nframes);
                ringbuffer.push(port_buffer);
        }

	/* signal that there is data available */
	if (pthread_mutex_trylock (&disk_thread_lock) == 0) {
	    pthread_cond_signal (&data_ready);
	    pthread_mutex_unlock (&disk_thread_lock);
	}

        return 0;
}


void
setup()
{
        attrs["creator"] = CLIENT_NAME;
        char buf[64];

        client.reset(new jack_client(CLIENT_NAME));
        for (int i = 0; i < NSAMPLED; ++i) {
                sprintf(buf,"pcm_%04d",i);
                client->register_port(buf,JACK_DEFAULT_AUDIO_TYPE,
                                      JackPortIsInput | JackPortIsTerminal);
        }
        for (int i = 0; i < NEVENT; ++i) {
                sprintf(buf,"evt_%04d",i);
                client->register_port(buf,JACK_DEFAULT_MIDI_TYPE,
                                      JackPortIsInput | JackPortIsTerminal);
        }

        ringbuffer.resize(client->buffer_size() * (NSAMPLED+NEVENT) * 10);
        client->log() << "ringbuffer size (bytes)" << ringbuffer.write_space();

        arf_thread.reset(new file::arf_thread("test.arf",
                                              &attrs,
                                              client.get(),
                                              &ringbuffer,
                                              COMPRESSION));

        client->set_process_callback(process);
        arf_thread->start();
        client->activate();
}

void
signal_handler(int sig)
{
        arf_thread->stop();
}


void
teardown()
{
        arf_thread->join();
        client->deactivate();
}

void
test_write_log()
{
        arf_thread->log("[" CLIENT_NAME "] a random log message");
}

int
main(int argc, char **argv)
{
        setup();
        signal(SIGINT,  signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGHUP,  signal_handler);
        printf("Hit Ctrl-C to stop test\n");

        test_write_log();
        teardown();

        printf("passed tests\n");
        return 0;
}
