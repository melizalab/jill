/*
 * A simple module that transforms events into clicks.
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <random>
#include <csignal>
#include <atomic>
#include <boost/algorithm/string.hpp>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/midi.hh"
#include "jill/program_options.hh"
#include "jill/dsp/ringbuffer.hh"

#define PROGRAM_NAME "jclicker"

using namespace jill;
using std::string;
using stringvec = std::vector<string>;
using sample_ringbuffer = dsp::ringbuffer<sample_t>;

class jclicker_options : public program_options {

public:
        jclicker_options(string const &program_name);

        /** The server name */
        string server_name;
        /** The client name (used in internal JACK representations) */
        string client_name;

        /** probability of emitting a pulse */
        float pulse_prob;
        /** user-defined pulses (post-processed) */
        stringvec pulses;

protected:

        void print_usage() override;

}; // jclicker_options


struct pulse_type {
        /** the shape of the pulse */
        enum { Positive, Negative, Biphasic} shape;
        /** the type of midi message that will trigger the pulse */
        midi::data_type status;
        /** delay between the triggering event and pulse onset, in samples */
        nframes_t delay;
        /** duration of the pulse, in samples */
        nframes_t duration;
};

std::ostream& operator << (std::ostream &os, const pulse_type &p) {
        os << midi::status_type(p.status) << ": ";
        switch(p.shape) {
        case pulse_type::Positive: os << "positive"; break;
        case pulse_type::Negative: os << "negative"; break;
        case pulse_type::Biphasic: os << "biphasic"; break;
        }
        os << ", " << p.duration << " samples";
        if (p.delay > 0)
                os << ", delayed " << p.delay << " samples";
        return os;
}

static jclicker_options options(PROGRAM_NAME);
jack_port_t *port_in, *port_out;
std::vector<pulse_type> pulses;
static nframes_t max_lookahead = 0;
// ringbuffer acts as a backing buffer so that pulses can span the end of the current period.
std::unique_ptr<sample_ringbuffer> ringbuf;
std::atomic<int> ret(EXIT_SUCCESS);
std::atomic<bool> running(true);

static std::random_device rd;  // Will be used to obtain a seed for the random number engine
static std::mt19937 p_gen(rd()); // Standard mersenne_twister_engine seeded with rd()
static std::uniform_real_distribution<> p_dis(0.0, 1.0);

int
process(jack_client *client, nframes_t nframes, nframes_t time)
{
        void *in = client->events(port_in, nframes);
        sample_t *out = client->samples(port_out, nframes);

        // write pointer should be one period + max_lookahead ahead of read pointer
        assert (ringbuf->read_space() == nframes + max_lookahead);
        assert (ringbuf->write_space() >= nframes);
        // zero out the back of the buffer
        ringbuf->push(nullptr, nframes);
        // write the pulses to the front using the read pointer
        sample_t * buf = ringbuf->buffer() + ringbuf->read_offset();

        jack_midi_event_t event;
        nframes_t nevents = jack_midi_get_event_count(in);
        for (nframes_t i = 0; i < nevents; ++i) {
                jack_midi_event_get(&event, in, i);
                if (event.size < 1) continue;
                float prob = p_dis(p_gen);
                DBG << time + event.time << ": " << static_cast<midi::status_type>(event.buffer[0]);
                DBG << " - skip if " << prob << ">" << options.pulse_prob;
                if (prob > options.pulse_prob) continue;
                for (const auto &pulse : pulses) {
                        if (pulse.status != event.buffer[0]) continue;
                        DBG << " - adding pulse at " << pulse.delay;
                        sample_t * pulse_buf = buf + event.time + pulse.delay;
                        switch (pulse.shape) {
                        case pulse_type::Positive:
                                std::fill(pulse_buf, pulse_buf + pulse.duration, 1.0f);
                                break;
                        case pulse_type::Negative:
                                std::fill(pulse_buf, pulse_buf + pulse.duration, -1.0f);
                                break;
                        case pulse_type::Biphasic:
                                nframes_t half_dur = pulse.duration / 2;
                                std::fill(pulse_buf, pulse_buf + half_dur, 1.0f);
                                std::fill(pulse_buf + half_dur, pulse_buf + pulse.duration, -1.0f);
                        }
                }
        }
        // copy the front of the buffer into the output and advance the read pointer
        ringbuf->pop(out, nframes);

        return 0;
}

int
jack_bufsize(jack_client *, nframes_t nframes)
{
        // Do we need to reset the pointers?
        ringbuf->resize(nframes * 3 + max_lookahead);
        DBG << "jack period size changed; ringbuffer resized to " << ringbuf->size();
        return 0;
}

/** handle server shutdowns */
void
jack_shutdown(jack_status_t code, char const *)
{
        ret = -1;
        running = false;
}

/** handle POSIX signals */
void
signal_handler(int sig)
{
        ret = sig;
        running = false;
}

