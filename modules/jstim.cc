/*
 * Play stimuli at intervals or when triggered.
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <iostream>
#include <atomic>
#include <csignal>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <algorithm>
#include <random>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>

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
        std::optional<float> pretrigger_interval_sec;
        std::optional<float> posttrigger_interval_sec;
        float condition_prob;
        nframes_t min_gap;
        nframes_t min_interval;
        std::optional<nframes_t> pretrigger_interval;
        std::optional<nframes_t> posttrigger_interval;

protected:

        void print_usage() override;
        void process_options() override;

}; // jstim_options


jstim_options options(PROGRAM_NAME);
// Holds (and owns) all the stimuli. Possibly worth revisiting, moving ownership
// into _stimlist as a shared_ptr, which would remove the declaration-order
// dependency noted below.
std::vector<std::unique_ptr<stimulus_t>> _stimuli;
// The sequence of trials being played. May be shuffled, and holds one entry
// per repetition. Each trial presenting the same stimulus aliases the objects
// held by _stimuli
std::vector<util::trial> _stimlist;
// The queue of stimuli. Handles the actual file reads and any resampling
// needed in a background thread. Must be declared after _stimuli so that the
// thread can continue to dereference the objects owned by _stimuli.
std::unique_ptr<util::readahead_stimqueue> stim_queue;
jack_port_t *port_out, *port_syncout, *port_trigin;
std::atomic<int> xruns(0);                  // xrun counter
// MIDI events the realtime thread could not write. jack refuses an event whose
// offset is not inside the period, and refuses to reserve space in a full
// buffer; write_message reports both by returning non-zero, which must not be
// discarded. A dropped sync event is otherwise invisible, showing up only as a
// trial with no end and no indication of why. Counted here and reported by
// main, since the callback cannot log.
std::atomic<int> midi_errors(0);
// time when last stimulus ended. global atomic so that the main loop can
// initialize it. Otherwise the pre-stimulus trigger doesn't work on the first
// stimulus.
std::atomic<nframes_t> last_stop(0);
std::atomic<int> ret(EXIT_SUCCESS);
// cleared by a signal or by the server going away; main stops the queue
std::atomic<bool> running(true);

/** Write a sync event, counting the failures instead of discarding them. */
static void
emit(void * buffer, nframes_t offset, midi::status_type status, char const * name) JILL_RT
{
        if (midi::write_message(buffer, offset, status, name) != 0) {
                midi_errors.fetch_add(1, std::memory_order_relaxed);
        }
}

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
        // A stimulus that ends flush with the end of a period wants its
        // stim_off at offset nframes, which is one past the last frame of the
        // period and which jack refuses. The event belongs at the first frame
        // of the next period -- the same frame either way, just expressed in
        // the next period's coordinates -- so it is carried over and written
        // below. Without this the stop was silently dropped, roughly one trial
        // in every period_size, leaving the trial with no end and the trigger
        // line stuck on.
        static jill::stimulus_t const * deferred_off;
        static bool deferred_off_condition;

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

        // Ahead of everything else: events have to be written in ascending
        // order, and this one belongs at the first frame of this period.
        if (deferred_off) {
                emit(sync, 0, midi::status_type::stim_off, deferred_off->name());
                if (deferred_off_condition && !options.posttrigger_interval) {
                        emit(sync, 0,
                             midi::status_type(midi::status_type::stim_off,
                                               midi::channel::condition),
                             deferred_off->name());
                }
                DBG << "wrote deferred stim_off: time=" << time
                    << ", stim=" << deferred_off->name();
                deferred_off = nullptr;
                deferred_off_condition = false;
        }

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
                        emit(sync, *otrig,
                                            midi::status_type(midi::status_type::stim_off,
                                                              midi::channel::trial),
                                            last_stim->name());
                        if (last->condition) {
                                emit(sync, *otrig,
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
                emit(sync, period_offset, midi::status_type::stim_on, stim->name());
                // no pretrigger in triggered mode, so the condition window
                // opens with the stimulus
                if (current->condition) {
                        emit(sync, period_offset,
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
                                emit(sync, *otrig,
                                                    midi::status_type(midi::status_type::stim_on,
                                                                      midi::channel::trial),
                                                    stim->name());
                                if (current->condition) {
                                        emit(sync, *otrig,
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
                emit(sync, period_offset, midi::status_type::stim_on, stim->name());
                // The condition window brackets the trial, so when there is a
                // pretrigger it opened there and this would double it. Without
                // one there is no wider window to ride on, so it opens here.
                if (current->condition && !options.pretrigger_interval) {
                        emit(sync, period_offset,
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
                const nframes_t off_offset = period_offset + nsamples;
                last_stop = time + off_offset;
                if (off_offset >= nframes) {
                        // ends flush with the period; see deferred_off above
                        deferred_off = stim;
                        deferred_off_condition = was_condition;
                }
                else {
                        emit(sync, off_offset, midi::status_type::stim_off, stim->name());
                        // as at onset: the posttrigger closes the condition
                        // window when there is one, otherwise it closes with
                        // the stimulus
                        if (was_condition && !options.posttrigger_interval) {
                                emit(sync, off_offset,
                                     midi::status_type(midi::status_type::stim_off,
                                                       midi::channel::condition),
                                     stim->name());
                        }
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
        // Registered after activation, so this only ever runs on a real change,
        // never to report the initial size. JACK stops the realtime engine when
        // the buffer size changes, so this creates a gap that needs to be
        // treated like an xrun. OK to log because this is not realtime.
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
        // Only touch lock-free atomics to stay within bounds of what a signal
        // handler can safely do. main() takes care of the cleanup. The xruns
        // variable is incremented to tell the realtime thread to cut the
        // stimulus short at the start of the next period rather than playing it
        // out.
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

/**
 * How far the delivered condition fraction may sit from the requested
 * probability before the request is refused. This absorbs a probability
 * written as a short decimal -- 0.33 for one third is 0.0033 away from what
 * it means -- and nothing else. A repeat count that cannot express the
 * probability at all is a different situation: 0.25 over three repetitions
 * would deliver 0.333, and running that experiment is worse than not running
 * it, so it stops rather than reporting the discrepancy and continuing.
 */
static constexpr double condition_tolerance = 0.01;

/**
 * Parse the stimulus list, pairing each stimulus argument with its repeat
 * count.
 *
 * A bare count following a filename is that stimulus's number of repeats, and
 * is consumed here so it is not opened as a file.
 *
 * Called by both the option check and the loader so the two cannot disagree
 * about how many repetitions a stimulus gets.
 */
static std::vector<std::pair<string, size_t> >
parse_stimlist(std::vector<string> const & stims, size_t default_nreps)
{
        std::vector<std::pair<string, size_t> > out;
        for (size_t i = 0; i < stims.size(); ++i) {
                string const & name = stims[i];
                size_t nreps = default_nreps;
                if ((i + 1) < stims.size() && parse_count(stims[i + 1], nreps)) {
                        i += 1;
                }
                out.emplace_back(name, nreps);
        }
        return out;
}

/**
 * How many of a stimulus's repetitions carry the condition.
 *
 * The fraction has to be one this repeat count can express. Throws if this is
 * not possible (within condition_tolerance), reporting the value that would have
 * been delivered.
 */
static size_t
condition_count(string const & name, size_t nreps, float condition_prob)
{
        namespace fs = std::filesystem;
        const size_t want = std::lround(double(condition_prob) * nreps);
        const double achieved = (nreps > 0) ? double(want) / nreps : 0.0;
        if (std::fabs(achieved - condition_prob) > condition_tolerance) {
                std::ostringstream msg;
                msg << "--condition-prob " << condition_prob
                    << " cannot be delivered for " << fs::path(name).filename().string()
                    << ": " << nreps
                    << (nreps == 1 ? " repetition gives " : " repetitions give ")
                    << achieved
                    << ". Use a repeat count this probability divides into,"
                       " or change --condition-prob.";
                throw std::invalid_argument(msg.str());
        }
        return want;
}

/**
 * Parse the list of stimuli, shuffle, and set condition flags. Actually
 * loading the stimuli is taken care of by the readahead queue.
 */
static void
init_stimset(std::vector<string> const & stims, size_t const default_nreps,
             float condition_prob, std::mt19937 & rng)
{
        size_t marked = 0;
        for (auto const & entry : parse_stimlist(stims, default_nreps)) {
                string const & name = entry.first;
                const size_t nreps = entry.second;
                // Assign the condition by counting rather than by drawing per
                // trial, so every stimulus carries it on exactly the same
                // fraction of its repetitions. This is set randomly using the
                // same seed used to shuffle, so both the order and which
                // repetitions have the condition variable set should be
                // replicable with the same seed.
                //
                // The repeat count was checked against the probability before
                // the client was created, so this cannot throw here.
                const size_t want = condition_count(name, nreps, condition_prob);
                try {
                        _stimuli.push_back(std::make_unique<file::stimfile>(name));
                        jill::stimulus_t * stim = _stimuli.back().get();
                        std::vector<bool> flags(nreps, false);
                        std::fill_n(flags.begin(), want, true);
                        std::shuffle(flags.begin(), flags.end(), rng);
                        for (size_t j = 0; j < nreps; ++j) {
                                _stimlist.push_back(util::trial{stim, flags[j]});
                        }
                        marked += want;
                }
                catch (jill::FileError const & e) {
                        LOG << "invalid stimulus " << name << ": " << e.what();
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

                DBG << "loading " << options.stimuli.size() << " stimuli";
                const auto given = options.get<string>("shuffle");
                unsigned seed;
                if (given && !given->empty()) {
                        // A string rather than an unsigned so that "-S" alone
                        // and "-S 0" are distinguishable: with a numeric
                        // implicit value they both arrive as zero, and a seed
                        // of zero is as good as any other.
                        std::size_t parsed = 0;
                        if (!parse_count(*given, parsed)) {
                                throw std::invalid_argument(
                                        "--shuffle takes a non-negative integer seed, or no value");
                        }
                        seed = static_cast<unsigned>(parsed);
                }
                else {
                        seed = std::random_device{}();
                }
                std::mt19937 rng(seed);
                LOG << "random seed: " << seed;
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
                // Reported rather than ignored: a sync event that never made
                // it out shows up downstream as a trial with no end, and
                // nothing else would say why.
                if (const int lost = midi_errors.load()) {
                        LOG << "ERROR: " << lost << " sync event(s) could not be"
                            " written; some trials may have no matching offset";
                        ret = EXIT_FAILURE;
                }
        }

        catch (Exit const &e) {
                ret = e.status();
        }
        catch (std::exception const &e) {
                LOG << "ERROR: " << e.what();
                ret = EXIT_FAILURE;
        }

        // Tear down explicitly rather than leaving it to static destruction
        // based on declaration order.
        stim_queue.reset();
        return ret;
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
                ("stim-list", po::value<string>(),
                 "read stimuli from a file, one per line, each optionally "
                 "followed by whitespace and a repeat count. Blank lines and "
                 "lines beginning with # are ignored. Combines with stimuli "
                 "named on the command line, which come first.")
                ("shuffle,S", po::value<string>()->implicit_value(""),
                 "shuffle order of presentation, optionally with a seed. The "
                 "seed also governs which repetitions carry the condition, so "
                 "one seed reproduces the whole schedule; if none is given, one "
                 "is drawn and logged.")
                ("loop,l",    "loop endlessly")
                ("repeats,r", po::value(&nreps)->default_value(1),
                 "default number of repetitions (can be overridden for individual stimuli)")
                ("gap,g",     po::value(&min_gap_sec)->default_value(2.0),
                 "minimum gap between sound (s)")
                ("interval,i",po::value(&min_interval_sec)->default_value(0.0),
                 "minimum interval between stimulus start times (s)")
                ("trial-before", po::value<float>(),
                 "if set, open the trial window (channel 1) this many seconds before stimulus onset (does not apply to triggered mode)")
                ("trial-after", po::value<float>(),
                 "if set, close the trial window (channel 1) this many seconds after the stimulus ends")
                ("condition-prob", po::value(&condition_prob)->default_value(0.0),
                 "proportion of each stimulus's repetitions to mark as being in "
                 "the manipulated condition (channel 2). Exactly this fraction "
                 "of every stimulus's repeats is marked, chosen at random, so "
                 "the repeat count has to be one the fraction divides into: "
                 "0.5 needs an even number of repeats, 0.25 a multiple of four. "
                 "A repeat count that cannot deliver the fraction is refused "
                 "rather than rounded to the nearest whole trial.");


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

/**
 * Append the stimuli named in a list file.
 *
 * One per line, each optionally followed by a repeat count. The count is
 * separated from the name by whitespace, and only the *last* field is taken as
 * one -- so a path containing spaces still works, which splitting the line
 * would break. Appends to whatever was given on the command line rather than
 * replacing it, so the two can be combined.
 */
static void
read_stim_list(string const & path, std::vector<string> & out)
{
        std::ifstream ff(path);
        if (!ff) {
                throw std::invalid_argument("cannot read stimulus list " + path);
        }
        string line;
        int lineno = 0;
        while (std::getline(ff, line)) {
                lineno += 1;
                // strip a trailing comment and surrounding whitespace
                const auto hash = line.find('#');
                if (hash != string::npos) line.erase(hash);
                const auto first = line.find_first_not_of(" \t\r");
                if (first == string::npos) continue;
                const auto last = line.find_last_not_of(" \t\r");
                line = line.substr(first, last - first + 1);

                // Split a trailing count off the end, if there is one. Anything
                // else, including embedded spaces, belongs to the name.
                const auto gap = line.find_last_of(" \t");
                size_t count = 0;
                if (gap != string::npos && parse_count(line.substr(gap + 1), count)) {
                        const auto name_end = line.find_last_not_of(" \t", gap);
                        if (name_end == string::npos) {
                                throw std::invalid_argument(
                                        path + ":" + std::to_string(lineno) +
                                        ": repeat count with no stimulus");
                        }
                        out.push_back(line.substr(0, name_end + 1));
                        out.push_back(line.substr(gap + 1));
                }
                else {
                        out.push_back(line);
                }
        }
}

void
jstim_options::process_options()
{
        program_options::process_options();
        // Need to manually assign as a shim for older versions of boost that
        // couldn't handle options bound to std::optionals
        pretrigger_interval_sec = get<float>("trial-before");
        posttrigger_interval_sec = get<float>("trial-after");
        if (const auto listfile = get<string>("stim-list")) {
                read_stim_list(*listfile, stimuli);
        }
        const double pre_and_post_gap = pretrigger_interval_sec.value_or(0.0) +
                posttrigger_interval_sec.value_or(0.0);
        if (pre_and_post_gap >= min_gap_sec) {
                LOG << "ERROR: pretrigger + posttrigger intervals must be less than the gap between stimuli!" << std::endl;
                throw Exit(EXIT_FAILURE);
        }
        // Validate condition probability against repeats and the stimulus list
        // here to catch misconfigured runs before starting the client. 
        if (condition_prob < 0.0 || condition_prob > 1.0) {
                throw std::invalid_argument("--condition-prob must be between 0 and 1");
        }
        for (auto const & entry : parse_stimlist(stimuli, nreps)) {
                condition_count(entry.first, entry.second, condition_prob);
        }
}
