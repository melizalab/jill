/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for the threshold-crossing detector behind jdetect.
 *
 * Several cases here are characterization tests: they pin down behaviour that
 * is arguably wrong but is what the detector has always done, so that a
 * deliberate change shows up as a failing test rather than a silent shift in
 * how recordings get triggered. Those are called out individually.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>

#include "jill/dsp/crossing_counter.hh"
#include "jill/dsp/crossing_trigger.hh"

using jill::dsp::crossing_counter;
using jill::dsp::crossing_trigger;

namespace {

const float thresh = 0.5;

/* A square wave alternating either side of the threshold. Every low-to-high
 * step is one positive-going crossing, so the count is predictable. */
std::vector<float> square(std::size_t n, float low = 0.0, float high = 1.0)
{
        std::vector<float> out(n);
        for (std::size_t i = 0; i < n; ++i) {
                out[i] = (i % 2 == 0) ? low : high;
        }
        return out;
}

/* A constant signal produces no crossings at all. */
std::vector<float> flat(std::size_t n, float value = 0.0)
{
        return std::vector<float>(n, value);
}

}

TEST_CASE("a new counter is empty and remembers its parameters") {
        crossing_counter<float> counter(thresh, 16, 4);
        CHECK(counter.count() == 0);
        CHECK(counter.thresh() == thresh);
        CHECK(counter.period_size() == 16);
}

TEST_CASE("a short or empty block is rejected rather than read") {
        crossing_counter<float> counter(thresh, 16, 4);
        const std::vector<float> data = square(8);

        // fewer than two samples cannot contain a crossing. Before this was
        // guarded, size == 0 dereferenced the pointer anyway.
        CHECK(counter.push(data.data(), 0, 4) == -1);
        CHECK(counter.push(data.data(), 1, 4) == -1);
        CHECK(counter.push(nullptr, 0, 4) == -1);
        CHECK(counter.count() == 0);
}

TEST_CASE("a flat signal never crosses") {
        crossing_counter<float> counter(thresh, 8, 4);
        const std::vector<float> data = flat(256);
        CHECK(counter.push(data.data(), data.size(), 1) == -1);
        CHECK(counter.count() == 0);
}

TEST_CASE("a signal below threshold never crosses") {
        crossing_counter<float> counter(thresh, 8, 4);
        const std::vector<float> data = square(256, 0.0, 0.4);   // high stays under
        CHECK(counter.push(data.data(), data.size(), 1) == -1);
        CHECK(counter.count() == 0);
}

TEST_CASE("crossings are counted once the window fills") {
        // period_size 8, 4 periods: the running count covers 32 samples
        crossing_counter<float> counter(thresh, 8, 4);
        const std::vector<float> data = square(64);

        counter.push(data.data(), data.size(), 1000);   // threshold too high to trip
        // a square wave crosses on every other sample
        CHECK(counter.count() > 0);
        CHECK(counter.count() <= static_cast<int>(8 * 4 / 2));
}

TEST_CASE("the counter reports which period tripped the threshold") {
        crossing_counter<float> counter(thresh, 8, 2);
        const std::vector<float> data = square(128);

        // a low count threshold trips as soon as the window is full
        const int period = counter.push(data.data(), data.size(), 1);
        CHECK(period >= 0);
}

TEST_CASE("reset clears the running count") {
        crossing_counter<float> counter(thresh, 8, 4);
        const std::vector<float> data = square(128);
        counter.push(data.data(), data.size(), 1000);
        REQUIRE(counter.count() > 0);

        counter.reset();
        CHECK(counter.count() == 0);
}

TEST_CASE("a negative count threshold detects falling below a rate") {
        crossing_counter<float> counter(thresh, 8, 2);

        // fill the window with activity so the counter is full and busy
        const std::vector<float> busy = square(64);
        counter.push(busy.data(), busy.size(), 1000);
        REQUIRE(counter.count() > 0);

        // then go quiet: the running count falls and the negative threshold trips
        const std::vector<float> quiet = flat(64);
        CHECK(counter.push(quiet.data(), quiet.size(), -1) >= 0);
}

