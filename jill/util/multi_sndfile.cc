#include "multi_sndfile.hh"
#include "string.hh"
#include <boost/format.hpp>
using namespace jill::util;

#include <iostream>

const MultiSndfile::Entry*
MultiSndfile::_next(const char *)  {
	if (_fn_templ.empty()) return &_entry;

	struct timeval etime;
	gettimeofday(&etime,0);
	_entry.time[0] = _entry.time[1] = etime;

	// get time information
	bool waiting_for_new_item = false;
	time_t t;
	struct tm tm;
	t = time(NULL);
	localtime_r(&t, &tm);
	const int buf_size = 128;
	char buf[buf_size];
	strftime(buf, buf_size, "%B %d %Y %H %M %S", &tm);
	const char* items[6];
	int item_count = 1;
	items[0] = &(buf[0]);
	for (int i = 0; buf[i] && i < buf_size; i++) {
		if (buf[i] == ' ') {
			buf[i] = 0;
			waiting_for_new_item = true;
			continue;
		} else if (waiting_for_new_item) {
			waiting_for_new_item = false;
			items[item_count++] = &(buf[i]);
		}
	} // for
	
	try {
		boost::format formatter(_fn_templ);
		formatter.exceptions(boost::io::all_error_bits^(boost::io::too_many_args_bit));
		formatter % ++_entry.file_idx;
		// add special value
		// milliseconds since the start of day
		int msecs = ((tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec);
		msecs = msecs * 1000 + etime.tv_usec / 1000;
		formatter % msecs;
		for (int i = 0; i < item_count; i++) {
			formatter % items[i];
		} // for
		// _entry.filename = (boost::format(_fn_templ) % ++_entry.file_idx).str();
		_entry.filename = formatter.str();
	}
	catch (const boost::io::format_error &e) {
		throw FileError(make_string() << "invalid filename template: " << _fn_templ);
	}

	SimpleSndfile::_open(_entry.filename.c_str(), _samplerate);
	return &_entry;
}

void
MultiSndfile::_open(const char *templ, size_type samplerate)
{
	_fn_templ = templ;
	_samplerate = samplerate;
}

void
MultiSndfile::_close()
{
	SimpleSndfile::_close();
	utimes(_entry.filename.c_str(), _entry.time);
}

