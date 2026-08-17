/*
 * JILL - C++ framework for JACK
 *
 * Writes an ARF file with known contents, for test_arf_format.py to check
 * against the layout documented in doc/arf-files.md.
 *
 * This deliberately does no checking of its own: it is a fixture generator,
 * and the assertions live on the python side where the file structure is far
 * easier to express. It does report progress on stdout, so that if the writer
 * crashes it is obvious how far it got.
 *
 * usage: write_arf_fixture <output.arf>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <boost/date_time/posix_time/posix_time.hpp>

#include "jill/data_source.hh"
#include "jill/data_writer.hh"
#include "jill/midi.hh"
#include "jill/file/arf_writer.hh"

using namespace jill;
using boost::posix_time::microsec_clock;

namespace {

/* Values the python side asserts against. Keep in step with
 * test/test_arf_format.py. */
const nframes_t SAMPLING_RATE = 20000;
const nframes_t PERIOD = 1024;
const int PERIODS_PER_ENTRY = 10;
const char * CHANNELS[] = {"pcm_000", "pcm_001"};
const char * EVENT_CHANNEL = "trig_in";
const char * STIMULUS_NAME = "stim_a";

/* A data source that does not need a JACK server. */
class null_source : public data_source {
public:
        null_source(std::string const & name, nframes_t sampling_rate)
                : _name(name), _sampling_rate(sampling_rate),
                  _base_time(microsec_clock::universal_time()) {}

        char const * name() const override { return _name.c_str(); }
        nframes_t sampling_rate() const override { return _sampling_rate; }
        nframes_t frame() const override { return frame(time()); }
        nframes_t frame(utime_t t) const override { return t / (1000000 / _sampling_rate); }
        utime_t time(nframes_t t) const override { return t * (1000000 / _sampling_rate); }
        utime_t time() const override
        {
                return (microsec_clock::universal_time() - _base_time).total_microseconds();
        }

private:
        std::string _name;
        nframes_t _sampling_rate;
        boost::posix_time::ptime _base_time;
};

/* Build a data block: header, then the id bytes, then the payload. The id is
 * not null terminated -- sz_id gives its length. */
std::vector<char> make_block(nframes_t time, dtype_t dtype, std::string const & id,
                             void const * payload, std::size_t payload_bytes)
{
        std::vector<char> buf(sizeof(data_block_t) + id.size() + payload_bytes);
        auto * header = reinterpret_cast<data_block_t *>(buf.data());
        header->time = time;
        header->dtype = dtype;
        header->sz_id = id.size();
        header->sz_data = payload_bytes;
        memcpy(buf.data() + sizeof(data_block_t), id.data(), id.size());
        if (payload_bytes) {
                memcpy(buf.data() + sizeof(data_block_t) + id.size(), payload, payload_bytes);
        }
        return buf;
}

/* A ramp over [-1, 1), distinct per channel so the two are told apart. */
std::vector<sample_t> ramp(nframes_t n, int channel)
{
        std::vector<sample_t> out(n);
        const float sign = (channel % 2 == 0) ? 1.0f : -1.0f;
        for (nframes_t i = 0; i < n; ++i) {
                out[i] = sign * (2.0f * i / n - 1.0f);
        }
        return out;
}

/* Write one entry's worth of sampled data on every channel.
 *
 * If event_after_period is not negative, a stimulus-onset event is emitted at
 * that period's timestamp. */
void write_periods(data_writer & writer, nframes_t start, int periods,
                   int event_after_period = -1)
{
        for (int p = 0; p < periods; ++p) {
                const nframes_t t = start + p * PERIOD;
                for (int c = 0; c < 2; ++c) {
                        const std::vector<sample_t> samples = ramp(PERIOD, c);
                        std::vector<char> block =
                                make_block(t, SAMPLED, CHANNELS[c], samples.data(),
                                           samples.size() * sizeof(sample_t));
                        writer.write(reinterpret_cast<data_block_t const *>(block.data()), 0, 0);
                }
                if (p == event_after_period) {
                        // an event payload is the status byte followed by the
                        // message, which for a stimulus is its utf-8 name
                        std::vector<char> payload;
                        payload.push_back(static_cast<char>(midi::status_type::stim_on));
                        const std::string stim = STIMULUS_NAME;
                        payload.insert(payload.end(), stim.begin(), stim.end());
                        std::vector<char> block =
                                make_block(t, EVENT, EVENT_CHANNEL,
                                           payload.data(), payload.size());
                        writer.write(reinterpret_cast<data_block_t const *>(block.data()), 0, 0);
                }
        }
}

}

int
main(int argc, char ** argv)
{
        if (argc < 2) {
                std::cerr << "usage: write_arf_fixture <output.arf>" << std::endl;
                return 2;
        }
        const std::string path = argv[1];

        std::map<std::string, std::string> attrs;
        attrs["experimenter"] = "dmeliza";
        attrs["experiment"] = "arf format fixture";

        null_source source("fixture", SAMPLING_RATE);
        std::cout << "creating " << path << std::endl;
        std::unique_ptr<data_writer> writer(
                new file::arf_writer(path, source, attrs, 0));

        std::cout << "writing log message" << std::endl;
        writer->log(microsec_clock::universal_time(), "fixture", "a log message");

        // first entry: sampled data on two channels, with a stimulus event
        // interleaved partway through
        std::cout << "entry 0" << std::endl;
        writer->new_entry(1000);
        write_periods(*writer, 1000, PERIODS_PER_ENTRY, 2);
        writer->close_entry();

        // second entry, to check that numbering advances
        std::cout << "entry 1" << std::endl;
        writer->new_entry(100000);
        write_periods(*writer, 100000, 2);
        writer->xrun();
        writer->close_entry();

        // third entry, started just below the 32-bit frame counter wrap. The
        // writer is expected to notice the counter going backwards and split
        // into a further entry, which is what keeps multi-day recordings sane.
        std::cout << "entry 2 (frame counter wraps)" << std::endl;
        const nframes_t near_wrap = 0xffffffffu - 3 * PERIOD;
        writer->new_entry(near_wrap);
        write_periods(*writer, near_wrap, 6);
        writer->close_entry();

        /* An event stamped before data already written. arf_writer used to
         * record the end of an entry as the last write rather than the
         * furthest one, so this dragged trial_off backwards; the entry is two
         * periods long whichever order the writes arrive in. */
        writer->new_entry(200000);
        write_periods(*writer, 200000, 2);
        {
                std::vector<char> payload;
                payload.push_back(static_cast<char>(midi::status_type::stim_on));
                const std::string stim = STIMULUS_NAME;
                payload.insert(payload.end(), stim.begin(), stim.end());
                std::vector<char> block = make_block(200000, EVENT, EVENT_CHANNEL,
                                                     payload.data(), payload.size());
                writer->write(reinterpret_cast<data_block_t const *>(block.data()), 0, 0);
        }
        writer->close_entry();

        writer->flush();
        std::cout << "done" << std::endl;
        return 0;
}
