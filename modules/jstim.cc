/*
 * Provides stimulus playback
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 */
#include <iostream>
#include <atomic>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <random>
#include <boost/filesystem.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/optional.hpp>

#include "jill/logging.hh"
#include "jill/jack_client.hh"
#include "jill/program_options.hh"
#include "jill/midi.hh"
#include "jill/file/stimfile.hh"
#include "jill/util/readahead_stimqueue.hh"
#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/playback_timing.hh"

#define PROGRAM_NAME "jstim"

using namespace jill;
using std::string;

class jstim_options : public program_options {

public:
        jstim_options(string const &program_name);

        string server_name;
        string client_name;

        /** Ports to connect to */
        std::vector<string> output_ports;
        std::vector<string> syncout_ports;
        std::vector<string> trigin_ports;

        std::vector<string> stimuli; // this is postprocessed

        size_t nreps;           // default set by reps flag
        float min_gap_sec;      // min gap btw sound, in sec
        float min_interval_sec; // min interval btw starts, in sec
        boost::optional<float> pretrigger_interval_sec;
        boost::optional<float> posttrigger_interval_sec;
        float condition_prob;
        nframes_t min_gap;
        nframes_t min_interval;
        boost::optional<nframes_t> pretrigger_interval;
        boost::optional<nframes_t> posttrigger_interval;

protected:

        void print_usage() override;
        void process_options() override;

}; // jstim_options


jstim_options options(PROGRAM_NAME);
// Holds (and owns) all the stimuli
boost::ptr_vector<stimulus_t> _stimuli;
// The sequence of trials being played. May be shuffled, and holds one entry
// per repetition -- several of which alias the same stimulus, which is why the
// condition flag lives on the entry rather than on the stimulus.
std::vector<util::trial> _stimlist;
// The queue of stimuli. Handles the actual file reads and any resampling
// needed in a background thread. Must be declared after _stimuli so that the
// thread can continue to dereference the objects owned by _stimuli.
std::unique_ptr<util::readahead_stimqueue> stim_queue;
jack_port_t *port_out, *port_syncout, *port_trigin;
std::atomic<int> xruns(0);                  // xrun counter
std::atomic<nframes_t> last_stop(0);        // time when last stimulus ended
std::atomic<int> ret(EXIT_SUCCESS);
// cleared by a signal or by the server going away; main stops the queue
std::atomic<bool> running(true);

/**
 * The realtime process loop for jstim. The logic is complicated. The process
 * can be in two states:
 *
 * 1. currently playing stimulus. copy data from the stimulus buffer to the
 * output buffer, and update the static variable that tracks position in the
 * stimulus buffer. If an xrun was flagged, terminate the stimulus at the
 * beginning of the period. If the stimulus ends before the end of the period,
 * save the stop time to another static variable.
 *
 * 2. not playing stimulus. check for (a) trigger event, if we're in triggered
 * mode or (b) satisfaction of one of the minimum time constraints (interval =
 * time since last start; gap = time since last stop). Checking (b) is
 * complicated by the fact that the frame counter is unsigned and may overflow.
 * If either condition is true, copy data from the stimulus buffer (if
 * available) into the output buffer, starting with the onset time. Advance the
 * stimulus buffer tracking variable to reflect the number of samples played.
 *
 */
