
#include <sys/stat.h>
#include <iostream>

#include "sapsucker_multi_sndfile.hh"

namespace capture {

SapsuckerMultiSndfile::SapsuckerMultiSndfile(const char* templ,
                                             size_type samplerate,
                                             const char* final_dest,
                                             jill::util::logstream *logv) :
		jill::util::MultiSndfile(templ, samplerate),
		_final_dest(final_dest),
		_logv(logv),
		_file_mover(NULL)
{
	if (_logv) {
		(*_logv) << "Setting permanent output to "
		         << _final_dest
		         << std::endl;
	} 
	if (!_final_dest.empty()) {
		_file_mover = 
			new jill::util::FileCopyQueue(_final_dest, 30, _logv);
		_file_mover->run();
	} // if
}

SapsuckerMultiSndfile::SapsuckerMultiSndfile(const std::string &templ,
                                             size_type samplerate,
                                             const std::string &final_dest,
                                             jill::util::logstream *logv) :
		jill::util::MultiSndfile(templ, samplerate),
		_final_dest(final_dest),
		_logv(logv),
		_file_mover(NULL)
{
	if (_logv) {
		(*_logv) << "Setting permanent output to "
		         << _final_dest
		         << std::endl;
	} 
	if (!_final_dest.empty()) {
		_file_mover = 
			new jill::util::FileCopyQueue(_final_dest, 30, _logv);
		_file_mover->run();
	} // if
}

SapsuckerMultiSndfile::~SapsuckerMultiSndfile() 
{
	if (_file_mover) {
		_file_mover->stop();
		delete _file_mover;
	} // if
}

void SapsuckerMultiSndfile::_close() {
	jill::util::MultiSndfile::_close();
	if (!_file_mover)
		return;

	jill::util::FileCopyQueue::utimes_timeval tv;
	get_entry_time(&(tv.timeval1));
	_file_mover->add_file(current_file(), tv);
}

}

