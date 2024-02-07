#include <iostream>
#include "types.hh"
#include "midi.hh"

using jill::nframes_t;
using namespace jill::midi;

std::ostream&
jill::midi::operator<< (std::ostream &os, const status_type &s) {
	switch (s) {
	case status_type::stim_on:
		os << "STIM_ON"; break;
	case status_type::stim_off:
		os << "STIM_OFF"; break;
	case status_type::info:
		os << "INFO"; break;
	case status_type::note_on:
		os << "NOTE_ON"; break;
	case status_type::note_off:
		os << "NOTE_OFF"; break;
	case status_type::key_press:
		os << "KEYPRESS"; break;
	case status_type::ctl:
		os << "CTL"; break;
	case status_type::program_change:
		os << "PROGRAM_CHANGE"; break;
	case status_type::channel_aftertouch:
		os << "AFTERTOUCH"; break;
	case status_type::pitch_bend:
		os << "PITCH_BEND"; break;
	case status_type::sysex:
		os << "SYSEX"; break;
	case status_type::sysex_end:
		os << "SYSEX_END"; break;
	case status_type::reset:
		os << "RESET"; break;
	default:
		os << "UNDEFINED";
	}
	if (auto chan = s.channel()) {
		os << "(" << static_cast<int>(*chan) << ")";
	}
	return os;
}

int
jill::midi::write_message(void * buffer, nframes_t time, status_type status, char const * message) {
	std::size_t len = 1;
	if (message)
		len += strlen(message) + 1; // add the null byte
	data_type *buf = jack_midi_event_reserve(buffer, time, len);
	if (buf) {
		buf[0] = status.value();
		// FIXME: replace with strlcpy
		if (message) strcpy(reinterpret_cast<char*>(buf)+1, message);
		return 0;
	}
	else
		return ENOBUFS;
}

int
jill::midi::find_trigger(void const * midi_buffer, bool onset) {
	void * buf = const_cast<void *>(midi_buffer);
	jack_midi_event_t event;
	nframes_t nevents = jack_midi_get_event_count(buf);
	for (nframes_t i = 0; i < nevents; ++i) {
		jack_midi_event_get(&event, buf, i);
		if (event.size == 0)
			continue;
		status_type status = *reinterpret_cast<data_type const *>(event.buffer);
		if (onset and status.is_onset()) {
			return event.time;
		}
		else if (!onset and status.is_offset()) {
			return event.time;
		}
	}
	return -1;
}

bool
jill::midi::is_onset(void const * midi_buffer, std::size_t size) {
	if (size == 0) return false;
	status_type status = *reinterpret_cast<data_type const *>(midi_buffer);
	return status.is_onset();
}

bool
jill::midi::is_offset(void const * midi_buffer, std::size_t size) {
	if (size == 0) return false;
	status_type status = *reinterpret_cast<data_type const *>(midi_buffer);
	return status.is_offset();
}
