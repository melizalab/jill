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
#ifndef _EVENT_RANDOMIZER_HH
#define _EVENT_RANDOMIZER_HH

#include <atomic>
#include <cstddef>
#include <memory>
#include <random>
#include <string>
#include <utility>

namespace jill { namespace util {

/**
 * Tracks how many times an event has been emitted for a collection of stimuli
 * (or any other string label). Useful when you want to only generate
 * optical stimulus pulses on a subset of trials, but need to balance
 * proportions across stimuli. The implementation is not quite random without
 * replacement; instead, the current proportion is compared against a fixed
 * probability, and a random draw is taken to push the current proportion
 * towards the desired target.
 *
 * The randomizer is initialized with a desired proportion (prob) and a
 * proportional control parameter that determines how much of a correction to
 * make on the next trial. Setting this parameter between around 8-10 should
 * ensure that the final observed proportion for each stimulus is relatively
 * close after 20 presentations. Smaller values add more randomness at the
 * expense of larger variance around the target.
 *
 * present() is called from the realtime thread and does not allocate. That is
 * the reason for the storage below rather than the std::map this used to hold:
 * map::operator[] allocates a tree node and copies the key on first sight of a
 * stimulus, and the key copy allocates again once the name outgrows the small
 * string buffer. Here the table is sized once at construction and never grows,
 * and the names live inline rather than in std::strings, so a lookup or an
 * insertion touches no allocator at all.
 *
 * The counts are written only by the thread calling present(). Read them from
 * another thread only once that thread has stopped -- jclicker reports them
 * after the client is deactivated. The two overflow flags are the exception:
 * they are atomic precisely so the main thread can poll them while the
 * realtime thread runs, which is how a full table gets reported at all. The
 * realtime thread cannot log: LOG allocates and takes a mutex, DBG compiles
 * out of release builds, and the process callback is noexcept.
 */
class event_randomizer {

public:
        /** stimuli tracked when no capacity is given */
        static constexpr std::size_t default_capacity = 64;
        /** longest name stored in full, excluding the terminator */
        static constexpr std::size_t max_name = 119;

        /** one stimulus and its tally */
        struct entry {
                char name[max_name + 1];
                /** the name's length *before* truncation, so that two names
                 *  sharing a long prefix are still told apart by length */
                std::size_t len;
                /** times an event was emitted */
                int events;
                /** times the stimulus was seen */
                int presentations;
        };

        /**
         * @param prob      desired proportion of presentations to emit on
         * @param control   proportional gain of the correction
         * @param rng_seed  seed for the draw
         * @param capacity  how many distinct stimuli can be tracked. The table
         *                  is allocated once, here, and never grows.
         */
        event_randomizer(float prob, float control, float rng_seed,
                         std::size_t capacity = default_capacity);

        /**
         * Look up whether to emit an event for a given stimulus.
         *
         * Realtime safe: no allocation, no locking, and a scan bounded by the
         * number of distinct stimuli seen so far.
         *
         * If the table is full and the stimulus is not already in it, the draw
         * is made against the desired proportion with no correction applied,
         * and overflowed() starts returning true. The alternative was to fail,
         * which in the realtime thread means failing silently; a wider
         * variance on an unlucky run is the better trade.
         *
         * @param name  the label, which need not be null-terminated
         * @param len   its length
         */
        bool present(char const * name, std::size_t len);
        /** as above, for a null-terminated name */
        bool present(char const * name);

        /** get the current number of events and total presentations for a given stimulus */
        std::pair<int, int> get_events(std::string const & name) const;

        /** get the current proportion of events per total presentations */
        float get_proportion(std::string const & name) const;

        /** @return true if a stimulus was dropped because the table was full */
        bool overflowed() const { return _overflowed.load(std::memory_order_relaxed); }
        /** @return true if a name was longer than max_name and was truncated */
        bool truncated() const { return _truncated.load(std::memory_order_relaxed); }
        /** @return how many stimuli can be tracked */
        std::size_t capacity() const { return _capacity; }

        /* iteration over the stimuli seen so far, for reporting */
        std::size_t size() const { return _n; }
        bool empty() const { return _n == 0; }
        entry const * begin() const { return _counts.get(); }
        entry const * end() const { return _counts.get() + _n; }

        /** reset the counts for one stimulus */
        void reset(std::string const & name);

        /** reset all the counts */
        void reset();

private:
        /** @return the entry for a name, or nullptr */
        entry * find(char const * name, std::size_t len);
        entry const * find(char const * name, std::size_t len) const;

        float _desired_proportion;
        float _control;

        std::unique_ptr<entry[]> _counts;
        std::size_t _capacity;
        std::size_t _n;

        std::atomic<bool> _overflowed;
        std::atomic<bool> _truncated;

        std::mt19937 _rng;
        std::uniform_real_distribution<float> _distribution;

};

}}

#endif
