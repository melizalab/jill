/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for small pure utilities: the data block header, the stringstream
 * wrapper, and the daily time window used by jtime.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstring>
#include <string>
#include <vector>

// daytime.hh needs only the types; parsing durations from strings needs the
// full posix_time header
#include <boost/date_time/posix_time/posix_time.hpp>

#include "jill/types.hh"
#include "jill/util/string.hh"
#include "jill/util/daytime.hh"

using jill::util::is_daytime;
using jill::util::make_string;
using boost::posix_time::duration_from_string;

namespace {

/* data_block_t is a header that precedes its id and data in one allocation,
 * so a test has to lay the three out contiguously the way the ringbuffer does. */
std::vector<char> pack(jill::nframes_t time, jill::dtype_t dtype,
                       std::string const & id, void const * data, std::size_t sz_data)
{
        std::vector<char> buf(sizeof(jill::data_block_t) + id.size() + sz_data);
        jill::data_block_t header;
        header.time = time;
        header.dtype = dtype;
        header.sz_id = id.size();
        header.sz_data = sz_data;

        memcpy(buf.data(), &header, sizeof(header));
        memcpy(buf.data() + sizeof(header), id.data(), id.size());
        if (sz_data) memcpy(buf.data() + sizeof(header) + id.size(), data, sz_data);
        return buf;
}

}

TEST_CASE("a sampled data block reports its layout") {
        const std::vector<jill::sample_t> samples{0.0f, 0.25f, -0.5f, 1.0f};
        const std::vector<char> packed =
                pack(1234, jill::SAMPLED, "pcm_000", samples.data(),
                     samples.size() * sizeof(jill::sample_t));
        auto const * block = reinterpret_cast<jill::data_block_t const *>(packed.data());

        CHECK(block->time == 1234);
        CHECK(block->dtype == jill::SAMPLED);
        CHECK(block->id() == "pcm_000");
        CHECK(block->sz_data == samples.size() * sizeof(jill::sample_t));
        CHECK(block->nframes() == samples.size());
        CHECK(block->size() == packed.size());
        CHECK(memcmp(block->data(), samples.data(), block->sz_data) == 0);

        SUBCASE("and the typed accessor returns the samples") {
                jill::sample_t const * out = block->data<jill::sample_t>();
                for (std::size_t i = 0; i < samples.size(); ++i) {
                        CAPTURE(i);
                        CHECK(out[i] == samples[i]);
                }
        }
}

TEST_CASE("an event block is always one frame") {
        const std::string message = "stimulus_name";
        const std::vector<char> packed =
                pack(99, jill::EVENT, "trig_in", message.data(), message.size());
        auto const * block = reinterpret_cast<jill::data_block_t const *>(packed.data());

        CHECK(block->dtype == jill::EVENT);
        CHECK(block->id() == "trig_in");
        // event blocks carry a message, not a time series
        CHECK(block->nframes() == 1);
        CHECK(block->sz_data == message.size());
}

TEST_CASE("an empty id and empty payload are handled") {
        const std::vector<char> packed = pack(0, jill::SAMPLED, "", nullptr, 0);
        auto const * block = reinterpret_cast<jill::data_block_t const *>(packed.data());

        CHECK(block->id().empty());
        CHECK(block->nframes() == 0);
        CHECK(block->size() == sizeof(jill::data_block_t));
}

TEST_CASE("make_string builds a string from streamed values") {
        const std::string out = make_string() << "port " << 3 << " at " << 1.5 << " Hz";
        CHECK(out == "port 3 at 1.5 Hz");
}

TEST_CASE("make_string can be built up over several statements") {
        make_string err;
        err << "unable to start client (status=" << 7;
        err << "; couldn't connect to server";
        err << ")";
        const std::string out = err;
        CHECK(out == "unable to start client (status=7; couldn't connect to server)");
}

TEST_CASE("a daytime window that does not wrap") {
        const auto start = duration_from_string("06:00:00");
        const auto stop = duration_from_string("22:00:00");

        CHECK(is_daytime(start, stop, duration_from_string("12:00:00")));
        CHECK(is_daytime(start, stop, duration_from_string("06:00:01")));
        CHECK_FALSE(is_daytime(start, stop, duration_from_string("05:59:59")));
        CHECK_FALSE(is_daytime(start, stop, duration_from_string("23:00:00")));
        CHECK_FALSE(is_daytime(start, stop, duration_from_string("00:00:00")));

        SUBCASE("the window is half open") {
                // start is outside, stop is inside
                CHECK_FALSE(is_daytime(start, stop, start));
                CHECK(is_daytime(start, stop, stop));
        }
}

TEST_CASE("a daytime window that wraps past midnight") {
        const auto start = duration_from_string("22:00:00");
        const auto stop = duration_from_string("06:00:00");

        CHECK(is_daytime(start, stop, duration_from_string("23:00:00")));
        CHECK(is_daytime(start, stop, duration_from_string("00:00:00")));
        CHECK(is_daytime(start, stop, duration_from_string("03:00:00")));
        CHECK_FALSE(is_daytime(start, stop, duration_from_string("12:00:00")));
        CHECK_FALSE(is_daytime(start, stop, duration_from_string("21:00:00")));

        SUBCASE("the window is half open the same way as the non-wrapping case") {
                // both branches exclude start and include stop, so a window
                // and its complement partition the day with no overlap or gap
                CHECK_FALSE(is_daytime(start, stop, start));
                CHECK(is_daytime(start, stop, stop));
        }
}

TEST_CASE("the default window covers the whole day") {
        // jtime defaults to 00:00:00 through 24:00:00
        const auto start = duration_from_string("00:00:00");
        const auto stop = duration_from_string("24:00:00");

        CHECK(is_daytime(start, stop, duration_from_string("00:00:01")));
        CHECK(is_daytime(start, stop, duration_from_string("12:00:00")));
        CHECK(is_daytime(start, stop, duration_from_string("23:59:59")));
}

TEST_CASE("an empty window covers the whole day") {
        // start == stop takes the wrap-around branch on an empty interval
        const auto t = duration_from_string("09:00:00");
        CHECK(is_daytime(t, t, duration_from_string("09:00:00")));
        CHECK(is_daytime(t, t, duration_from_string("21:00:00")));
}
