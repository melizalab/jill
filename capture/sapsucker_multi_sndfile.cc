
#include <sys/stat.h>
#include <iostream>

#include "sapsucker_multi_sndfile.hh"

namespace capture {

void SapsuckerMultiSndfile::_close() {
	jill::util::MultiSndfile::_close();

	if (_final_dest.length() == 0)
		return;

	// attempt to move file
	// first stat the location and verify that it is a directory
	struct stat stat_info;
	int stat_ret = stat(_final_dest.c_str(), &stat_info);
	if (stat_ret) {
		if (_logv) {
			(*_logv) << "Failure to stat final destination: "
			         << _final_dest
			         << std::endl;
		} // if
		return;
	} // if
	if (!S_ISDIR(stat_info.st_mode)) {
		if (_logv) {
			(*_logv) << "Final destination is not a directory!"
			         << std::endl;
		} // if
		return;
	} // if
	std::string old_name = current_file();
	std::string new_name = old_name;
	if (new_name.find_last_of("/") != std::string::npos) {
		new_name = new_name.substr(new_name.find_last_of("/")+1);
	} // if
	new_name = _final_dest + "/" + new_name;

	int rename_ret = rename(old_name.c_str(), new_name.c_str());
	if (_logv) {
		if (rename_ret == 0) {
			(*_logv) << "Moving "
			         << old_name
			         << " to "
			         << new_name
			         << std::endl;
		} else {
			(*_logv) << "File move failed!" << std::endl;
		} // if
	} // if
}

}

