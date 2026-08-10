/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for stimulus loading, resampling, and the readahead queue.
 * Sound files are synthesized into a temporary directory, so the suite needs
 * no external fixtures.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <unistd.h>
#include <sndfile.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "jill/file/stimfile.hh"
#include "jill/util/readahead_stimqueue.hh"

namespace fs = std::filesystem;
using jill::file::stimfile;

namespace {

/* A temporary directory that cleans itself up. */
struct temp_dir {
        fs::path path;

        temp_dir()
        {
                static int counter = 0;
                path = fs::temp_directory_path() /
                        ("jill_stimset_" + std::to_string(::getpid()) +
                         "_" + std::to_string(counter++));
                fs::create_directories(path);
        }
        ~temp_dir()
        {
                std::error_code ec;
                fs::remove_all(path, ec);
        }
        temp_dir(temp_dir const &) = delete;
        temp_dir & operator=(temp_dir const &) = delete;
};

/* Write a mono sine wave to a WAV file and return its path. A tone rather than
 * noise, so that resampling has something band-limited to work with. */
std::string write_tone(fs::path const & path, jill::nframes_t nframes,
                       jill::nframes_t samplerate, float freq = 440.0)
{
        SF_INFO info;
        memset(&info, 0, sizeof(info));
        info.samplerate = static_cast<int>(samplerate);
        info.channels = 1;
        info.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;

        SNDFILE * f = sf_open(path.c_str(), SFM_WRITE, &info);
        REQUIRE_MESSAGE(f != nullptr, sf_strerror(nullptr));

        std::vector<float> data(nframes);
        for (jill::nframes_t i = 0; i < nframes; ++i) {
                data[i] = 0.5f * std::sin(2.0 * M_PI * freq * i / samplerate);
        }
        const sf_count_t written = sf_writef_float(f, data.data(), nframes);
        sf_close(f);
        REQUIRE(written == static_cast<sf_count_t>(nframes));
        return path.string();
}

/* Drain a queue, giving up after a budget rather than blocking forever: a
 * stalled queue has to fail the suite, not hang the run. */
std::vector<std::string> drain(jill::util::stimqueue & q, std::size_t expected,
                               std::chrono::milliseconds budget = std::chrono::seconds(10))
{
        std::vector<std::string> seen;
        const auto deadline = std::chrono::steady_clock::now() + budget;
        while (seen.size() < expected && std::chrono::steady_clock::now() < deadline) {
                jill::util::trial const * t = q.head();
                if (t == nullptr) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                }
                seen.push_back(t->stim->name());
                q.release();
        }
        return seen;
}

}

TEST_CASE("a missing stimulus file is reported") {
        CHECK_THROWS_AS(stimfile("/nonexistent/path/to/stimulus.wav"), jill::FileError);
}

TEST_CASE("stimfile reads a file's metadata without loading samples") {
        temp_dir dir;
        const jill::nframes_t nframes = 8000, samplerate = 16000;
        const std::string path = write_tone(dir.path / "tone.wav", nframes, samplerate);

        stimfile f(path);
        CHECK(f.nframes() == nframes);
        CHECK(f.samplerate() == samplerate);
        CHECK(f.duration() == doctest::Approx(0.5));
        CHECK(std::string(f.name()) == "tone");

        // samples are not read until asked for
        CHECK(f.buffer() == nullptr);
}

TEST_CASE("load_samples fills the buffer without altering the rate") {
        temp_dir dir;
        const jill::nframes_t nframes = 4000, samplerate = 8000;
        const std::string path = write_tone(dir.path / "tone.wav", nframes, samplerate);

        stimfile f(path);
        f.load_samples();
        REQUIRE(f.buffer() != nullptr);
        CHECK(f.nframes() == nframes);
        CHECK(f.samplerate() == samplerate);

        // the samples are a half-amplitude tone, so they must be in range and
        // not all silent
        bool any_nonzero = false;
        for (jill::nframes_t i = 0; i < f.nframes(); ++i) {
                REQUIRE(f.buffer()[i] <= 1.0f);
                REQUIRE(f.buffer()[i] >= -1.0f);
                if (f.buffer()[i] != 0.0f) any_nonzero = true;
        }
        CHECK(any_nonzero);
}

TEST_CASE("resampling preserves duration") {
        temp_dir dir;
        const jill::nframes_t nframes = 8000, samplerate = 16000;
        const std::string path = write_tone(dir.path / "tone.wav", nframes, samplerate);

        stimfile f(path);
        const float duration = f.duration();
        REQUIRE(duration == doctest::Approx(0.5));

        for (jill::nframes_t target : {10000u, 20000u, 40000u, 80000u}) {
                CAPTURE(target);
                f.load_samples(target);
                REQUIRE(f.buffer() != nullptr);
                CHECK(f.samplerate() == target);
                // within one frame at the coarser of the two rates
                const float tolerance = 1.0f / std::min(target, samplerate);
                CHECK(std::fabs(f.duration() - duration) < tolerance);
        }
}

TEST_CASE("resampling to the file's own rate is a no-op") {
        temp_dir dir;
        const jill::nframes_t nframes = 2000, samplerate = 8000;
        const std::string path = write_tone(dir.path / "tone.wav", nframes, samplerate);

        stimfile f(path);
        f.load_samples(samplerate);
        CHECK(f.samplerate() == samplerate);
        CHECK(f.nframes() == nframes);
}

