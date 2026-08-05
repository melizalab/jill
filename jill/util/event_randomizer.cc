/*
 * JILL - C++ framework for JACK
 *
 * Balances how often an event is emitted across a set of stimulus labels.
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include "event_randomizer.hh"

using namespace jill::util;

event_randomizer::event_randomizer(float prob, float control, float rng_seed,
                                   std::size_t capacity)
        : _desired_proportion(prob), _control(control),
          _capacity(capacity), _n(0),
          _overflowed(false), _truncated(false),
          _rng(rng_seed), _distribution(0.0, 1.0) {
        if (prob > 1.0 || prob < 0.0) {
                throw std::out_of_range("desired proportion must be between 0.0 and 1.0");
        }
        if (capacity == 0) {
                throw std::out_of_range("capacity must be at least 1");
        }
        // the one allocation: done here, before the caller hands this to a
        // realtime thread, and never repeated
        _counts.reset(new entry[capacity]);
}

event_randomizer::entry *
event_randomizer::find(char const * name, std::size_t len)
{
        // const_cast rather than a second copy of the loop; the const overload
        // is the one doing the real work
        return const_cast<entry *>(
                static_cast<event_randomizer const *>(this)->find(name, len));
}

event_randomizer::entry const *
event_randomizer::find(char const * name, std::size_t len) const
{
        const std::size_t stored = std::min(len, max_name);
        for (std::size_t i = 0; i < _n; ++i) {
                entry const & e = _counts[i];
                // compare the untruncated lengths first: it rejects almost
                // everything in one integer test, and it keeps two names that
                // truncate to the same prefix from being treated as one
                if (e.len == len && std::memcmp(e.name, name, stored) == 0) {
                        return &e;
                }
        }
        return nullptr;
}

bool
event_randomizer::present(char const * name, std::size_t len)
{
        entry * e = find(name, len);

        if (e == nullptr) {
                if (_n == _capacity) {
                        /* Table full. Draw against the desired proportion with
                         * no correction and do not record the result: the
                         * alternative is to fail, and in the realtime thread
                         * failing means failing silently. The flag is what
                         * lets main() say something about it. */
                        _overflowed.store(true, std::memory_order_relaxed);
                        return _distribution(_rng) < _desired_proportion;
                }
                e = &_counts[_n];
                const std::size_t stored = std::min(len, max_name);
                std::memcpy(e->name, name, stored);
                e->name[stored] = '\0';
                if (len > max_name) {
                        _truncated.store(true, std::memory_order_relaxed);
                }
                e->len = len;
                e->events = 0;
                e->presentations = 0;
                _n += 1;
        }

        float new_prob(_desired_proportion);
        if (e->presentations > 0) {
                float observed_proportion = float(e->events) / float(e->presentations);
                float err = observed_proportion - _desired_proportion;
                new_prob = std::min(std::max(_desired_proportion - _control * err, 0.0f), 1.0f);
        }
        float draw = _distribution(_rng);
        e->presentations += 1;
        if (draw < new_prob) {
                e->events += 1;
                return true;
        }
        else {
                return false;
        }
}

bool
event_randomizer::present(char const * name)
{
        return present(name, std::strlen(name));
}

std::pair<int, int>
event_randomizer::get_events(std::string const & name) const
{
        entry const * e = find(name.data(), name.size());
        if (e == nullptr) {
                throw std::out_of_range("no counts for stimulus " + name);
        }
        return std::pair<int, int>(e->events, e->presentations);
}

float
event_randomizer::get_proportion(std::string const & name) const
{
        auto events = get_events(name);
        return float(events.first) / float(events.second);
}

void
event_randomizer::reset(std::string const & name)
{
        entry * e = find(name.data(), name.size());
        if (e == nullptr) return;
        // fill the hole with the last entry rather than shifting the rest; the
        // order carries no meaning
        _n -= 1;
        if (e != &_counts[_n]) {
                *e = _counts[_n];
        }
}

void
event_randomizer::reset()
{
        _n = 0;
}