int
process(jack_client *client, nframes_t nframes, nframes_t time) JILL_RT
{
        // NB static variables are initialized to 0
        static nframes_t stim_offset; // current position in stimulus buffer
        static nframes_t last_start;  // last stimulus start time
        // last stimulus stop is a global atomic so that the main loop can
        // initialize it. Otherwise the pre-stimulus trigger doesn't work on the
        // first stimulus.

        nframes_t period_offset;      // the offset in the period to start copying

        void * sync = client->events(port_syncout, nframes);
        sample_t * out = client->samples(port_out, nframes);
        // zero the output buffer - somewhat inefficient but safer
        memset(out, 0, nframes * sizeof(sample_t));

        // the currently playing trial (or nullptr), and the one before it
        util::trial const * current = stim_queue->head();
        util::trial const * last = stim_queue->previous();
        jill::stimulus_t const * stim = (current) ? current->stim : nullptr;
        jill::stimulus_t const * last_stim = (last) ? last->stim : nullptr;

        // handle xruns
        if (xruns) {
                // playing stimuli are truncated
                if (stim_offset > 0 && stim) {
                        stim_offset = stim->nframes();
                }
                // reset timers because running time has changed
                last_start = last_stop = time;
                // indicate we've handled the xrun
                xruns.fetch_add(-1);
        }

        // a bunch of annoying unsigned arithmetic here, but it seems to work.
        // time since last start and stop (relative to start of the period).
        // This difference is correct even if sample counter has overflowed
        // because time >= lastX
        const nframes_t dstart = time - last_start;
        const nframes_t dstop = time - last_stop;

        // check if we need to emit a posttrigger event
        if (options.posttrigger_interval && last_stim) {
                const auto otrig = dsp::posttrigger_offset(
                        dstop, *options.posttrigger_interval, nframes);
                if (otrig) {
                        midi::write_message(sync, *otrig,
                                            midi::status_type(midi::status_type::stim_off,
                                                              midi::channel::trial),
                                            last_stim->name());
                        if (last->condition) {
                                midi::write_message(sync, *otrig,
                                                    midi::status_type(midi::status_type::stim_off,
                                                                      midi::channel::condition),
                                                    last_stim->name());
                        }
                        DBG << "sent posttrigger: time=" << time + *otrig
                            << ", stim=" << last_stim->name();
                }
        }

        // if no stimulus queued, we are done
        if (!stim) return 0;

        // am I playing a stimulus?
        if (stim_offset > 0) {
                period_offset = 0;
        }
        // is there an external trigger?
        else if (port_trigin) {
                void * midi_buffer = client->events(port_trigin, nframes);
                period_offset = midi::find_trigger(midi_buffer, true);
                if (period_offset > nframes) return 0; // no trigger
                last_start = time + period_offset;
                midi::write_message(sync, period_offset, midi::status_type::stim_on, stim->name());
                // no pretrigger in triggered mode, so the condition window
                // opens with the stimulus
                if (current->condition) {
                        midi::write_message(sync, period_offset,
                                            midi::status_type(midi::status_type::stim_on,
                                                              midi::channel::condition),
                                            stim->name());
                }
                DBG << "playback triggered: time=" << last_start << ", stim=" << stim->name();
        }
        // has enough time elapsed since the last stim?
        else {
                period_offset = dsp::next_onset_offset(dstart, dstop,
                                                       options.min_interval, options.min_gap);
                // check if we need to emit a pretrigger event
                if (options.pretrigger_interval) {
                        const auto otrig = dsp::pretrigger_offset(
                                period_offset, *options.pretrigger_interval, nframes);
                        if (otrig) {
                                midi::write_message(sync, *otrig,
                                                    midi::status_type(midi::status_type::stim_on,
                                                                      midi::channel::trial),
                                                    stim->name());
                                if (current->condition) {
                                        midi::write_message(sync, *otrig,
                                                            midi::status_type(midi::status_type::stim_on,
                                                                              midi::channel::condition),
                                                            stim->name());
                                }
                                DBG << "sent pretrigger: time=" << time + *otrig
                                    << ", stim=" << stim->name();
                        }
                }

                if (period_offset >= nframes) return 0; // not time yet
                last_start = time + period_offset;
                midi::write_message(sync, period_offset, midi::status_type::stim_on, stim->name());
                /* The condition window brackets the trial, so when there is a
                 * pretrigger it opened there and this would double it. Without
                 * one there is no wider window to ride on, so it opens here. */
                if (current->condition && !options.pretrigger_interval) {
                        midi::write_message(sync, period_offset,
                                            midi::status_type(midi::status_type::stim_on,
                                                              midi::channel::condition),
                                            stim->name());
                }
                DBG << "playback started: time=" << last_start << ", stim=" << stim->name();
        }
        // sanity check - will be optimized out
        assert(period_offset < nframes);

        // copy samples, if there are any
        nframes_t nsamples = dsp::samples_to_copy(stim->nframes(), stim_offset,
                                                  nframes, period_offset);
        // DBG << "stim_offset=" << stim_offset << ", period_offset="
        //     << period_offset << ", nsamples=" << nsamples;
        if (nsamples > 0) {
                memcpy(out + period_offset,
                       stim->buffer() + stim_offset,
                       nsamples * sizeof(sample_t));
                stim_offset += nsamples;
        }
        // did the stimulus end?
        if (stim_offset >= stim->nframes()) {
                const bool was_condition = current->condition;
                stim_queue->release();
                last_stop = time + period_offset + nsamples;
                midi::write_message(sync, period_offset + nsamples,
                                    midi::status_type::stim_off, stim->name());
                // as at onset: the posttrigger closes the condition window when
                // there is one, otherwise it closes with the stimulus
                if (was_condition && !options.posttrigger_interval) {
                        midi::write_message(sync, period_offset + nsamples,
                                            midi::status_type(midi::status_type::stim_off,
                                                              midi::channel::condition),
                                            stim->name());
                }
                DBG << "playback ended: time=" << last_stop << ", stim=" << stim->name();
                stim_offset = 0;
        }

        return 0;
}

