/*
 * Converts a signal to amplitude-modulated noise
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2021 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <csignal>
#include <boost/filesystem.hpp>

#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/logging.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jamnoise"

using namespace jill;
using std::string;
using sample_ringbuffer = dsp::ringbuffer<sample_t>;

class jamnoise_options : public program_options {

public:
        jamnoise_options(string const &program_name);

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

}; // jstim_options


static jamnoise_options options(PROGRAM_NAME);
static std::unique_ptr<jack_client> client;
jack_port_t *port_in, *port_out;
static int ret = EXIT_SUCCESS;
static int running = 1;


static const nframes_t n_hilbert = 128;
static const double hilbert_filt[n_hilbert] = {
        1.35574586e-01,  4.60859408e-04,  8.92437318e-03,  5.07861048e-04,
        9.24435788e-03,  5.69220879e-04,  9.59325502e-03,  6.38987738e-04,
        9.95084658e-03,  7.11308425e-04,  1.03098408e-02,  7.76308935e-04,
        1.06316260e-02,  8.73478813e-04,  1.08765812e-02,  1.24267701e-03,
        1.14506282e-02,  1.28723296e-03,  1.18104287e-02,  1.51398720e-03,
        1.22435080e-02,  1.76896695e-03,  1.27081033e-02,  2.06556124e-03,
        1.32061648e-02,  2.41620424e-03,  1.37394473e-02,  2.78296306e-03,
        1.43181333e-02,  3.15341648e-03,  1.49623271e-02,  3.71955064e-03,
        1.56544374e-02,  4.27884480e-03,  1.64303935e-02,  4.94616307e-03,
        1.73032540e-02,  5.72800738e-03,  1.82967224e-02,  6.64937101e-03,
        1.94585877e-02,  7.76544348e-03,  2.07954898e-02,  9.11355352e-03,
        2.23619322e-02,  1.07345356e-02,  2.43249447e-02,  1.27965733e-02,
        2.67367864e-02,  1.54306873e-02,  2.98413331e-02,  1.89222113e-02,
        3.39986152e-02,  2.37548174e-02,  3.98622674e-02,  3.08852884e-02,
        4.88566809e-02,  4.24033772e-02,  6.43443755e-02,  6.41641855e-02,
        9.74280062e-02,  1.20767245e-01,  2.18722724e-01,  6.30080414e-01,
       -6.30080414e-01, -2.18722724e-01, -1.20767245e-01, -9.74280062e-02,
       -6.41641855e-02, -6.43443755e-02, -4.24033772e-02, -4.88566809e-02,
       -3.08852884e-02, -3.98622674e-02, -2.37548174e-02, -3.39986152e-02,
       -1.89222113e-02, -2.98413331e-02, -1.54306873e-02, -2.67367864e-02,
       -1.27965733e-02, -2.43249447e-02, -1.07345356e-02, -2.23619322e-02,
       -9.11355352e-03, -2.07954898e-02, -7.76544348e-03, -1.94585877e-02,
       -6.64937101e-03, -1.82967224e-02, -5.72800738e-03, -1.73032540e-02,
       -4.94616307e-03, -1.64303935e-02, -4.27884480e-03, -1.56544374e-02,
       -3.71955064e-03, -1.49623271e-02, -3.15341648e-03, -1.43181333e-02,
       -2.78296306e-03, -1.37394473e-02, -2.41620424e-03, -1.32061648e-02,
       -2.06556124e-03, -1.27081033e-02, -1.76896695e-03, -1.22435080e-02,
       -1.51398720e-03, -1.18104287e-02, -1.28723296e-03, -1.14506282e-02,
       -1.24267701e-03, -1.08765812e-02, -8.73478813e-04, -1.06316260e-02,
       -7.76308935e-04, -1.03098408e-02, -7.11308425e-04, -9.95084658e-03,
       -6.38987738e-04, -9.59325502e-03, -5.69220879e-04, -9.24435788e-03,
       -5.07861048e-04, -8.92437318e-03, -4.60859408e-04, -1.35574586e-01 };
static sample_ringbuffer hilbert_rb(n_hilbert * 2);
static sample_ringbuffer delay_rb(n_hilbert);
static const nframes_t n_iir = 5;
static const double lp_filt_b[] = { 8.03606182e-10, 3.21442473e-09, 4.82163709e-09, 3.21442473e-09, 8.03606182e-10 };
static const double lp_filt_a[] = { -3.97207701,  5.91662022, -3.91700624,  0.97246304 };
static double shift_in[n_iir] = {};
static double shift_out[n_iir - 1] = {};
// static dsp::ringbuffer<double> iir_in_rb(n_iir * 2);
// static dsp::ringbuffer<double> iir_out_rb(n_iir * 2);


/*
 * The process callback extracts the envelope of the incoming signal by
 * performing a Hilbert transform and then low-pass filtering with a 4th-order
 * Butterworth.
 */
