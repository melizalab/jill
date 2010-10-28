
#ifndef _FILE_PLAYBACK_RECORDER_HH_
#define _FILE_PLAYBACK_RECORDER_HH_

#include "switch_playback_listener.hh"
#include <fstream>
#include <iostream>

namespace capture {

class FilePlaybackRecorder : public SwitchPlaybackListener {
public:
	FilePlaybackRecorder(const char* filename,
	                     int length_max=-1) : _filename(filename), _max_size(length_max) {};

        virtual void playback(const char* name,
                              const char* song,
                              time_t time) {
	        // time info
       		const int buf_size = 26;
	        char ctime_buf[buf_size];
		ctime_r(&time, ctime_buf);
		// ctime_r appends \n to the end of the buffer
		for (int n = buf_size; n >= 0; n--) {
			if (ctime_buf[n] <= ' ') {
				ctime_buf[n] = 0;
			} else {
				break;
			} // if
		} // for

		// open file
		std::fstream f_log;

		f_log.open(_filename.c_str(), std::ios::out | std::ios::app);
		f_log << name << '\t' << song << '\t' << ctime_buf << std::endl;
		f_log.close();
	} // playback
private:
	std::string _filename;
	int _max_size;
}; // class FilePlaybackRecorder

}

#endif