int
jack_xrun(jack_client *client, float delay)
{
        xruns.fetch_add(1);
        return 0;
}

int
jack_bufsize(jack_client *client, nframes_t nframes)
{
        /* Registered after activation, so this only ever runs on a real change,
         * never to report the initial size. JACK delivers it on its
         * notification thread with the engine stopped, so logging here is fine;
         * this is not the realtime thread.
         *
         * The xrun counter is how the process thread is told the stream broke.
         * It truncates the stimulus at the next period boundary and emits the
         * stim_off marker there, so the recording carries the length that was
         * actually played rather than the length that was intended. One trial
         * is spoiled and can be dropped from the analysis. */
        LOG << "WARNING: JACK period size changed to " << nframes
            << " frames; the audio stream was interrupted and any stimulus in"
               " progress has been truncated";
        xruns.fetch_add(1);
        return 0;
}

void
jack_shutdown(jack_status_t code, char const *)
{
        xruns.fetch_add(1);
        ret = -1;
        running = false;
}

void
signal_handler(int sig)
{
        /*
	 * Only touch lock-free atomics to stay within bounds of what a signal
         * handler can safely do. This used to call stim_queue->stop(), which
         * takes the queue's mutex: a signal delivered while the worker thread
         * held that mutex deadlocked the process, since the handler runs on the
         * thread it interrupted and that thread cannot release it. main() does
         * the stopping now. The xruns variable is incremented to tell the
         * realtime thread to cut the stimulus short at the start of the next
         * period rather than playing it out.
	 */
        xruns.fetch_add(1);
        ret = sig;
        running = false;
}


/**
 * Is this argument a repeat count rather than a filename?
 *
 * The whole token has to be digits. sscanf("%zu") would happily read 5 out of
 * "5.wav" and report success, which would eat a legitimate stimulus.
 */
static bool
parse_count(string const & s, size_t & out)
{
        if (s.empty()) return false;
        char * end = nullptr;
        const unsigned long value = std::strtoul(s.c_str(), &end, 10);
        if (*end != '\0') return false;
        out = value;
        return true;
}