TEST_CASE("the state buffer is filled when requested") {
        crossing_counter<float> counter(thresh, 8, 4);
        const std::vector<float> data = square(64);
        std::vector<float> state(data.size(), -1.0);

        counter.push(data.data(), data.size(), 1000, state.data());
        for (std::size_t i = 0; i < state.size(); ++i) {
                CAPTURE(i);
                CHECK(state[i] >= 0.0);         // normalized running count
        }
}

TEST_CASE("characterization: the count depends on how the signal is blocked") {
        // Two quirks interact here. push() seeds 'last' from samples[0] and
        // starts comparing at index 1, and that value is not carried between
        // calls, so a transition straddling a block boundary is invisible.
        // But _period_crossings and _period_nsamples *are* carried, so the
        // period closes at a different sample and sweeps up a different set
        // of crossings. The two effects do not cancel.
        //
        // The exact values below are what the detector does today, not what
        // it necessarily should do. They are pinned so that a deliberate
        // change to the period bookkeeping shows up here rather than silently
        // altering when jdetect triggers.
        const std::vector<float> whole = {0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0};

        crossing_counter<float> one(thresh, 4, 2);
        one.push(whole.data(), whole.size(), 1000);
        const int count_one_block = one.count();

        crossing_counter<float> two(thresh, 4, 2);
        two.push(whole.data(), 4, 1000);
        two.push(whole.data() + 4, 4, 1000);
        const int count_two_blocks = two.count();

        CHECK(count_one_block == 2);
        CHECK(count_two_blocks == 3);
        CHECK(count_one_block != count_two_blocks);
}

TEST_CASE("a new trigger starts closed") {
        crossing_trigger<float> trigger(thresh, 4, 2, thresh, 1, 2, 8);
        CHECK_FALSE(trigger.open());
        CHECK(trigger.open_thresh() == thresh);
        CHECK(trigger.close_thresh() == thresh);
}

TEST_CASE("silence leaves the gate closed") {
        crossing_trigger<float> trigger(thresh, 4, 2, thresh, 1, 2, 8);
        const std::vector<float> quiet = flat(256);
        CHECK(trigger.push(quiet.data(), quiet.size()) == -1);
        CHECK_FALSE(trigger.open());
}

TEST_CASE("a loud signal opens the gate and silence closes it again") {
        crossing_trigger<float> trigger(thresh, 4, 2, thresh, 1, 2, 8);
        const std::vector<float> loud = square(256);
        const std::vector<float> quiet = flat(256);

        int offset = -1;
        for (int i = 0; i < 8 && !trigger.open(); ++i) {
                offset = trigger.push(loud.data(), loud.size());
        }
        REQUIRE(trigger.open());
        CHECK(offset >= 0);

        offset = -1;
        for (int i = 0; i < 8 && trigger.open(); ++i) {
                offset = trigger.push(quiet.data(), quiet.size());
        }
        CHECK_FALSE(trigger.open());
        CHECK(offset >= 0);
}

TEST_CASE("the reported offset lies inside the block") {
        crossing_trigger<float> trigger(thresh, 4, 2, thresh, 1, 2, 8);
        const std::vector<float> loud = square(256);

        for (int i = 0; i < 8; ++i) {
                const int offset = trigger.push(loud.data(), loud.size());
                if (offset >= 0) {
                        // crossing_trigger uses this to index into the block,
                        // so it has to stay in range whatever the period
                        // bookkeeping does
                        CHECK(offset < static_cast<int>(loud.size()));
                        break;
                }
        }
}

TEST_CASE("the gate does not reopen while it is already open") {
        crossing_trigger<float> trigger(thresh, 4, 2, thresh, 1, 2, 8);
        const std::vector<float> loud = square(256);

        for (int i = 0; i < 8 && !trigger.open(); ++i) {
                trigger.push(loud.data(), loud.size());
        }
        REQUIRE(trigger.open());

        // continued activity keeps it open and reports no further transitions
        for (int i = 0; i < 4; ++i) {
                CHECK(trigger.push(loud.data(), loud.size()) == -1);
                CHECK(trigger.open());
        }
}