TEST_CASE("readahead_stimqueue delivers every stimulus in order") {
        temp_dir dir;
        const jill::nframes_t samplerate = 8000;

        std::vector<std::unique_ptr<stimfile>> owned;
        std::vector<jill::util::trial> playlist;
        for (int i = 0; i < 4; ++i) {
                const std::string name = "stim_" + std::to_string(i);
                const std::string path =
                        write_tone(dir.path / (name + ".wav"), 800 * (i + 1), samplerate);
                owned.push_back(std::make_unique<stimfile>(path));
                playlist.push_back(jill::util::trial{owned.back().get(), false});
        }

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), samplerate);
        const std::vector<std::string> seen = drain(queue, playlist.size());
        queue.stop();
        queue.join();

        REQUIRE(seen.size() == playlist.size());
        for (std::size_t i = 0; i < playlist.size(); ++i) {
                CHECK(seen[i] == playlist[i].stim->name());
        }
}

TEST_CASE("an exhausted queue reports nothing more") {
        temp_dir dir;
        const jill::nframes_t samplerate = 8000;
        const std::string path = write_tone(dir.path / "only.wav", 800, samplerate);

        stimfile f(path);
        std::vector<jill::util::trial> playlist{{&f, false}};

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), samplerate);
        const std::vector<std::string> seen = drain(queue, 1);
        REQUIRE(seen.size() == 1);

        CHECK(queue.head() == nullptr);
        queue.stop();
        queue.join();
}

TEST_CASE("the queue resamples to the rate the consumer asked for") {
        temp_dir dir;
        const jill::nframes_t file_rate = 8000, consumer_rate = 24000;
        const std::string path = write_tone(dir.path / "tone.wav", 800, file_rate);

        stimfile f(path);
        std::vector<jill::util::trial> playlist{{&f, false}};

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), consumer_rate);
        jill::util::trial const * head = nullptr;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (head == nullptr && std::chrono::steady_clock::now() < deadline) {
                head = queue.head();
                if (head == nullptr) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        REQUIRE(head != nullptr);
        CHECK(head->stim->samplerate() == consumer_rate);
        CHECK(head->stim->buffer() != nullptr);

        queue.release();
        queue.stop();
        queue.join();
}

/* The condition flag rides on the playlist entry, not on the stimulus, because
 * several entries alias one stimulus. These pin that it survives the handoff
 * and that a stimulus can appear both marked and unmarked. */

TEST_CASE("previous() reports the trial that was just released") {
        temp_dir dir;
        const jill::nframes_t samplerate = 8000;
        const std::string a = write_tone(dir.path / "a.wav", 800, samplerate);
        const std::string b = write_tone(dir.path / "b.wav", 1600, samplerate);
        stimfile fa(a), fb(b);
        std::vector<jill::util::trial> playlist{{&fa, true}, {&fb, false}};

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), samplerate);

        CHECK(queue.previous() == nullptr);     // nothing released yet

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (queue.head() == nullptr && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(queue.head() != nullptr);
        queue.release();

        jill::util::trial const * prev = queue.previous();
        REQUIRE(prev != nullptr);
        CHECK(prev->stim->name() == fa.name());
        CHECK(prev->condition);                 // the flag comes along
        queue.stop();
        queue.join();
}

TEST_CASE("head() promotes without waiting for the worker") {
        /* The point of the redesign. release() clears the head and head()
         * promotes the pre-loaded trial itself, so the queue is never
         * momentarily empty while a background thread is scheduled. jstim gives
         * up on its trigger port for the period when head() is null, so such a
         * window discards external triggers rather than delaying them. */
        temp_dir dir;
        const jill::nframes_t samplerate = 8000;
        const std::string path = write_tone(dir.path / "a.wav", 800, samplerate);
        stimfile f(path);
        std::vector<jill::util::trial> playlist{{&f, false}, {&f, false}, {&f, false}};

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), samplerate);

        // wait for the first, which does need the worker to have run once
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (queue.head() == nullptr && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(queue.head() != nullptr);

        /* Now let the worker get ahead, release, and demand the next one
         * immediately -- no sleeping, no retry. The old design cleared _head
         * and signalled, so this would have come back null. */
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        queue.release();
        CHECK(queue.head() != nullptr);

        queue.stop();
        queue.join();
}

TEST_CASE("a queue with no stimuli finishes on its own") {
        std::vector<jill::util::trial> playlist;
        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), 8000);

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!queue.finished() && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        CHECK(queue.finished());
        CHECK(queue.head() == nullptr);
        queue.join();
}

TEST_CASE("finished() waits for the last stimulus to be released") {
        // it is a latch for "the list is done", not "the list was handed out":
        // jstim polls it to decide when playback is over
        temp_dir dir;
        const jill::nframes_t samplerate = 8000;
        const std::string path = write_tone(dir.path / "a.wav", 800, samplerate);
        stimfile f(path);
        std::vector<jill::util::trial> playlist{{&f, false}};

        jill::util::readahead_stimqueue queue(playlist.begin(), playlist.end(), samplerate);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (queue.head() == nullptr && std::chrono::steady_clock::now() < deadline) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        REQUIRE(queue.head() != nullptr);
        CHECK_FALSE(queue.finished());          // still playing

        queue.release();
        const auto deadline2 = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!queue.finished() && std::chrono::steady_clock::now() < deadline2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        CHECK(queue.finished());
        queue.join();
}
