/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for MIDI status bytes and message encoding. The status values and
 * the encoding rules are part of the on-disk format, so these tests pin what
 * doc/arf-files.md documents.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <sstream>
#include <string>

#include "jill/midi.hh"

using jill::midi::status_type;

TEST_CASE("status values match the documented ranges") {
        // doc/arf-files.md: 0-15 stimulus on, 16-31 stimulus off,
        // 128-143 note on, 144-159 note off
        CHECK(status_type(status_type::stim_on).value() == 0);
        CHECK(status_type(status_type::stim_off).value() == 16);
        CHECK(status_type(status_type::note_on).value() == 128);
        CHECK(status_type(status_type::note_off).value() == 144);
}

TEST_CASE("the channel is packed into the low nibble") {
        for (std::uint8_t chan = 0; chan < 16; ++chan) {
                CAPTURE(chan);
                status_type s(status_type::note_on, chan);
                CHECK(s.value() == 0x80 + chan);
                CHECK(s.status() == status_type::note_on);
                REQUIRE(s.channel().has_value());
                CHECK(*s.channel() == chan);
        }
}

TEST_CASE("a channel wider than a nibble is masked") {
        status_type s(status_type::note_on, 0xff);
        CHECK(s.value() == 0x8f);
        CHECK(*s.channel() == 0x0f);
}

TEST_CASE("system messages carry no channel") {
        CHECK_FALSE(status_type(status_type::sysex).channel().has_value());
        CHECK_FALSE(status_type(status_type::sysex_end).channel().has_value());
        CHECK_FALSE(status_type(status_type::reset).channel().has_value());

        // and the status is the whole byte, not the high nibble
        CHECK(status_type(status_type::sysex_end).status() == status_type::sysex_end);
        CHECK(status_type(status_type::reset).status() == status_type::reset);
}

TEST_CASE("onsets and offsets are recognized across the range") {
        for (std::uint8_t chan = 0; chan < 16; ++chan) {
                CAPTURE(chan);
                CHECK(status_type(status_type::stim_on, chan).is_onset());
                CHECK(status_type(status_type::note_on, chan).is_onset());
                CHECK(status_type(status_type::stim_off, chan).is_offset());
                CHECK(status_type(status_type::note_off, chan).is_offset());
        }
}

TEST_CASE("an onset is not an offset") {
        CHECK_FALSE(status_type(status_type::stim_on).is_offset());
        CHECK_FALSE(status_type(status_type::note_on).is_offset());
        CHECK_FALSE(status_type(status_type::stim_off).is_onset());
        CHECK_FALSE(status_type(status_type::note_off).is_onset());
}

TEST_CASE("info messages are neither onset nor offset") {
        status_type s(status_type::info);
        CHECK_FALSE(s.is_onset());
        CHECK_FALSE(s.is_offset());
        CHECK_FALSE(s.is_standard_midi());
}

TEST_CASE("the standard-midi boundary is at 0x80") {
        CHECK_FALSE(status_type(status_type::stim_on).is_standard_midi());
        CHECK_FALSE(status_type(status_type::stim_off).is_standard_midi());
        CHECK_FALSE(status_type(static_cast<jill::midi::data_type>(0x7f)).is_standard_midi());
        CHECK(status_type(static_cast<jill::midi::data_type>(0x80)).is_standard_midi());
        CHECK(status_type(status_type::note_on).is_standard_midi());
}

TEST_CASE("jill string messages encode as raw bytes") {
        // stimulus names are stored as utf-8, not hex
        const std::string name = "bu49_ref_3x";
        status_type s(status_type::stim_on);
        CHECK(s.encode(name.c_str(), name.size()) == name);

        SUBCASE("including non-ascii") {
                const std::string utf8 = "st\xc3\xafmulus";
                CHECK(s.encode(utf8.c_str(), utf8.size()) == utf8);
        }

        SUBCASE("and the empty message") {
                CHECK(s.encode("", 0).empty());
        }
}

TEST_CASE("standard midi messages encode as hex") {
        // jdetect emits note on/off with the default pitch and velocity
        const char body[] = { static_cast<char>(jill::midi::default_pitch),
                              static_cast<char>(jill::midi::default_velocity) };
        status_type s(status_type::note_on);
        CHECK(s.encode(body, sizeof(body)) == "0x3c40");   // 60, 64

        SUBCASE("high bytes are not sign-extended") {
                const char high[] = { static_cast<char>(0xff), static_cast<char>(0x80) };
                CHECK(s.encode(high, sizeof(high)) == "0xff80");
        }

        SUBCASE("an empty body is just the prefix") {
                CHECK(s.encode("", 0) == "0x");
        }
}

TEST_CASE("statuses stream with a readable name") {
        auto to_string = [](status_type s) {
                std::ostringstream os;
                os << s;
                return os.str();
        };
        CHECK(to_string(status_type(status_type::stim_on)) == "STIM_ON(0)");
        CHECK(to_string(status_type(status_type::note_on, 3)) == "NOTE_ON(3)");
        CHECK(to_string(status_type(status_type::reset)) == "RESET");
}
