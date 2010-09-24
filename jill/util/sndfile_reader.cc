#include "sndfile_reader.hh"
#include "sndfile.hh"
#include "string.hh"

using namespace jill::util;

SndfileReader::SndfileReader() {}

SndfileReader::SndfileReader(const char *filename) { open(filename); }

SndfileReader::SndfileReader(const std::string &filename) { open(filename.c_str()); }


void
SndfileReader::open(const char *filename)
{
	SNDFILE *f = sf_open(filename, SFM_READ, &_sfinfo);
	if (!f) {
		throw FileError(make_string() << "couldn't open '" << filename << "' for input");
	}
	_sndfile.reset(f, sf_close);
	_nread = 0;
}
