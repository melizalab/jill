#include "multi_sndfile.hh"
#include "string.hh"
#include <boost/format.hpp>
using namespace jill::util;

const MultiSndfile::Entry*
MultiSndfile::_next(const char *)  {
	if (_fn_templ.empty()) return &_entry;

	struct timeval etime;
	gettimeofday(&etime,0);
	_entry.time[0] = _entry.time[1] = etime;

	try {
		_entry.filename = (boost::format(_fn_templ) % ++_entry.file_idx).str();
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