/* parse the list of stimuli */
static void
init_stimset(std::vector<string> const & stims, size_t const default_nreps,
             float condition_prob, std::mt19937 & rng)
{
        using namespace boost::filesystem;

        size_t marked = 0;
        for (size_t i = 0; i < stims.size(); ++i) {
                path p(stims[i]);
                /* A bare count following a filename is that stimulus's number
                 * of repeats, and is consumed here so it is not opened as a
                 * file. Both halves of this used to be inverted: a count was
                 * read but left in place, so it became the next "filename",
                 * and a following *filename* was skipped instead, so
                 * `jstim a.wav b.wav` silently played only a.wav. */
                size_t nreps = default_nreps;
                if ((i + 1) < stims.size() && parse_count(stims[i + 1], nreps)) {
                        i += 1;
                }
                try {
                        jill::stimulus_t *stim = new file::stimfile(p.string());
                        _stimuli.push_back(stim);
                        /* Assign the condition by counting rather than by
                         * drawing per trial. Every stimulus gets exactly
                         * round(p * nreps) of its repetitions marked, so the
                         * proportion is exact instead of approached, and which
                         * repetitions they are is still random -- the flags are
                         * shuffled within this stimulus's block. Doing it here
                         * rather than after the whole list is built is what
                         * keeps the balance per stimulus rather than overall.
                         *
                         * This is also why it does not depend on --shuffle:
                         * that decides presentation order, which is a separate
                         * question from which trials carry the manipulation. */
                        const size_t want = std::lround(condition_prob * nreps);
                        std::vector<bool> flags(nreps, false);
                        std::fill_n(flags.begin(), want, true);
                        std::shuffle(flags.begin(), flags.end(), rng);
                        for (size_t j = 0; j < nreps; ++j) {
                                _stimlist.push_back(util::trial{stim, flags[j]});
                        }
                        marked += want;
                }
                catch (jill::FileError const & e) {
                        LOG << "invalid stimulus " << p << ": " << e.what();
                }
        }
        LOG << "loaded " << _stimuli.size() << " stimuli, " << _stimlist.size()
            << " trials";
        if (condition_prob > 0.0) {
                LOG << marked << " of " << _stimlist.size()
                    << " trials are in the condition (channel "
                    << int(midi::channel::condition) << ")";
        }
}


