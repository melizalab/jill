/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for the triggered_data_writer state machine.
 *
 * This is the one piece of the writing path that had no coverage, and the
 * reason is the shape of the class: it inherits a consumer thread from
 * buffered_data_writer, and that thread is what normally calls write(). A test
 * that started the thread could only prod it and wait, which is how flaky
 * tests are made.
 *
 * So the thread is never started here. Blocks are pushed into the inherited
 * ringbuffer and write() is called directly, through the
 * triggered_data_writer_test friend the header already declared. The state
 * machine then runs one step at a time with nothing else moving.
 *
 * The sink is a double that records the calls it receives, so the assertions
 * are about what the writer *asked for* rather than about bytes in a file.
 * The prebuffer walk in start_recording() is the part worth checking: it
 * discards periods that are too old, writes a partial period for the one that
 * straddles the onset, and writes whole periods after that.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <memory>
#include <string>
#include <vector>

#include "jill/midi.hh"
#include "jill/data_writer.hh"
#include "jill/dsp/block_ringbuffer.hh"
#include "jill/dsp/triggered_data_writer.hh"

using namespace jill;
using jill::dsp::triggered_data_writer;

namespace {

constexpr char TRIGGER_PORT[] = "trig_in";
constexpr char DATA_PORT[] = "data";
constexpr nframes_t PERIOD = 64;

/* A sink that records what it was asked to do. */
struct recording_writer : public data_writer {
        struct call {
                std::string kind;
                std::string id;
                nframes_t time = 0;
                nframes_t start = 0;
                nframes_t stop = 0;
        };

        std::vector<call> calls;
        /* Models arf_writer: ready() reports whether an entry is open, an
         * entry opens lazily on the first write, and close_entry() ends it.
         * Getting this wrong matters -- a sink that claims to be ready before
         * anything has been recorded sends write() down its posttrigger
         * branch, and the prebuffer is never exercised at all. */
        bool entry_open = false;

        bool ready() const override { return entry_open; }
        void new_entry(nframes_t frame) override
        {
                entry_open = true;
                calls.push_back({"new_entry", "", frame});
        }
        void close_entry() override
        {
                entry_open = false;
                calls.push_back({"close_entry", ""});
        }
        void xrun() override { calls.push_back({"xrun", ""}); }

        void write(data_block_t const * data, nframes_t start, nframes_t stop) override
        {
                entry_open = true;
                calls.push_back({"write", data->id(), data->time, start, stop});
        }

        /** The write calls only, which is what most assertions are about. */
        std::vector<call> writes() const
        {
                std::vector<call> out;
                for (call const & c : calls)
                        if (c.kind == "write") out.push_back(c);
                return out;
        }

        std::size_t count(std::string const & kind) const
        {
                std::size_t n = 0;
                for (call const & c : calls) n += (c.kind == kind);
                return n;
        }
};

} // namespace

/* Declared a friend by triggered_data_writer, so this is how the test reaches
 * write() and the inherited buffer without the consumer thread running. */
namespace jill { namespace dsp {

class triggered_data_writer_test {
public:
        static void step(triggered_data_writer & w, data_block_t const * d) { w.write(d); }
        static bool recording(triggered_data_writer const & w) { return w._recording; }
        static block_ringbuffer & buffer(triggered_data_writer & w) { return *w._buffer; }
};

}}

using jill::dsp::triggered_data_writer_test;

namespace {

/* A fixture holding the writer, the sink it reports to, and helpers for
 * pushing periods. The sink is owned by the writer, so keep a raw pointer to
 * look at afterwards -- it outlives every use here. */
struct harness {
        recording_writer * sink;
        std::unique_ptr<triggered_data_writer> writer;
        nframes_t now = 0;

        harness(nframes_t pretrigger, nframes_t posttrigger)
        {
                auto owned = std::make_unique<recording_writer>();
                sink = owned.get();
                writer = std::make_unique<triggered_data_writer>(
                        std::move(owned), TRIGGER_PORT, pretrigger, posttrigger);
        }

