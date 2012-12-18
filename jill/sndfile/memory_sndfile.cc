#include "memory_sndfile.hh"
using namespace jill::util;

MemorySndfile::MemorySndfile() {}

MemorySndfile::~MemorySndfile() {}

void
MemorySndfile::_open(path const &, size_type framerate)
{
	_close();
	_entry.framerate = framerate;
}


void
MemorySndfile::_close()
{
	_entry.data.clear();
}

MemorySndfile::Entry*
MemorySndfile::_next(const std::string &)
{
	_close();
	return &_entry;
}
