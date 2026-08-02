/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for the mirrored memory allocator and the ringbuffers built on it.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <unistd.h>
#include <cstring>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "jill/util/mirrored_memory.hh"
#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/block_ringbuffer.hh"

namespace {

const std::size_t BUFSIZE = 4096;

/* deterministic filler, so a failure is reproducible */
template <typename T>
std::vector<T> random_values(std::size_t n, unsigned seed = 12345)
{
        std::mt19937 rng(seed);
        std::uniform_int_distribution<int> dist(0, 255);
        std::vector<T> out(n);
        for (std::size_t i = 0; i < n; ++i) {
                out[i] = static_cast<T>(dist(rng));
        }
        return out;
}

/* the channel names the block ringbuffer is keyed on. Built with a stream
 * rather than sprintf: the previous version passed a size_t to a %d
 * conversion, which is undefined behaviour on 64-bit platforms. */
std::string channel_name(std::size_t i)
{
        std::ostringstream os;
        os << "chan_" << std::setw(3) << std::setfill('0') << i;
        return os.str();
}

}

TEST_CASE("mirrored_memory allocates at least what was asked for") {
        jill::util::mirrored_memory m(BUFSIZE, 4);
        const std::size_t page_size = static_cast<std::size_t>(getpagesize());

        // the contract is "at least req_size, rounded up to a page multiple" --
        // not equality. On platforms with 16K pages (Apple Silicon) a 4K
        // request yields a 16K buffer.
        CHECK(m.size() >= BUFSIZE);
        CHECK(m.size() % page_size == 0);
        CHECK(m.buffer() != nullptr);
}

TEST_CASE("mirrored_memory maps the buffer twice, contiguously") {
        jill::util::mirrored_memory m(BUFSIZE, 4);
        const std::vector<char> data = random_values<char>(m.size());

        memcpy(m.buffer(), data.data(), m.size());
        CHECK(memcmp(m.buffer(), m.buffer() + m.size(), m.size()) == 0);

        SUBCASE("a write through the low mapping is visible in the high one") {
                m.buffer()[0] = 0x5a;
                CHECK(m.buffer()[m.size()] == 0x5a);
        }

        SUBCASE("a write through the high mapping is visible in the low one") {
                m.buffer()[m.size() + 7] = 0x3c;
                CHECK(m.buffer()[7] == 0x3c);
        }
}

TEST_CASE_TEMPLATE("ringbuffer round-trips data", T, char, float, jill::sample_t) {
        jill::dsp::ringbuffer<T> rb(BUFSIZE);
        REQUIRE(rb.size() >= BUFSIZE);
        REQUIRE(rb.read_space() == 0);
        REQUIRE(rb.write_space() == rb.size());

        const std::vector<T> data = random_values<T>(BUFSIZE);
        std::vector<T> readback(BUFSIZE);

        std::size_t chunksize = BUFSIZE / 2;
        std::size_t reps = 3;
        SUBCASE("half-buffer chunks") { chunksize = BUFSIZE / 2; reps = 3; }
        SUBCASE("chunks that do not divide the buffer") { chunksize = BUFSIZE / 3 + 5; reps = 5; }
        SUBCASE("single item") { chunksize = 1; reps = 4; }

        // repeating the push/pop cycle walks the pointers around the buffer,
        // which is the case the mirroring exists to handle
        for (std::size_t i = 0; i < reps; ++i) {
                CHECK(rb.push(data.data(), chunksize) == chunksize);
                CHECK(rb.read_space() == chunksize);
                CHECK(rb.write_space() == rb.size() - chunksize);

                CHECK(rb.pop(readback.data(), chunksize) == chunksize);
                CHECK(rb.read_space() == 0);
                CHECK(rb.write_space() == rb.size());
                CHECK(memcmp(readback.data(), data.data(), chunksize * sizeof(T)) == 0);
        }
}

TEST_CASE("ringbuffer will not write past its capacity") {
        jill::dsp::ringbuffer<char> rb(BUFSIZE);
        const std::vector<char> data = random_values<char>(rb.size() * 2);

        const std::size_t written = rb.push(data.data(), rb.size() * 2);
        CHECK(written == rb.size());
        CHECK(rb.write_space() == 0);
        CHECK(rb.read_space() == rb.size());
}

TEST_CASE("block_ringbuffer starts empty") {
        jill::dsp::block_ringbuffer rb(BUFSIZE * sizeof(jill::sample_t) * 5);
        CHECK(rb.peek() == nullptr);
        CHECK(rb.peek_ahead() == nullptr);
}

TEST_CASE("block_ringbuffer stores one block per channel") {
        const std::size_t nchannels = 3;
        const std::size_t data_bytes = BUFSIZE * sizeof(jill::sample_t);

        jill::dsp::block_ringbuffer rb(data_bytes * nchannels * 5);
        const std::vector<jill::sample_t> data = random_values<jill::sample_t>(BUFSIZE);

        std::size_t write_space = rb.write_space();
        for (std::size_t chan = 0; chan < nchannels; ++chan) {
                const std::string name = channel_name(chan);
                const std::size_t bytes =
                        rb.push(0, jill::SAMPLED, name.c_str(), data_bytes, data.data());
                CHECK(bytes > data_bytes);      // header plus id plus payload
                write_space -= bytes;
                CHECK(rb.write_space() == write_space);
        }

        SUBCASE("peek_ahead walks forward without consuming") {
                for (std::size_t chan = 0; chan < nchannels; ++chan) {
                        jill::data_block_t const * info = rb.peek_ahead();
                        REQUIRE(info != nullptr);
                        CHECK(info->time == 0);
                        CHECK(info->dtype == jill::SAMPLED);
                        CHECK(info->sz_data == data_bytes);
                        CHECK(info->nframes() == BUFSIZE);
                        CHECK(info->id() == channel_name(chan));
                        CHECK(memcmp(data.data(), info->data(), info->sz_data) == 0);
                }
                // exhausted, but nothing has been released
                CHECK(rb.peek_ahead() == nullptr);
                REQUIRE(rb.peek() != nullptr);
                CHECK(rb.peek()->id() == channel_name(0));
        }

        SUBCASE("peek is idempotent and release advances") {
                for (std::size_t chan = 0; chan < nchannels; ++chan) {
                        jill::data_block_t const * info = rb.peek();
                        REQUIRE(info != nullptr);
                        CHECK(info->time == 0);
                        CHECK(info->sz_data == data_bytes);
                        CHECK(info->id() == channel_name(chan));
                        CHECK(memcmp(data.data(), info->data(), info->sz_data) == 0);

                        // repeated peeks return the same block
                        CHECK(rb.peek()->id() == channel_name(chan));
                        rb.release();
                }
                CHECK(rb.peek() == nullptr);
        }
}

TEST_CASE("block_ringbuffer preserves block times") {
        const std::size_t data_bytes = 16 * sizeof(jill::sample_t);
        jill::dsp::block_ringbuffer rb(data_bytes * 20);
        const std::vector<jill::sample_t> data = random_values<jill::sample_t>(16);

        for (jill::nframes_t t = 0; t < 4; ++t) {
                rb.push(t * 16, jill::SAMPLED, "pcm", data_bytes, data.data());
        }
        for (jill::nframes_t t = 0; t < 4; ++t) {
                jill::data_block_t const * info = rb.peek();
                REQUIRE(info != nullptr);
                CHECK(info->time == t * 16);
                rb.release();
        }
        CHECK(rb.peek() == nullptr);
}
