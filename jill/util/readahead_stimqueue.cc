/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <chrono>
#include <thread>
#include "../logging.hh"
#include "readahead_stimqueue.hh"

using namespace jill::util;

namespace {

/* How long the worker sleeps before checking whether the realtime thread has
 * taken the pre-loaded trial.
 *
 * Nothing wakes it for that any more. It used to be signalled from release(),
 * which meant pthread_cond_signal on the audio path -- glibc's takes the
 * condition variable's internal lock and issues a futex wake. The realtime
 * thread now promotes the pre-loaded trial itself, so this only governs how
 * quickly the empty slot is refilled.
 *
 * It bounds the queue's sustained rate at one trial per interval, because
 * head() empties _next and nothing refills it until the next wake. That is not
 * a throughput problem for the protocols this runs -- stimuli last a second or
 * two with gaps of at least a second -- but it is a jitter problem: whatever
 * the interval is, it is also how much the gap between stimuli can vary from
 * one run to the next. --gap promises a minimum rather than an exact spacing,
 * so this breaks no contract, but repeatable is better than merely conformant.
 * Five milliseconds costs two hundred wakeups a second on a thread that does
 * one comparison when it has nothing to do, and measures out: consuming
 * back-to-back went from 51 ms a trial to 5.6, and a consumer holding each
 * trial for 20 ms now runs at 21.5 ms a trial rather than 50, meaning the
 * queue has stopped being the thing setting the pace.
 *
 * Note this is a floor, not the whole story: a stimulus not already in memory
 * still has to be read and resampled, which dominates for a playlist of
 * distinct files and vanishes for one that repeats.
 *
 * It also bounds how long stop() takes to be noticed, since sleep_for cannot
 * be interrupted. Five milliseconds is not worth a mutex and a
 * condition_variable_any to skip, but the two are coupled: raise this interval
 * much and an interruptible wait starts to earn its keep. */
const auto poll_interval = std::chrono::milliseconds(5);

}

readahead_stimqueue::readahead_stimqueue(iterator first, iterator last,
                                         nframes_t samplerate,
                                         bool loop)
        :  _first(first), _last(last), _it(first),
           _next(nullptr), _head(nullptr), _previous(nullptr),
           _samplerate(samplerate), _loop(loop), _finished(false),
           // a lambda rather than a pointer-to-member: jthread supplies the
           // stop token as the first argument, which for a pmf is where the
           // object has to go. Qualified because the constructor's `loop`
           // parameter would otherwise shadow the member function.
           _thread([this](std::stop_token st) { this->loop(st); })
{}

void
readahead_stimqueue::stop()
{
        LOG << "stimulus queue terminated by stop()";
        _thread.request_stop();
}

void
readahead_stimqueue::join()
{
        // guarded so that an explicit join followed by the destructor, which
        // is the normal sequence, does not throw
        if (_thread.joinable())
                _thread.join();
}


/*
 * The worker keeps _next filled and does nothing else. It never writes _head:
 * the realtime thread promotes for itself in head(), so a stimulus becomes
 * available the moment the previous one is released rather than whenever this
 * thread is next scheduled.
 *
 * Loading a stimulus that is already loaded is cheap -- load_samples() checks
 * -- so a pass with nothing to do costs a comparison and a sleep.
 */
void
readahead_stimqueue::loop(std::stop_token st)
{
        while (!st.stop_requested()) {
                if (_next.load() == nullptr) {
                        if (_it == _last && _loop) {
                                _it = _first;
                        }
                        if (_it != _last) {
                                trial * t = &*_it;
                                t->stim->load_samples(_samplerate);
                                LOG << "pre-loaded next stim: " << t->stim->name()
                                    << " (" << t->stim->duration() << " s)";
                                _next.store(t);
                                _it += 1;
                        }
                        else if (_head.load() == nullptr) {
                                // list exhausted and nothing still playing
                                break;
                        }
                }
                std::this_thread::sleep_for(poll_interval);
        }
        LOG << "end of stimulus list";
        // published last, so a caller that sees finished() also sees
        // everything the worker did before it
        _finished.store(true, std::memory_order_release);
}

jill::util::trial const *
readahead_stimqueue::head()
{
        trial * ptr = _head.load();
        if (ptr) return ptr;
        /* Nothing current, so take whatever the worker has ready. Both steps
         * are ours alone: the worker only ever fills _next, and only the
         * realtime thread writes _head. */
        ptr = _next.exchange(nullptr);
        if (ptr) _head.store(ptr);
        return ptr;
}

jill::util::trial const *
readahead_stimqueue::previous()
{
        return _previous.load();
}


void
readahead_stimqueue::release()
{
        /* Hand the current trial to _previous and clear _head. The next call to
         * head() promotes the pre-loaded one, so there is no window where the
         * queue looks empty while a background thread catches up. */
        _previous.store(_head.load());
        _head.store(nullptr);
}
