/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Mike Lusignan <mlusigna@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This file is a C++ header file.  Its function is to declare the
 * interface of our class, which will be responsible for analyzing
 * incoming samples and deciding whether to write them to disk. The
 * implementation of the class is declared separately, in
 * trigger_writer.cc (which we'll visit later).
 *
 * As it turns out, some of the work has already been done for us.  In
 * the JILL library, there's a class called WindowDiscriminator, which
 * will analyze the number of times a signal crosses a threshold and
 * keep a running tally.  When the number of crossings exceeds a
 * certain value, the WindowDiscriminator is "open".  It then keeps a
 * separate tally of threshold crossings, and when the number of
 * crossings drops below a certain value, the WindowDiscriminator is
 * "closed".  More information on how the WindowDiscriminator works
 * can be found in jill/filters/window_discriminator.hh
 *
 * What our class needs to do is pass samples from JACK (or a sound
 * file) to the window discriminator, and check whether the gate is
 * open or closed.  If the gate is open, the samples are written to
 * disk.  If not, they're stored in a "prebuffer", which is just a
 * temporary holding area.  When the gate opens, we'll typically want
 * to write samples from some period of time before the gate opened.
 * By storing the data in the prebuffer we'll have access to this.
 *
 */
#ifndef _SWITCH_TRACKER_HH
#define _SWITCH_TRACKER_HH

/*
 * We need to import some types from JILL. The client
 * header provides some basic types (sample_t and nframes_t).
 */
#include "jill/client.hh"
#include "jill/sndfile_player_client.hh"

/* 
 * We'll need a ringbuffer to pass data from the real-time thread to
 * the main loop, which will will handle all the threshold
 * calculations and disk IO.  We use the TimeRingbuffer class, found
 * in in this header, which will store the frame number associated
 * with the data, which we need to timestamp the output files. This
 * include also gets us the Prebuffer class.  We also need the
 * BufferAdapter to simplify writing the prebuffer to disk.
 */
#include "jill/util/time_ringbuffer.hh"
#include "jill/util/buffer_adapter.hh"
#include "jill/util/multi_sndfile.hh"
#include "jill/util/logger.hh"

/* This header contains the WindowDiscriminator class */
#include "window_discriminator.hh"

/* 
 * This header lets us declare the TriggerWriter class noncopyable;
 * this prevents downstream users from accidentally creating a copy of
 * any TriggerWriter objects, an operation we don't want to support.
 */
#include <boost/noncopyable.hpp>

#include <list>

#include "switch_playback_listener.hh"
#include "switch.hh"
#include "quotas.hh"

/* 
 * We place the triggered writer into the capture namespace
 */
namespace capture {
using namespace jill;
namespace util = jill::util;

/*
 * We will also need access to the jill::util::logstream class (for
 * logging the times when the signal starts and stops). This class is
 * found in jill/util/logger.hh, but we don't actually use it in this
 * header (just declare a pointer to it).  The following is a "forward
 * declaration", a trick to speed up compilation times.
 */
//class jill::util::logstream;

/*
 * Here is where we define our processing class.  We're going to take
 * advantage of the fact that we can pass a function *object* to the
 * AudioInterface::set_process_callback function; this allows us to
 * encapsulate all the variables needed for the processing logic into
 * a single structure.
 *
 * Note that although this is a definition of the class, we are only
 * declaring the member functions. Their implementations will be in
 * trigger_classes.cc
 *
 * The class derives from boost::noncopyable to prevent copying.
 */
class SwitchTracker : boost::noncopyable {

public:
        /*
	 * Members defined as public are accessible from outside the
	 * class. This allows us to protect the state of the object
	 * and simplify the users' interaction.
	 */

	/**
	 * The constructor of the class allocates memory and sets up
	 * any internal variables.  The arguments to the constructor
	 * are as follows:
	 *
	 * @param wd      A reference to a window discriminator. Rather
	 *                than creating the object here, which would require the 
	 *                this class to know all the constructor parameters, we
	 *                initialize the object elsewhere and store a reference to it.
	 * @param writer  A reference to a file output object. We use one that allows
	 *                us to write to multiple files.
	 * @param logger  A reference to a logstream object, for logging start and stop
	 *                information. 
	 * @param prebuffer_size  The size of the prebuffer, in samples
	 * @param buffer_size     The size of the process buffer, in samples
	 */
	SwitchTracker(const std::string& output_name,
	              const std::string& song_name,
                      int switch_refractory_time,
                      QuotaInfo& quotas,
	              boost::shared_ptr<SwitchPlaybackListener> listener,
	              boost::shared_ptr<Switch> s);

	/**
	 * Simply returns true or false whether the key press at the time
	 * of the call.
	 */
	bool trigger();

	bool song_ended();

	bool stop_playback(const char* msg);

	bool connect_playback_output(const char* channel_name);

protected:
	/*
	 * In the protected part of the class interface, we define
	 * functions that perform well-defined operations, but which
	 * should not be accessible from outside the class.
	 */
	
private:
	/*
	 * Variables declared in this section are not visible outside
	 * of the class member functions; this is where we store any
	 * data we want to encapsulate in the class.  Any objects that
	 * will be accessed by both operator() and flush() need to be
	 * threadsafe.
	 */

	// tutor client which plays the song
	boost::shared_ptr<SndfilePlayerClient> _tutor_client;

	QuotaInfo _quotas;
	int _switch_refractory;

	// end points for waiting to reset switch, recording
	int _end_switch_refractory;

	int _triggering_interval;
	int _triggering_count;

	bool _song_ended;

	boost::shared_ptr<Switch> _switch;
	boost::shared_ptr<SwitchPlaybackListener> _listener;
	std::string _output_name;
	std::string _song_name;
};

/*
 * As you can see, the definition of the class is fairly simple. Users
 * that want to use the class only need to see this file, as it
 * describes the interface for interacting with objects of type
 * TriggerWriter.  The implementations of the functions are defined in
 * trigger_writer.cc, which you should open at this point.
 */

}

#endif