int
main(int argc, char **argv)
{
        using namespace std;
        try {
                options.parse(argc,argv);
                auto client = jack_client(options.client_name, options.server_name);

                nframes_t sampling_rate = client.sampling_rate();
                if (options.count("trig") == 0) {
                        options.min_interval = options.min_interval_sec * sampling_rate;
                        options.min_gap = options.min_gap_sec * sampling_rate;
                        LOG << "minimum gap: " << options.min_gap_sec << "s ("
                                      << options.min_gap << " samples)";
                        LOG << "minimum interval: " << options.min_interval_sec << "s ("
                                      << options.min_interval << " samples)";
                }
                if (options.pretrigger_interval_sec) {
                        options.pretrigger_interval = *options.pretrigger_interval_sec * sampling_rate;
                        LOG << "pre-trigger interval: " << *options.pretrigger_interval_sec << "s ("
                            << *options.pretrigger_interval << " samples)";
                }
                if (options.posttrigger_interval_sec) {
                        options.posttrigger_interval = *options.posttrigger_interval_sec * sampling_rate;
                        LOG << "post-trigger interval: " << *options.posttrigger_interval_sec << "s ("
                            << *options.posttrigger_interval << " samples)";
                }
                if (options.stimuli.size() == 0) {
                        LOG << "no stimuli; quitting";
                        throw Exit(0);
                }

                /* stimulus queue */
                DBG << "loading " << options.stimuli.size() << " stimuli";
                if (options.condition_prob < 0.0 || options.condition_prob > 1.0) {
                        throw std::invalid_argument("--condition-prob must be between 0 and 1");
                }
                std::mt19937 rng(std::random_device{}());
                init_stimset(options.stimuli, options.nreps, options.condition_prob, rng);
                if (options.count("shuffle")) {
                        LOG << "shuffled stimuli";
                        shuffle(_stimlist.begin(), _stimlist.end(), rng);
                }
                stim_queue.reset(new util::readahead_stimqueue(_stimlist.begin(), _stimlist.end(),
                                                          client.sampling_rate(),
                                                          options.count("loop")));


                port_out = client.register_port("out", JACK_DEFAULT_AUDIO_TYPE,
                                                 JackPortIsOutput | JackPortIsTerminal, 0);
                port_syncout = client.register_port("sync_out", JACK_DEFAULT_MIDI_TYPE,
                                                     JackPortIsOutput | JackPortIsTerminal, 0);
                if (options.count("trig")) {
                        LOG << "triggering playback from trig_in";
                        port_trigin = client.register_port("trig_in", JACK_DEFAULT_MIDI_TYPE,
                                                            JackPortIsInput | JackPortIsTerminal,
                                                            0);
                }

                // register signal handlers
                signal(SIGINT,  signal_handler);
                signal(SIGTERM, signal_handler);
                signal(SIGHUP,  signal_handler);

                client.set_shutdown_callback(jack_shutdown);
                client.set_xrun_callback(jack_xrun);
                client.set_process_callback(process);
                last_stop = client.frame();
                activated_client active(client);
                // set this after starting the client so it will only be called
                // when the buffer size *changes*
                client.set_buffer_size_callback(jack_bufsize);

                active.connect_ports("out",
                                      options.output_ports.begin(), options.output_ports.end());
                active.connect_ports("sync_out",
                                      options.syncout_ports.begin(), options.syncout_ports.end());
                active.connect_ports(options.trigin_ports.begin(), options.trigin_ports.end(),
                                      "trig_in");

		// Poll the running flag and the stimulus queue. Running gets
		// flipped to false by signal handlers; stimulus queue flags
		// finished when the stimulus list is complete.
                while (running && !stim_queue->finished()) {
                        usleep(100000);
                }
                stim_queue->stop();
                stim_queue->join();
                // wait for posttrigger and midi buffers to clear
                sleep(options.posttrigger_interval_sec.value_or(0.0) + 1.0);

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


jstim_options::jstim_options(string const &program_name)
        : program_options(program_name)
{
        using std::vector;

        po::options_description jillopts("JILL options");
        jillopts.add_options()
                ("server,s",  po::value(&server_name), "connect to specific jack server")
                ("name,n",    po::value(&client_name)->default_value(_program_name),
                 "set client name")
                ("out,o",     po::value(&output_ports),
                 "add connection to output audio port")
                ("event,e",   po::value(&syncout_ports),
                 "add connection to output event port")
                ("trig,t",
                 po::value(&trigin_ports)->multitoken()->zero_tokens(),
                 "turn on triggered mode (playback only occurs on triggers) and optionally add connection to input trigger port.");

        // tropts is a group of options
        po::options_description opts("Stimulus options");
        opts.add_options()
                ("shuffle,S", "shuffle order of presentation")
                ("loop,l",    "loop endlessly")
                ("repeats,r", po::value(&nreps)->default_value(1),
                 "default number of repetitions (can be overridden for individual stimuli)")
                ("gap,g",     po::value(&min_gap_sec)->default_value(2.0),
                 "minimum gap between sound (s)")
                ("interval,i",po::value(&min_interval_sec)->default_value(0.0),
                 "minimum interval between stimulus start times (s)")
                ("trial-before", po::value(&pretrigger_interval_sec),
                 "if set, open the trial window (channel 1) this many seconds before stimulus onset (does not apply to triggered mode)")
                ("trial-after", po::value(&posttrigger_interval_sec),
                 "if set, close the trial window (channel 1) this many seconds after the stimulus ends")
                ("condition-prob", po::value(&condition_prob)->default_value(0.0),
                 "proportion of each stimulus's repetitions to mark as being in "
                 "the manipulated condition (channel 2). Exactly this fraction "
                 "of every stimulus's repeats is marked, chosen at random.");


        cmd_opts.add(jillopts).add(opts);
        cmd_opts.add_options()
                ("stim", po::value<vector<string> >(&stimuli)->multitoken(), "stimulus file");
        pos_opts.add("stim", -1);
        visible_opts.add(jillopts).add(opts);
}

void
jstim_options::print_usage()
{
        std::cout << "Usage: " << _program_name << " [options] [stim1 [nreps]] [stim2 [nreps]] ...\n"
                  << visible_opts << std::endl
                  << "Ports:\n"
                  << " * out:       sampled output of the presented stimulus\n"
                  << " * sync_out:  event port reporting stimulus onset/offsets\n"
                  << " * trig_in:   (optional) event port for triggering playback"
                  << std::endl;
}

void
jstim_options::process_options()
{
        program_options::process_options();
        const double pre_and_post_gap = pretrigger_interval_sec.value_or(0.0) +
                posttrigger_interval_sec.value_or(0.0);
        if (pre_and_post_gap >= min_gap_sec) {
                LOG << "ERROR: pretrigger + posttrigger intervals must be less than the gap between stimuli!" << std::endl;
                throw Exit(EXIT_FAILURE);
        }

}