        /** Push one period of samples and let the writer act on it. */
        void push_data()
        {
                const std::vector<sample_t> samples(PERIOD, 0.5f);
                auto & buf = triggered_data_writer_test::buffer(*writer);
                buf.push(now, SAMPLED, DATA_PORT, PERIOD * sizeof(sample_t),
                         samples.data());
                now += PERIOD;
        }

        /** Push a period of samples without stepping the writer, to fill the
         *  prebuffer the way the realtime thread would. */
        void fill(int periods)
        {
                for (int i = 0; i < periods; ++i) push_data();
        }

        /** Hand the oldest unprocessed block to write(). */
        void step()
        {
                auto & buf = triggered_data_writer_test::buffer(*writer);
                data_block_t const * hdr = buf.peek_ahead();
                REQUIRE(hdr != nullptr);
                triggered_data_writer_test::step(*writer, hdr);
        }

        /** Push a MIDI trigger event at the current time and step it through. */
        void trigger(midi::status_type status)
        {
                const midi::data_type message[1] = { status.value() };
                auto & buf = triggered_data_writer_test::buffer(*writer);
                buf.push(now, EVENT, TRIGGER_PORT, sizeof(message), message);
                // the event block is what write() must act on
                data_block_t const * hdr = buf.peek_ahead();
                REQUIRE(hdr != nullptr);
                triggered_data_writer_test::step(*writer, hdr);
        }

        bool recording() const { return triggered_data_writer_test::recording(*writer); }
};

} // namespace

TEST_CASE("a new triggered writer is not recording") {
        harness h(PERIOD * 2, PERIOD * 2);
        CHECK_FALSE(h.recording());
        CHECK(h.sink->calls.empty());
}

TEST_CASE("data before a trigger is buffered, not written") {
        harness h(PERIOD * 2, PERIOD * 2);
        h.fill(4);
        for (int i = 0; i < 4; ++i) h.step();
        CHECK_FALSE(h.recording());
        CHECK(h.sink->writes().empty());
}

TEST_CASE("an onset trigger starts recording") {
        harness h(PERIOD * 2, PERIOD * 2);
        h.fill(4);
        for (int i = 0; i < 4; ++i) h.step();
        h.trigger(midi::status_type::note_on);
        CHECK(h.recording());
}

TEST_CASE("the prebuffer walk writes exactly the pretrigger window") {
        /* Four periods buffered and a pretrigger of two, so the onset falls on
         * the boundary between period 1 and period 2: periods 0 and 1 are too
         * old and must be discarded, 2 and 3 must be written whole. */
        const nframes_t pretrigger = PERIOD * 2;
        harness h(pretrigger, PERIOD * 2);
        h.fill(4);
        for (int i = 0; i < 4; ++i) h.step();
        const nframes_t onset_at = h.now;
        h.trigger(midi::status_type::note_on);

        /* The trigger event block is written too, so that the entry records
         * what opened it; count only the sampled channel here. */
        std::vector<recording_writer::call> data_writes;
        for (auto const & w : h.sink->writes())
                if (w.id == DATA_PORT) data_writes.push_back(w);

        const nframes_t onset = onset_at - pretrigger;
        REQUIRE(!data_writes.empty());
        // nothing from before the onset survives
        for (auto const & w : data_writes) {
                CHECK(w.time + PERIOD > onset);
        }
        // and the periods after the onset are written whole and in order
        for (std::size_t i = 1; i < data_writes.size(); ++i) {
                CHECK(data_writes[i].start == 0);
                CHECK(data_writes[i].time == data_writes[i - 1].time + PERIOD);
        }
        // the newest buffered period reaches the trigger
        CHECK(data_writes.back().time + PERIOD == onset_at);
}

