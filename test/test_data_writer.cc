/*
 * JILL - C++ framework for JACK
 *
 * Tests for the buffered writer's thread lifecycle.
 *
 * buffered_data_writer takes a data_writer by unique_ptr, so a test can supply
 * its own and drive the thread without a JACK server or a file on disk. These
 * cover starting and stopping, which is where a lost stop() used to hang
 * join() forever.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "jill/data_writer.hh"
#include "jill/dsp/buffered_data_writer.hh"

using jill::data_block_t;
using jill::nframes_t;
using jill::sample_t;

namespace {

/* Records what the writer thread was asked to do, so a test can assert on the
 * sequence rather than on side effects. Only touched by the writer thread
 * until it has been joined, after which the main thread reads it. */
class recording_writer : public jill::data_writer {

public:
        struct call {
                std::string what;
                nframes_t frame;
        };

        void new_entry(nframes_t frame) override
        {
                calls.push_back({"new_entry", frame});
                _ready = true;
        }
        void close_entry() override
        {
                calls.push_back({"close_entry", 0});
                _ready = false;
        }
        void xrun() override { calls.push_back({"xrun", 0}); }
        bool ready() const override { return _ready; }
        void write(data_block_t const * data, nframes_t, nframes_t) override
        {
                calls.push_back({"write", data->time});
        }
        void flush() override { ++flushes; }

        std::vector<call> calls;
        int flushes = 0;

private:
        bool _ready = false;
};

std::unique_ptr<jill::dsp::buffered_data_writer>
make_writer(recording_writer ** out = nullptr)
{
        auto sink = std::make_unique<recording_writer>();
        if (out) *out = sink.get();
        return std::make_unique<jill::dsp::buffered_data_writer>(std::move(sink), 4096);
}

/* Fail rather than hang: a lost stop() shows up as join() never returning. */
/* Takes the owning pointer, not a reference, because a writer whose join()
 * never returns can no longer be destroyed safely: the waiter is parked inside
 * it. On timeout the writer is released deliberately, so that the detached
 * waiter keeps a live object to sit in and the caller is left holding nothing
 * to destruct. Leaking it is the point -- the alternative is a second join()
 * from the destructor racing the first, which is undefined behaviour and shows
 * up under ThreadSanitizer as an abort inside its pthread_join interceptor
 * rather than as the test failure it should be. */
void join_within(std::unique_ptr<jill::dsp::buffered_data_writer> & w,
                 std::chrono::seconds budget)
{
        std::atomic<bool> done(false);
        std::thread waiter([&w, &done] { w->join(); done = true; });
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (!done && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const bool finished = done;
        if (!finished) {
                // the thread is wedged; detaching leaks it but lets the suite
                // report the failure instead of hanging the whole run
                waiter.detach();
                w.release();    // deliberate: the waiter is still inside it
        }
        else {
                waiter.join();
        }
        REQUIRE_MESSAGE(finished, "join() did not return: a stop() was lost");
}

}

TEST_CASE("a writer that is never started can still be destroyed") {
        auto w = make_writer();
        // no start(), so no thread: the destructor must cope
        w.reset();
        CHECK(true);
}

TEST_CASE("start then stop terminates the thread") {
        auto w = make_writer();
        w->start();
        w->stop();
        join_within(w, std::chrono::seconds(10));
}

TEST_CASE("a stop arriving immediately after start is not lost") {
        // The thread used to set the state to Running itself, leaving a window
        // where it was still Stopped and a stop() would fail its
        // compare-and-swap and vanish, after which join() hung. Repeat enough
        // times to land inside that window.
        for (int i = 0; i < 200; ++i) {
                CAPTURE(i);
                auto w = make_writer();
                w->start();
                w->stop();              // as tight as possible against start()
                join_within(w, std::chrono::seconds(10));
        }
}

TEST_CASE("a stop from another thread racing startup is not lost") {
        for (int i = 0; i < 100; ++i) {
                CAPTURE(i);
                auto w = make_writer();
                std::thread stopper([&w] { w->stop(); });
                w->start();
                stopper.join();
                join_within(w, std::chrono::seconds(10));
        }
}

TEST_CASE("starting an already running writer is refused") {
        auto w = make_writer();
        w->start();
        CHECK_THROWS_AS(w->start(), std::runtime_error);
        w->stop();
        join_within(w, std::chrono::seconds(10));
}

TEST_CASE("data pushed before stopping reaches the writer") {
        recording_writer * sink = nullptr;
        auto w = make_writer(&sink);
        REQUIRE(sink != nullptr);

        w->start();
        const std::vector<sample_t> samples(64, 0.5f);
        for (nframes_t i = 0; i < 8; ++i) {
                w->push(i * 64, jill::SAMPLED, "pcm",
                        samples.size() * sizeof(sample_t), samples.data());
                w->data_ready();
        }
        w->stop();
        join_within(w, std::chrono::seconds(10));

        // the thread has exited, so reading its record is safe now
        int writes = 0;
        for (auto const & c : sink->calls) {
                if (c.what == "write") ++writes;
        }
        CHECK(writes == 8);
}
