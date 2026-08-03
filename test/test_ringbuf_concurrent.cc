/*
 * JILL - C++ framework for JACK
 *
 * Drives the ringbuffers from two threads at once, which nothing else does.
 *
 * The single-threaded suite in test_ringbuf.cc checks the data structure; this
 * one exists to exercise the handoff the design actually depends on, so that
 * ThreadSanitizer has something to report. The ringbuffers advance their read
 * and write pointers with __sync_add_and_fetch but declare them as plain
 * size_t and read them without synchronization, so a run under
 * `scons debug=1 sanitize=thread` is expected to flag them. That is the point:
 * before this suite existed the most important race in the codebase was
 * invisible to the test run.
 *
 * The contract being exercised is single producer, single consumer: one
 * realtime thread pushing, one disk thread popping.
 *
 * doctest's assertion macros are not thread safe, so every check happens on
 * the main thread. The worker threads only touch the ringbuffer and atomics.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "jill/dsp/ringbuffer.hh"
#include "jill/dsp/block_ringbuffer.hh"

using jill::nframes_t;
using jill::sample_t;

namespace {

/* Enough traffic to wrap the buffer many times over, but still quick. A soak
 * run (pytest --soak=N) repeats the whole binary, which is the intended way to
 * hunt for something intermittent. */
const int TOTAL_ITEMS = 200000;
const std::size_t CAPACITY = 4096;

/* Give up rather than hang if the other thread stops making progress. A
 * deadlocked ringbuffer must fail the suite, not wedge the run. */
const auto BUDGET = std::chrono::seconds(30);

bool past(std::chrono::steady_clock::time_point deadline)
{
        return std::chrono::steady_clock::now() > deadline;
}

}

TEST_CASE("ringbuffer round-trips a stream between two threads") {
        jill::dsp::ringbuffer<int> rb(CAPACITY);
        REQUIRE(rb.size() >= CAPACITY);

        std::size_t chunk_size = 256;
        SUBCASE("chunks that divide the buffer") { chunk_size = 256; }
        SUBCASE("chunks that do not divide the buffer") { chunk_size = 173; }
        SUBCASE("single items") { chunk_size = 1; }

        std::atomic<bool> producer_stalled(false);

        // producer: a monotonically increasing sequence, so the consumer can
        // detect a gap, a duplicate, or a reordering with one comparison
        std::thread producer([&rb, chunk_size, &producer_stalled] {
                std::vector<int> chunk(chunk_size);
                int next = 0;
                const auto deadline = std::chrono::steady_clock::now() + BUDGET;
                while (next < TOTAL_ITEMS) {
                        if (past(deadline)) { producer_stalled = true; return; }
                        const std::size_t want =
                                std::min<std::size_t>(chunk_size, TOTAL_ITEMS - next);
                        for (std::size_t i = 0; i < want; ++i) {
                                chunk[i] = next + static_cast<int>(i);
                        }
                        const std::size_t wrote = rb.push(chunk.data(), want);
                        next += static_cast<int>(wrote);
                        if (wrote == 0) std::this_thread::yield();
                }
        });

        // consumer runs here so the assertions are on the main thread
        std::vector<int> out(512);
        int expected = 0;
        int first_bad_value = -1;
        int first_bad_index = -1;
        const auto deadline = std::chrono::steady_clock::now() + BUDGET;

        while (expected < TOTAL_ITEMS && !past(deadline)) {
                const std::size_t want =
                        std::min<std::size_t>(out.size(), TOTAL_ITEMS - expected);
                const std::size_t got = rb.pop(out.data(), want);
                if (got == 0) { std::this_thread::yield(); continue; }
                for (std::size_t i = 0; i < got; ++i) {
                        if (out[i] != expected && first_bad_index < 0) {
                                first_bad_index = expected;
                                first_bad_value = out[i];
                        }
                        ++expected;
                }
        }
        producer.join();

        CAPTURE(chunk_size);
        CHECK_FALSE(producer_stalled);
        CHECK(expected == TOTAL_ITEMS);         // nothing lost, nothing extra
        CAPTURE(first_bad_index);
        CAPTURE(first_bad_value);
        CHECK(first_bad_index == -1);           // nothing reordered or corrupted
        CHECK(rb.read_space() == 0);
        CHECK(rb.write_space() == rb.size());
}