TEST_CASE("a period straddling the onset is written from the onset") {
        /* A pretrigger that is not a multiple of the period puts the onset
         * inside a period rather than on a boundary. That period must be
         * written from the onset onwards, which is the `start` argument, and
         * getting it wrong silently shifts every triggered recording. */
        const nframes_t pretrigger = PERIOD + PERIOD / 2;
        harness h(pretrigger, PERIOD * 2);
        h.fill(4);
        for (int i = 0; i < 4; ++i) h.step();
        const nframes_t onset_at = h.now;
        h.trigger(midi::status_type::note_on);

        const auto writes = h.sink->writes();
        REQUIRE(writes.size() >= 1);
        const nframes_t onset = onset_at - pretrigger;
        // the first write covers the period the onset falls inside
        CHECK(writes[0].time <= onset);
        CHECK(writes[0].time + PERIOD > onset);
        CHECK(writes[0].start == onset - writes[0].time);
        CHECK(writes[0].start == PERIOD / 2);
        // everything after it is written whole
        for (std::size_t i = 1; i < writes.size(); ++i) {
                CHECK(writes[i].start == 0);
        }
}

TEST_CASE("a pretrigger longer than the buffered data writes what there is") {
        /* The comment on start_recording() says the onset may not be in the
         * buffer if it has not had time to fill. Asking for more history than
         * exists must not walk off the end. */
        harness h(PERIOD * 100, PERIOD * 2);
        h.fill(2);
        for (int i = 0; i < 2; ++i) h.step();
        h.trigger(midi::status_type::note_on);

        CHECK(h.recording());
        /* Both buffered periods are kept rather than discarded: asking for
         * more history than exists must yield what there is. Before the walk
         * was made wrap-safe this segfaulted, because onset underflowed and
         * every period looked older than it. */
        std::size_t data_writes = 0;
        for (auto const & w : h.sink->writes())
                if (w.id == DATA_PORT) ++data_writes;
        CHECK(data_writes == 2);
}

TEST_CASE("data after the trigger is written whole") {
        harness h(PERIOD * 2, PERIOD * 2);
        h.fill(2);
        for (int i = 0; i < 2; ++i) h.step();
        h.trigger(midi::status_type::note_on);
        const std::size_t before = h.sink->writes().size();

        h.push_data();
        h.step();
        const auto writes = h.sink->writes();
        REQUIRE(writes.size() == before + 1);
        CHECK(writes.back().start == 0);
        CHECK(writes.back().stop == 0);
}

TEST_CASE("an offset trigger stops recording but keeps writing posttrigger data") {
        const nframes_t posttrigger = PERIOD * 2;
        harness h(PERIOD, posttrigger);
        h.fill(2);
        for (int i = 0; i < 2; ++i) h.step();
        h.trigger(midi::status_type::note_on);
        REQUIRE(h.recording());

        h.trigger(midi::status_type::note_off);
        CHECK_FALSE(h.recording());

        /* Recording is off, but the entry stays open until the posttrigger
         * window has passed -- that is the whole point of the flag being
         * separate from the entry. */
        const std::size_t at_offset = h.sink->writes().size();
        h.push_data();
        h.step();
        CHECK(h.sink->writes().size() == at_offset + 1);
        CHECK(h.sink->count("close_entry") == 0);
}

TEST_CASE("the entry closes once the posttrigger window has passed") {
        const nframes_t posttrigger = PERIOD;
        harness h(PERIOD, posttrigger);
        h.fill(2);
        for (int i = 0; i < 2; ++i) h.step();
        h.trigger(midi::status_type::note_on);
        h.trigger(midi::status_type::note_off);

        // step past the posttrigger window
        for (int i = 0; i < 4; ++i) {
                h.push_data();
                h.step();
        }
        CHECK(h.sink->count("close_entry") >= 1);
}

TEST_CASE("an onset while already recording does not restart the entry") {
        harness h(PERIOD, PERIOD * 2);
        h.fill(2);
        for (int i = 0; i < 2; ++i) h.step();
        h.trigger(midi::status_type::note_on);
        std::size_t data_after_first = 0;
        for (auto const & w : h.sink->writes())
                if (w.id == DATA_PORT) ++data_after_first;

        h.trigger(midi::status_type::note_on);
        CHECK(h.recording());
        /* The second onset writes its own event block, because everything is
         * recorded once the entry is open, but it must not walk the prebuffer
         * again -- that would splice already-written audio back in. */
        std::size_t data_after_second = 0;
        for (auto const & w : h.sink->writes())
                if (w.id == DATA_PORT) ++data_after_second;
        CHECK(data_after_second == data_after_first);
}
