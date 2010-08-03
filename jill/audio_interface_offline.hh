/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _AUDIO_INTERFACE_OFFLINE_HH
#define _AUDIO_INTERFACE_OFFLINE_HH

#include "audio_interface.hh"
#include <boost/shared_array.hpp>
#include "util/sndfile.hh"

namespace jill {

/*
 * The classes in this file are for testing JILL clients. Rather than
 * use the JACK server to run the process thread, data are read from a
 * input sound file and passed to the process callback in blocks.  The
 * AudioInterfaceOffline should be used instead of the
 * AudioInterfaceJack class, and the OfflineApplication class instead
 * of JillApplication.
 */

class AudioInterfaceOffline : public AudioInterface {

public:
	AudioInterfaceOffline(nframes_t blocksize, nframes_t samplerate=1);
	virtual ~AudioInterfaceOffline();

	void set_input(const std::string &sndfile, nframes_t startframe=0);
	void set_output(const std::string &sndfile);


	/**
	 * Run the process callback, reading data from the input file
	 * and writing data to the output file.  Processes one block of data
	 */
	void process();

	virtual nframes_t samplerate() const { return _samplerate; }
	virtual bool is_shutdown() const { return _quit; }

private:
	util::sndfilereader _sfin;
	util::sndfile _sfout;
	nframes_t _blocksize, _samplerate;
	bool _quit;

	boost::shared_array<sample_t> _bufin, _bufout;
};

}
#endif