TEST_CASE("ringbuffer never reports more space than it has") {
        // read_space and write_space are computed from two pointers written by
        // different threads. If either read is torn or stale the totals go out
        // of range, which is easier to detect than a subtly wrong value.
        jill::dsp::ringbuffer<int> rb(CAPACITY);
        std::atomic<bool> stop(false);
        std::atomic<bool> out_of_range(false);

        std::thread producer([&rb, &stop] {
                std::vector<int> chunk(64, 7);
                while (!stop) {
                        rb.push(chunk.data(), chunk.size());
                        std::this_thread::yield();
                }
        });

        std::vector<int> sink(64);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!past(deadline)) {
                const std::size_t r = rb.read_space();
                const std::size_t w = rb.write_space();
                if (r > rb.size() || w > rb.size() || r + w != rb.size()) {
                        out_of_range = true;
                        break;
                }
                rb.pop(sink.data(), std::min<std::size_t>(sink.size(), r));
        }
        stop = true;
        producer.join();

        CHECK_FALSE(out_of_range);
}

TEST_CASE("block_ringbuffer round-trips periods between two threads") {
        const std::size_t FRAMES = 64;
        const std::size_t DATA_BYTES = FRAMES * sizeof(sample_t);
        const int TOTAL_PERIODS = 20000;

        // room for a handful of periods, so the producer regularly has to wait
        jill::dsp::block_ringbuffer rb(DATA_BYTES * 16);
        std::atomic<bool> producer_stalled(false);

        std::thread producer([&rb, &producer_stalled, FRAMES, DATA_BYTES, TOTAL_PERIODS] {
                std::vector<sample_t> payload(FRAMES);
                int next = 0;
                const auto deadline = std::chrono::steady_clock::now() + BUDGET;
                while (next < TOTAL_PERIODS) {
                        if (past(deadline)) { producer_stalled = true; return; }
                        // stamp the sequence number into every sample so a torn
                        // or stale read shows up in the payload as well as the
                        // block header
                        std::fill(payload.begin(), payload.end(),
                                  static_cast<sample_t>(next));
                        const std::size_t wrote =
                                rb.push(static_cast<nframes_t>(next) * FRAMES,
                                        jill::SAMPLED, "pcm", DATA_BYTES, payload.data());
                        if (wrote == 0) { std::this_thread::yield(); continue; }
                        ++next;
                }
        });

        int expected = 0;
        int bad_time = -1, bad_payload = -1, bad_id = -1;
        const auto deadline = std::chrono::steady_clock::now() + BUDGET;

        while (expected < TOTAL_PERIODS && !past(deadline)) {
                jill::data_block_t const * block = rb.peek();
                if (block == nullptr) { std::this_thread::yield(); continue; }
                if (block->time != static_cast<nframes_t>(expected) * FRAMES && bad_time < 0) {
                        bad_time = expected;
                }
                if (block->id() != "pcm" && bad_id < 0) {
                        bad_id = expected;
                }
                auto const * samples = block->data<sample_t>();
                if (samples[0] != static_cast<sample_t>(expected) && bad_payload < 0) {
                        bad_payload = expected;
                }
                rb.release();
                ++expected;
        }
        producer.join();

        CHECK_FALSE(producer_stalled);
        CHECK(expected == TOTAL_PERIODS);
        CAPTURE(bad_time);
        CAPTURE(bad_id);
        CAPTURE(bad_payload);
        CHECK(bad_time == -1);
        CHECK(bad_id == -1);
        CHECK(bad_payload == -1);
        CHECK(rb.peek() == nullptr);
}

TEST_CASE("a consumer that starts late still sees every period") {
        // the producer fills the buffer completely before the consumer runs,
        // which is the state the writer thread finds after any pause
        const std::size_t FRAMES = 32;
        const std::size_t DATA_BYTES = FRAMES * sizeof(sample_t);
        jill::dsp::block_ringbuffer rb(DATA_BYTES * 8);

        std::vector<sample_t> payload(FRAMES, 1.0f);
        int pushed = 0;
        while (rb.push(static_cast<nframes_t>(pushed) * FRAMES, jill::SAMPLED,
                       "pcm", DATA_BYTES, payload.data()) != 0) {
                ++pushed;
                if (pushed > 1000) break;       // guard against a push that never fails
        }
        REQUIRE(pushed > 0);

        std::atomic<int> drained(0);
        std::thread consumer([&rb, &drained, pushed] {
                while (drained < pushed && rb.peek() != nullptr) {
                        rb.release();
                        drained.fetch_add(1);
                }
        });
        consumer.join();

        CHECK(drained == pushed);
        CHECK(rb.peek() == nullptr);
}
