#include "multi_sndfile.hh"
#include "string.hh"
#include <boost/format.hpp>
using namespace jill::util;

#include "simple_template.hh"

#include <iostream>

const MultiSndfile::Entry*
MultiSndfile::_next(const std::string &)  {
	if (_fn_templ.empty()) return &_entry;

	struct timeval etime;
	gettimeofday(&etime,0);
	_entry.time[0] = _entry.time[1] = etime;
	// calculate msecs since start of day
	time_t t;
	struct tm tm;
	t = time(NULL);
	localtime_r(&t, &tm);
	int msecs = ((tm.tm_hour * 60 + tm.tm_min) * 60 + tm.tm_sec);
	msecs = msecs * 1000 + etime.tv_usec / 1000;
	try {
		SimpleTemplate templ;
		templ.process(_fn_templ);
		add_time_values(templ);
		templ.add_variable("entry", ++_entry.file_idx);
		templ.add_variable("total_msecs", msecs);
		_entry.filename = templ.str();
	}
	catch (const boost::io::format_error &e) {
		throw FileError(make_string() << "invalid filename template: " << _fn_templ);
	}
	SimpleSndfile::_open(_entry.filename.c_str(), _samplerate);
	return &_entry;
}

void
MultiSndfile::_open(const std::string &templ, size_type samplerate)
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

