/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for the playback timing arithmetic extracted from jstim.
 *
 * doc/implementation-notes.md calls this logic tricky, and the source comments
 * around it hedge ("a bunch of annoying unsigned arithmetic here, but it seems
 * to work"). These tests are what turns that into something checked.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <limits>

#include "jill/dsp/playback_timing.hh"

using namespace jill;
using jill::dsp::next_onset_offset;
using jill::dsp::posttrigger_offset;
using jill::dsp::pretrigger_offset;
using jill::dsp::samples_to_copy;

namespace {
const nframes_t NFRAMES = 1024;
}

TEST_CASE("with both minimums satisfied a stimulus starts immediately") {
        // plenty of time has passed since the last start and the last stop
        CHECK(next_onset_offset(50000, 50000, 10000, 5000) == 0);
}

TEST_CASE("the interval constraint delays the onset") {
        // 4000 frames since the last onset, minimum spacing 10000
        CHECK(next_onset_offset(4000, 50000, 10000, 5000) == 6000);
}

TEST_CASE("the gap constraint delays the onset") {
        // 1000 frames since the last offset, minimum gap 5000
        CHECK(next_onset_offset(50000, 1000, 10000, 5000) == 4000);
}

TEST_CASE("the later of the two constraints wins") {
        // interval wants 6000, gap wants 4000
        CHECK(next_onset_offset(4000, 1000, 10000, 5000) == 6000);
        // and the other way round
        CHECK(next_onset_offset(9000, 1000, 10000, 8000) == 7000);
}

TEST_CASE("a constraint met exactly does not delay the onset") {
        // since_start == min_interval takes the else branch: not strictly
        // greater, so the remaining wait is zero
        CHECK(next_onset_offset(10000, 50000, 10000, 5000) == 0);
        CHECK(next_onset_offset(50000, 5000, 10000, 5000) == 0);
}

TEST_CASE("zero minimums mean no waiting") {
        CHECK(next_onset_offset(0, 0, 0, 0) == 0);
}

TEST_CASE("a delay beyond the period tells the caller to wait") {
        // the caller compares against nframes and does nothing this period
        const nframes_t offset = next_onset_offset(0, 0, 48000, 0);
        CHECK(offset == 48000);
        CHECK(offset >= NFRAMES);
}

TEST_CASE("elapsed times computed across a counter wrap still work") {
        // the caller derives these as `time - last_event`, which is exact under
        // modular arithmetic. A start just before the wrap and a current time
        // just after gives a small, correct difference.
        const nframes_t last_start = std::numeric_limits<nframes_t>::max() - 999;
        const nframes_t now = 24;               // wrapped past zero
        const nframes_t since_start = now - last_start;  // 1024
        REQUIRE(since_start == 1024);
        CHECK(next_onset_offset(since_start, since_start, 4096, 0) == 3072);
}

TEST_CASE("a pretrigger lands the requested distance before the onset") {
        const auto offset = pretrigger_offset(800, 300, NFRAMES);
        REQUIRE(offset.has_value());
        CHECK(*offset == 500);
}

TEST_CASE("a pretrigger at the very start of the period is emitted") {
        const auto offset = pretrigger_offset(300, 300, NFRAMES);
        REQUIRE(offset.has_value());
        CHECK(*offset == 0);
}

TEST_CASE("no pretrigger when the onset is closer than the interval") {
        // there is no room for the warning, so nothing is emitted
        CHECK_FALSE(pretrigger_offset(100, 300, NFRAMES).has_value());
}

TEST_CASE("no pretrigger when it falls outside this period") {
        // onset is far away, so the warning belongs to a later period
        CHECK_FALSE(pretrigger_offset(48000, 300, NFRAMES).has_value());
        // exactly at the period boundary belongs to the next one
        CHECK_FALSE(pretrigger_offset(NFRAMES + 300, 300, NFRAMES).has_value());
}

TEST_CASE("a posttrigger lands the requested distance after the offset") {
        // 200 frames since the stimulus ended, event due at 500
        const auto offset = posttrigger_offset(200, 500, NFRAMES);
        REQUIRE(offset.has_value());
        CHECK(*offset == 300);
}

TEST_CASE("a posttrigger due exactly now is emitted at the period start") {
        const auto offset = posttrigger_offset(500, 500, NFRAMES);
        REQUIRE(offset.has_value());
        CHECK(*offset == 0);
}

TEST_CASE("no posttrigger once the moment has passed") {
        // this is the case that relies on the subtraction wrapping: 500 - 900
        // is a very large unsigned number, which falls outside the period
        CHECK_FALSE(posttrigger_offset(900, 500, NFRAMES).has_value());
        CHECK_FALSE(posttrigger_offset(1000000, 500, NFRAMES).has_value());
}

TEST_CASE("no posttrigger when it is still further off than this period") {
        CHECK_FALSE(posttrigger_offset(0, 48000, NFRAMES).has_value());
}

TEST_CASE("a whole period of stimulus is copied when it fits") {
        CHECK(samples_to_copy(48000, 0, NFRAMES, 0) == NFRAMES);
}

TEST_CASE("copying is limited by what remains of the stimulus") {
        // 100 frames left of the stimulus, a whole period available
        CHECK(samples_to_copy(48000, 47900, NFRAMES, 0) == 100);
}

TEST_CASE("copying is limited by what remains of the period") {
        // the stimulus starts partway into the period
        CHECK(samples_to_copy(48000, 0, NFRAMES, 900) == NFRAMES - 900);
}

TEST_CASE("nothing is copied once the stimulus is exhausted") {
        CHECK(samples_to_copy(48000, 48000, NFRAMES, 0) == 0);
}

TEST_CASE("nothing is copied when the onset is past the end of the period") {
        // guards a subtraction that would otherwise wrap
        CHECK(samples_to_copy(48000, 0, NFRAMES, NFRAMES) == 0);
        CHECK(samples_to_copy(48000, 0, NFRAMES, NFRAMES + 500) == 0);
}