int
process(jack_client *client, nframes_t nframes, nframes_t)
{
        // buffers for the IIR lowpass filter. Samples are shifted to higher indices
        sample_t *in = client->samples(port_in, nframes);
        sample_t *out = client->samples(port_out, nframes);

        for (nframes_t i = 0; i < nframes; ++i) {
                sample_t sample = in[i];
                // push sample to hilbert transform and delay ringbuffers
                hilbert_rb.push(sample);
                delay_rb.push(sample);
                // convolution
                {
                        double conv = 0.0;
                        sample_t const * rb = hilbert_rb.buffer() + hilbert_rb.read_offset();
                        for (nframes_t j = 0; j < n_hilbert; ++j) {
                                conv += rb[j] * hilbert_filt[j];
                        }
                        hilbert_rb.pop(nullptr, 1);
                        // envelope
                        sample_t delayed = delay_rb.pop();
                        double envelope = sqrt(conv * conv + delayed * delayed);
                        // shift input samples
                        for (nframes_t j = n_iir - 1; j > 0; --j) {
                                shift_in[j] = shift_in[j - 1];
                        }
                        shift_in[0] = envelope;
                        out[i] = shift_in[n_iir - 1];
                }
                // lowpass filter
                {
                        double conv = 0.0;
                        // FIR
                        for (nframes_t j = 0; j < n_iir; ++j) {
                                conv += shift_in[j] * lp_filt_b[j];
                        }
                        // IIR
                        for (nframes_t j = 0; j < n_iir - 1; ++j) {
                                conv -= shift_out[j] * lp_filt_a[j];
                        }
                        // shift output samples
                        for (nframes_t j = n_iir - 2; j > 0; --j) {
                                shift_out[j] = shift_out[j - 1];
                        }
                        //shift_out[0] = conv;

                }
        }

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
        running = 0;
}

void
signal_handler(int sig)
{
        ret = sig;
        running = 0;
}

int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                client.reset(new jack_client(options.client_name, options.server_name));
                LOG << "initializing hilbert transform (" << n_hilbert << " points)";
                hilbert_rb.push(nullptr, n_hilbert - 1);
                LOG << "initializing delay ringbuffer (" << n_hilbert / 2 << " points)";
                delay_rb.push(nullptr, n_hilbert / 2 - 1);
                // LOG << "initializing lowpass IIR filter ringbuffers (" << n_iir << " points)";
                // iir_in_rb.push(nullptr, n_iir - 1);
                // iir_out_rb.push(nullptr, n_iir - 1);

                port_in = client->register_port("in",JACK_DEFAULT_AUDIO_TYPE,
                                                JackPortIsInput, 0);
                port_out = client->register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client->set_shutdown_callback(jack_shutdown);
                client->set_xrun_callback(jack_xrun);
                client->set_process_callback(process);
                client->activate();

                client->connect_ports(options.input_ports.begin(), options.input_ports.end(), "in");
                client->connect_ports("out", options.output_ports.begin(), options.output_ports.end());

                while (running) {
                        usleep(100000);
                }

                client->deactivate();
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


jamnoise_options::jamnoise_options(string const &program_name)
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
        // po::options_description opts("Delay options");
        // opts.add_options()
        //         ("delay,d",   po::value<float>(&delay_msec)->default_value(10),
        //          "delay to add between input and output (ms)");

        cmd_opts.add(jillopts); //.add(opts);
        visible_opts.add(jillopts); //.add(opts);
}

void
jamnoise_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options]\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * in:        input port\n"
                  << " * out:       output port with delayed signal\n"
                  << std::endl;
}
