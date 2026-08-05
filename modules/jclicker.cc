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
#include "jill/util/event_randomizer.hh"
#include "jill/dsp/pulse.hh"

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

        /** user-defined pulses (post-processed) */
        stringvec pulses;

protected:

        void print_usage() override;

}; // jclicker_options


struct pulse_type {
        /** the shape of the pulse */
        dsp::pulse_shape shape;
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
        case dsp::pulse_shape::positive: os << "positive"; break;
        case dsp::pulse_shape::negative: os << "negative"; break;
        case dsp::pulse_shape::biphasic: os << "biphasic"; break;
        }
        os << ", " << p.duration << " samples";
        if (p.delay > 0)
                os << ", delayed " << p.delay << " samples";
        return os;
}

static jclicker_options options(PROGRAM_NAME);
jack_port_t *port_in, *port_out;
std::vector<pulse_type> pulses;
std::set<midi::data_type> event_types;
static nframes_t max_lookahead = 0;
// ringbuffer acts as a backing buffer so that pulses can span the end of the current period.
std::unique_ptr<sample_ringbuffer> ringbuf;
std::atomic<int> ret(EXIT_SUCCESS);
std::atomic<bool> running(true);
// used to track stimuli so that probability is on a per-stimulus basis
std::unique_ptr<jill::util::event_randomizer> randomizer;

static std::random_device rd;  // Will be used to obtain a seed for the random number engine
static std::mt19937 p_gen(rd()); // Standard mersenne_twister_engine seeded with rd()
static std::uniform_real_distribution<> p_dis(0.0, 1.0);

int
process(jack_client *client, nframes_t nframes, nframes_t time) JILL_RT
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
                auto status = static_cast<midi::status_type>(event.buffer[0]);
                /* The label is used as a pointer and a length, not copied into
                 * a std::string: encode() allocates, and this runs in the
                 * realtime thread. It stays behind DBG, which compiles out of
                 * release builds, so the allocation only happens in a build
                 * that is already not realtime safe. */
                char const * stimulus = reinterpret_cast<char const *>(event.buffer) + 1;
                std::size_t stimulus_len = event.size - 1;
                DBG << status << " @ " << time + event.time << ": "
                    << status.encode(stimulus, stimulus_len);
                if (event_types.count(status.value()) == 0) {
                        DBG << " - no pulses defined for this status";
                        continue;
                }
                if (!randomizer->present(stimulus, stimulus_len)) {
                        DBG << " - skipped due to randomizer";
                        continue;
                }
                for (const auto &pulse : pulses) {
                        if (pulse.status != event.buffer[0]) continue;
                        DBG << " - adding pulse at " << pulse.delay;
                        sample_t * pulse_buf = buf + event.time + pulse.delay;
                        dsp::render_pulse(pulse_buf, pulse.shape, pulse.duration);
                }
        }
        // copy the front of the buffer into the output and advance the read pointer
        ringbuf->pop(out, nframes);

        return 0;
}

/** Resize the buffer. May reallocate memory, losing upcoming pulses */
int
jack_bufsize(jack_client *, nframes_t nframes)
{
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
                        pulse.shape = dsp::pulse_shape::positive;
                else if (boost::iequals(words[1], "negative"))
                        pulse.shape = dsp::pulse_shape::negative;
                else if (boost::iequals(words[1], "biphasic"))
                        pulse.shape = dsp::pulse_shape::biphasic;
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
                event_types.insert(pulse.status);
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

                // configure the randomizer - use some nice defaults
                randomizer = std::make_unique<jill::util::event_randomizer>(
                        options.vmap["prob"].as<float>(),
                        8.0,
                        1024,
                        options.vmap["max-stimuli"].as<std::size_t>());

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
                activated_client active(client);

                // connect ports
                if (options.count("in")) {
                        auto const & portlist = options.vmap["in"].as<stringvec>();
                        active.connect_ports(portlist.begin(), portlist.end(), "in");
                }
                if (options.count("out")) {
                        auto const & portlist = options.vmap["out"].as<stringvec>();
                        active.connect_ports("out", portlist.begin(), portlist.end());
                }

                /* The realtime thread cannot log, so it raises flags and this
                 * loop reports them. Latched, so each is said once. */
                bool said_overflow = false, said_truncation = false;
                while (running) {
                        usleep(100000);
                        if (randomizer->overflowed() && !said_overflow) {
                                said_overflow = true;
                                LOG << "WARNING: more than " << randomizer->capacity()
                                    << " distinct stimuli; the extra ones are being"
                                       " presented at the target probability without"
                                       " balancing. Raise --max-stimuli.";
                        }
                        if (randomizer->truncated() && !said_truncation) {
                                said_truncation = true;
                                LOG << "WARNING: a stimulus name was longer than "
                                    << jill::util::event_randomizer::max_name
                                    << " characters and was truncated; names sharing"
                                       " that prefix and length count as one.";
                        }
                }

                if (randomizer) {
			std::cout << "Final event counts: " << std::endl;
                        for (auto const & it : *randomizer) {
				std::cout << " - " << it.name << ": " << it.events << "/" << it.presentations << std::endl;
                        }
                }
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
                 po::value<float>()->default_value(1.0),
                 "per-stimulus probability of emitting a pulse (0-1)")
                ("max-stimuli",
                 po::value<std::size_t>()->default_value(
                         jill::util::event_randomizer::default_capacity),
                 "how many distinct stimuli to balance across. The table is "
                 "allocated at startup and never grows, because growing it "
                 "would mean allocating in the realtime thread.");
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
                  << " - delay: delay between event and pulse onset, in ms (optional; default 0)\n\n"
                  << "Ports:\n"
                  << " * in:        input event port\n"
                  << " * out:       output audio port\n"
                  << std::endl;
}
