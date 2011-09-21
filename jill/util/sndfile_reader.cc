#include "sndfile_reader.hh"
#include "sndfile.hh"
#include "string.hh"

using namespace jill::util;


void
SndfileReader::open(boost::filesystem::path const &filename)
{
	SNDFILE *f = sf_open(filename.string().c_str(), SFM_READ, &_sfinfo);
	if (!f) {
		throw FileError(make_string() << "couldn't open '" << filename << "' for input");
	}
	_sndfile.reset(f, sf_close);
	_nread = 0;
}
