/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for the pulse rendering extracted from jclicker.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <vector>

#include "jill/dsp/pulse.hh"

using jill::sample_t;
using jill::dsp::pulse_shape;
using jill::dsp::render_pulse;

namespace {

/* Render into the middle of a zeroed buffer, so that writing outside the
 * requested region would be visible. */
const std::size_t PAD = 4;

std::vector<sample_t> rendered(pulse_shape shape, jill::nframes_t duration)
{
        std::vector<sample_t> buf(2 * PAD + duration, 0.0f);
        render_pulse(buf.data() + PAD, shape, duration);
        return buf;
}

}

TEST_CASE("a positive pulse is full scale throughout") {
        const auto buf = rendered(pulse_shape::positive, 5);
        for (std::size_t i = 0; i < 5; ++i) {
                CAPTURE(i);
                CHECK(buf[PAD + i] == 1.0f);
        }
}

TEST_CASE("a negative pulse is full scale negative throughout") {
        const auto buf = rendered(pulse_shape::negative, 5);
        for (std::size_t i = 0; i < 5; ++i) {
                CAPTURE(i);
                CHECK(buf[PAD + i] == -1.0f);
        }
}

TEST_CASE("nothing is written outside the requested duration") {
        for (auto shape : {pulse_shape::positive, pulse_shape::negative,
                           pulse_shape::biphasic}) {
                const auto buf = rendered(shape, 6);
                for (std::size_t i = 0; i < PAD; ++i) {
                        CHECK(buf[i] == 0.0f);
                        CHECK(buf[buf.size() - 1 - i] == 0.0f);
                }
        }
}

TEST_CASE("an even biphasic pulse is symmetric") {
        const auto buf = rendered(pulse_shape::biphasic, 6);
        CHECK(buf[PAD + 0] == 1.0f);
        CHECK(buf[PAD + 1] == 1.0f);
        CHECK(buf[PAD + 2] == 1.0f);
        CHECK(buf[PAD + 3] == -1.0f);
        CHECK(buf[PAD + 4] == -1.0f);
        CHECK(buf[PAD + 5] == -1.0f);

        SUBCASE("and so sums to zero") {
                sample_t sum = 0;
                for (std::size_t i = 0; i < 6; ++i) sum += buf[PAD + i];
                CHECK(sum == 0.0f);
        }
}

TEST_CASE("an odd biphasic pulse is balanced by an interphase gap") {
        // duration / 2 used to round down and give the remainder to the
        // negative phase, leaving a net negative sample at the electrode --
        // exactly what a biphasic pulse is meant to avoid.
        const auto buf = rendered(pulse_shape::biphasic, 5);
        CHECK(buf[PAD + 0] == 1.0f);
        CHECK(buf[PAD + 1] == 1.0f);
        CHECK(buf[PAD + 2] == 0.0f);       // the gap
        CHECK(buf[PAD + 3] == -1.0f);
        CHECK(buf[PAD + 4] == -1.0f);

        sample_t sum = 0;
        for (std::size_t i = 0; i < 5; ++i) sum += buf[PAD + i];
        CHECK(sum == 0.0f);
}

TEST_CASE("biphasic pulses of every length are charge balanced") {
        for (jill::nframes_t duration = 0; duration <= 33; ++duration) {
                CAPTURE(duration);
                const auto buf = rendered(pulse_shape::biphasic, duration);
                sample_t sum = 0;
                for (std::size_t i = 0; i < duration; ++i) sum += buf[PAD + i];
                CHECK(sum == 0.0f);
        }
}

TEST_CASE("a one-sample biphasic pulse is silence") {
        // Two phases do not fit in one sample. This renders as the gap alone;
        // jclicker rejects the configuration rather than emit nothing.
        const auto buf = rendered(pulse_shape::biphasic, 1);
        CHECK(buf[PAD] == 0.0f);
}

TEST_CASE("a zero duration pulse writes nothing") {
        for (auto shape : {pulse_shape::positive, pulse_shape::negative,
                           pulse_shape::biphasic}) {
                const auto buf = rendered(shape, 0);
                for (auto v : buf) CHECK(v == 0.0f);
        }
}

TEST_CASE("a two-sample biphasic pulse is one sample either way") {
        const auto buf = rendered(pulse_shape::biphasic, 2);
        CHECK(buf[PAD + 0] == 1.0f);
        CHECK(buf[PAD + 1] == -1.0f);
}

TEST_CASE("pulses stay within the range the format allows") {
        // doc/arf-files.md bounds sampled values to [-1, 1]
        for (auto shape : {pulse_shape::positive, pulse_shape::negative,
                           pulse_shape::biphasic}) {
                for (jill::nframes_t d : {1u, 2u, 7u, 64u}) {
                        const auto buf = rendered(shape, d);
                        for (auto v : buf) {
                                CHECK(v >= -1.0f);
                                CHECK(v <= 1.0f);
                        }
                }
        }
}
