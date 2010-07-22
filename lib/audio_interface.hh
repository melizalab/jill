/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _AUDIO_INTERFACE_HH
#define _AUDIO_INTERFACE_HH

#include "audio.hh"

#include <string>
#include <stdexcept>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>

namespace jill {

/**
 * AudioInterface is a generic wrapper around a callback-based audio
 * process. The callback is a function or function object will be
 * called repeatedly, being passed an input and output array of
 * samples. Processes that only input or output can ignore one or the other pointer.
 */
class AudioInterface : boost::noncopyable
{
public:

	struct AudioError : public std::runtime_error {
		AudioError(std::string const & w) : std::runtime_error(w) { }
	};

	AudioInterface();
	virtual ~AudioInterface() { }

	/// The type of the process callback
	typedef boost::function<void (sample_t *in, sample_t *out, nframes_t)> ProcessCallback;

	/**
	 * Set the process callback. This can be a raw function
	 * pointer or any function object. The argument is copied; if
	 * this is undesirable use a boost::ref.
	 * @param cb The function object that will process the audio data
	 */
	virtual void set_process_callback(const ProcessCallback &cb) {
		_process_cb = cb;
	}

	/// get sample rate
	virtual nframes_t samplerate() const = 0;
	/// check if backend is still running
	virtual bool is_shutdown() const = 0;

protected:

	ProcessCallback _process_cb;
};


/**
 * Define an audio interface that supports a transport protocol.
 *
 */
class AudioInterfaceTransport : public AudioInterface
{
public:

	typedef boost::function<void (position_t *)> TimebaseCallback;

	virtual void set_timebase_callback(TimebaseCallback cb) = 0;


	virtual bool transport_rolling() const = 0;
	virtual position_t position() const = 0;
	virtual nframes_t frame() const = 0;
	virtual bool set_position(position_t const &) = 0;
	virtual bool set_frame(nframes_t) = 0;

protected:

	TimebaseCallback _timebase_cb;
};

} // namespace jill

#endif // _AUDIO_INTERFACE_HH