static
void parse_pulses(stringvec const & pulse_defs, nframes_t sampling_rate) {
        LOG << "parsing pulse specifications: ";
        float dt = 1.0 / sampling_rate;
        for (const auto &it : pulse_defs) {
                stringvec words;
                boost::split(words, it, [](char c) { return c==',';});
                if (words.size() != 3 && words.size() != 4) {
                        throw std::invalid_argument(
                                "invalid pulse configuration (must be condition,shape,duration[,delay])");
                }
                pulse_type pulse;
                // parse first token as hex - std::invalid_argument on failure
                pulse.status = std::stoul(words[0], 0, 16);
                // parse second token by string matching
                if (boost::iequals(words[1], "positive"))
                        pulse.shape = pulse_type::Positive;
                else if (boost::iequals(words[1], "negative"))
                        pulse.shape = pulse_type::Negative;
                else if (boost::iequals(words[1], "biphasic"))
                        pulse.shape = pulse_type::Biphasic;
                else
                        throw std::invalid_argument("pulse shape must be 'positive', 'negative', or 'biphasic'");
                // parse third token as a float
                float duration_ms = std::stof(words[2], 0);
                if (duration_ms < dt) {
                        throw std::invalid_argument("duration must be positive and at least one sample");
                }
                pulse.duration = 0.001 * duration_ms * sampling_rate;
                // parse optional fourth token as a float (delay in ms)
                float delay_ms = (words.size() == 4) ? std::stof(words[3], 0) : 0.0f;
                if (delay_ms < 0.0) {
                        throw std::invalid_argument("delay must be positive ");
                }
                pulse.delay = 0.001 * delay_ms * sampling_rate;

                LOG << "  " << pulse;
                pulses.push_back(pulse);
                max_lookahead = std::max(max_lookahead, pulse.delay + pulse.duration);
        }
}


int
main(int argc, char **argv)
{
        using namespace std;
        try {
                // parse options
                options.parse(argc,argv);
                if (options.pulses.size() == 0)
                        throw std::invalid_argument("must define at least one pulse");

                // start client
                auto client = jack_client(options.client_name, options.server_name);
                parse_pulses(options.pulses, client.sampling_rate());

                // initialize ringbuffer; sized to hold one period of current
                // output plus enough lookahead for the longest configured
                // delay+duration
                nframes_t ringbuf_size = client.buffer_size() * 3 + max_lookahead;
                ringbuf = std::make_unique<sample_ringbuffer>(ringbuf_size);
                // advance the write pointer one period plus lookahead margin
                ringbuf->push(nullptr, client.buffer_size() + max_lookahead);
                DBG << "initialized ringbuffer with " << ringbuf->size() << " samples";

                // register ports
                port_in = client.register_port("in",JACK_DEFAULT_MIDI_TYPE,
                                               JackPortIsInput, 0);
                port_out = client.register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                JackPortIsOutput, 0);

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                // register jack callbacks
                client.set_shutdown_callback(jack_shutdown);
                client.set_buffer_size_callback(jack_bufsize);
                client.set_process_callback(process);

                // activate client
                client.activate();

                // connect ports
                if (options.count("in")) {
                        auto const & portlist = options.vmap["in"].as<stringvec>();
                        client.connect_ports(portlist.begin(), portlist.end(), "in");
                }
                if (options.count("out")) {
                        auto const & portlist = options.vmap["out"].as<stringvec>();
                        client.connect_ports("out", portlist.begin(), portlist.end());
                }

                while (running) {
                        usleep(100000);
                }

                client.deactivate();
                return ret;
        }

        catch (Exit const &e) {
                return e.status();
        }
        catch (std::exception const &e) {
                LOG  << "ERROR: " << e.what();
                return EXIT_FAILURE;
        }

}

/** configure commandline options */
jclicker_options::jclicker_options(string const &program_name)
        : program_options(program_name)
{

        // this section is for general JILL options. try to maintain consistency
        // with other modules
        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value<string>(&server_name), "connect to specific jack server")
                ("name,n",    po::value<string>(&client_name)->default_value(_program_name),
                 "set client name")
                ("in,i",      po::value<stringvec>(), "add connection to input port")
                ("out,o",     po::value<stringvec>(), "add connection to output port");
        po::options_description opts("Module options");
        opts.add_options()
                ("prob,p",
                 po::value(&pulse_prob)->default_value(1.0),
                 "probability of emitting a pulse (0-1)");
        cmd_opts.add(jillopts).add(opts);
        visible_opts.add(jillopts).add(opts);

        // add section(s) for module-specific options
        cmd_opts.add_options()
                ("pulse",
                 po::value<stringvec>(&pulses)->multitoken(),
                 "defines a pulse: condition,shape,duration");
        pos_opts.add("pulse", -1);
}

/** provide the user with some information about the ports */
void
jclicker_options::print_usage()
{
        std::cout << _program_name << ": generate audible clicks for events\n\n"
                  << "Usage: " << _program_name << " [options] [pulse1] [pulse2] ...\n"
                  << visible_opts << std::endl
                  << "Pulse specification: condition,shape,duration \n"
                  << " - condition: the midi event code (0x00: stim on, 0x01 acq on, 0x10 stim off, 0x11 acq off)\n"
                  << " - shape: {positive,negative,biphasic}\n"
                  << " - duration: total duration of the click, in ms\n"
                  << " - delay: optional delay between event and pulse onset, in ms (default 0)\n\n"
                  << "Ports:\n"
                  << " * in:        input event port\n"
                  << " * out:       output audio port\n"
                  << std::endl;
}
