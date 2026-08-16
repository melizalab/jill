/*
 * Inserts a fixed delay between the input and output
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <csignal>
#include <atomic>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/logging.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jdelay"

using namespace jill;
using std::string;
using sample_ringbuffer = dsp::ringbuffer<sample_t>;

class jdelay_options : public program_options {

public:
        jdelay_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        /** Ports to connect to */
        std::vector<string> input_ports;
        std::vector<string> output_ports;

        float delay_msec;
        nframes_t delay;

protected:

        void print_usage() override;

}; // jdelay_options


static jdelay_options options(PROGRAM_NAME);
static sample_ringbuffer ringbuf(1024);
jack_port_t *port_in, *port_out;
std::atomic<int> ret(EXIT_SUCCESS);
std::atomic<bool> running(true);

/*
 * The process callback introduces a delay by writing and then reading data from
 * a ringbuffer that was padded with zeros equal in length to the delay (see
 * jack_bufsize())
 */
int
process(jack_client *client, nframes_t nframes, nframes_t) JILL_RT
{
        sample_t *in = client->samples(port_in, nframes);
        sample_t *out = client->samples(port_out, nframes);

        if (ringbuf.push(in, nframes) != nframes) {
                DBG << "error: buffer overrun";
        }
        if (ringbuf.pop(out, nframes) != nframes) {
                DBG << "error: buffer underrun";
        }

        return 0;
}

/**
 * This function is called by jack when calculating latency. Its job is to
 * indicate how much latency this client introduces to the processing stream.
 */
void
jack_latency (jack_client * client, jack_latency_callback_mode_t mode)
{
        float sr = 0.001 * client->sampling_rate();
        jack_latency_range_t range;
        if (mode == JackCaptureLatency) {
                jack_port_get_latency_range (port_in, mode, &range);
                LOG << "estimated capture latency (ms): ["
                              << range.min / sr << "," << range.max / sr << "]";
                range.min += options.delay;
                range.max += options.delay;
                jack_port_set_latency_range (port_out, mode, &range);
        }
        else {
                jack_port_get_latency_range (port_out, mode, &range);
                range.min += options.delay;
                range.max += options.delay;
                jack_port_set_latency_range (port_in, mode, &range);
                LOG << "estimated playback latency (ms): ["
                              << range.min / sr << "," << range.max / sr << "]";
        }
}

/**
 * Make the ringbuffer fit the period size and re-establish the delay.
 *
 * Registered before activation, so JACK calls this once during startup with the
 * current period size and then again on any genuine change. It runs on the
 * server's notification thread with the engine stopped, so allocating here is
 * allowed and no process callback is running alongside it.
 */
int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        /* Grow only, but empty either way. The buffer holds the audio that
         * constitutes the delay line, and by the time this runs the stream has
         * already been interrupted -- so those samples are from before the gap
         * and would be spliced back in at the wrong time. Drop them and start
         * the delay again from silence. */
        const std::size_t needed = options.delay + nframes;
        if (needed > ringbuf.size()) {
                ringbuf.resize(needed);         // discards
        }
        else {
                ringbuf.clear();
        }

        // advancing the write pointer past the read pointer by `delay` samples
        // is what puts the output that far behind the input
        ringbuf.push(nullptr, options.delay);
        DBG << "jack period size now " << nframes << "; ringbuffer holds "
            << ringbuf.size() << " samples, " << ringbuf.read_space() << " delayed";
        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        ret = -1;
        running = false;
}

void
signal_handler(int sig)
{
        ret = sig;
        running = false;
}

int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                auto client = jack_client(options.client_name, options.server_name);
                options.delay = options.delay_msec * client.sampling_rate() / 1000;
                LOG << "delay: " << options.delay_msec << " ms (" << options.delay << " frames)";

                port_in = client.register_port("in",JACK_DEFAULT_AUDIO_TYPE,
                                                JackPortIsInput, 0);
                port_out = client.register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client.set_shutdown_callback(jack_shutdown);
                client.set_buffer_size_callback(jack_bufsize);
                client.set_xrun_callback(jack_xrun);
                client.set_process_callback(process);
                client.set_latency_callback(jack_latency);

                activated_client active(client);
                active.connect_ports(options.input_ports.begin(), options.input_ports.end(), "in");
                active.connect_ports("out", options.output_ports.begin(), options.output_ports.end());

                while (running) {
                        usleep(100000);
                }

                return ret;
        }

        catch (Exit const &e) {
                return e.status();
        }
        catch (std::exception const &e) {
                LOG << "ERROR: " << e.what();
                return EXIT_FAILURE;
        }

}


jdelay_options::jdelay_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<vector<string> >(&input_ports), "add connection to input port")
                ("out,o",     po::value<vector<string> >(&output_ports), "add connection to output port");

        // tropts is a group of options
        po::options_description opts("Delay options");
        opts.add_options()
                ("delay,d",   po::value<float>(&delay_msec)->default_value(10),
                 "delay to add between input and output (ms)");

        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);
}

void
jdelay_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:        input port\n"
                  << " * out:       output port with delayed signal\n"
                  << std::endl;
}
