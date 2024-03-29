/*
 * JILL - C++ framework for JACK
 * Copyright (C) 2012 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _MIDI_HH
#define _MIDI_HH 1

#include "types.hh"
#include <cerrno>
#include <string>
#include <cstring>
#include <optional>
#include <jack/midiport.h>

/**
 * @file midi.hh
 * @brief midi data types
 */
namespace jill {

namespace midi {

        using data_type = jack_midi_data_t;
        const static data_type default_pitch = 60;
        const static data_type default_velocity = 64;

	class status_type {
	public:
		enum Status : data_type {
			// these are technically data bytes that we're using for
			// string messages
			stim_on = 0x00,
			stim_off = 0x10,
			info = 0x20,
			// defined in the midi spec, not a complete enumeration
			note_on = 0x80,
			note_off = 0x90,
			key_press = 0xa0,
			ctl = 0xb0,
			program_change = 0xc0,
			channel_aftertouch = 0xd0,
			pitch_bend = 0xe0,
			// 0xfX values don't have an associated channel
			sysex = 0xf0,
			sysex_end = 0xf7,
			reset = 0xff,
		};

		status_type() = default;
		constexpr status_type(data_type value) : _value(value) {};
		constexpr status_type(Status value) : _value(value) {};
		constexpr status_type(Status value, std::uint8_t channel) : _value(value) {
			if (_value < 0xf0)
				_value = (_value & 0xf0) | (channel & 0x0f);
		}

		explicit operator bool() const = delete;
		constexpr Status status() const {
			return static_cast<Status>((_value >= 0xf0) ? _value : _value & 0xf0);
		}
		constexpr std::optional<std::uint8_t> channel() const {
			return (_value >= 0xf0) ? std::nullopt : std::optional<std::uint8_t>(_value & 0x0f);
		}
		constexpr data_type value() const { return _value; }
		constexpr operator Status() const { return status(); }

		constexpr bool is_standard_midi() const {
			return _value >= 0x80;
		}

		constexpr bool is_onset() const {
			switch(status()) {
			case status_type::note_on:
			case status_type::stim_on:
				return true;
			default:
				return false;
			}
		}

		constexpr bool is_offset() const {
			switch(status()) {
			case status_type::note_off:
			case status_type::stim_off:
				return true;
			default:
				return false;
			}
		}
	private:
		data_type _value;
	};

	std::ostream& operator<< (std::ostream &os, const status_type &s);

        /**
         * Write a string message to a midi buffer.
         *
         * @param buffer   the JACK midi buffer
         * @param time     the offset of the message (in samples)
         * @param status   the status byte
         * @param message  the string message, or 0 to send an empty message
         *
         */
        int write_message(void * buffer, nframes_t time, status_type status, char const * message=nullptr);

        /**
         * Find an onset or offset event in a midi event stream.
         *
         * @param midi_buffer  the JACK midi buffer
         * @param onset        if true, look for onset events; if false, for offsets
         * @return the time of the first event, or -1 if no event was found.
         */
        int find_trigger(void const * midi_buffer, bool onset=true);

	/** Returns true if a midi event buffer contains an onset */
        bool is_onset(void const * midi_buffer, std::size_t size);

        bool is_offset(void const * midi_buffer, std::size_t size);

} } // jill::midi


#endif /* _MIDI_HH */
