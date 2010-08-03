/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *! @file
 *! @brief Helper classes for the triggered_writer
 *! 
 */
#ifndef _TRIGGER_CLASSES_HH
#define _TRIGGER_CLASSES_HH

/*
 * We need to import some classes from JILL. We'll be using the window
 * discriminator, the ringbuffer, and the soundfile writer.
 */
#include "jill/audio.hh"
#include "jill/options.hh"
#include "jill/filters/window_discriminator.hh"
#include "jill/util/sndfile.hh"
#include "jill/util/time_ringbuffer.hh"

/*
 * These include statements bring in some other classes we need.
 */
#include <boost/noncopyable.hpp>
#include <boost/any.hpp>
#include <iostream>

/* 
 * We place the triggered writer into the jill namespace to simplify
 * access to other members of that namespace
 */
namespace jill {

/*
 * Here is where we define our processing class.  We're going to take
 * advantage of the fact that we can pass a function *object* to the
 * JACK client; this allows us to encapsulate all the variables needed
 * for the processing logic into a single structure.
 *
 * Our process loop needs to pass the data to the window
 * discriminator, which will let us know when to start and stop
 * recording.  We need a ringbuffer to store data for output to disk.
 * We also want to be able to pre-buffer data; that is, to be able to
 * look back some time in the past before the trigger was activated,
 * and write those samples to disk, too.
 *
 * The actual implementations of these functions will be in
 * trigger_classes.cc
 */
class TriggeredWriter : boost::noncopyable {

public:
	/**
	 * The constructor of the class allocates memory and sets up
	 * any internal variables.  The arguments to the constructor are as follows:
	 *
	 * @param wd      A reference to a window discriminator. Rather
	 *                than creating the object here, which would require the 
	 *                this class to know all the constructor parameters, we
	 *                initialize the object elsewhere and store a reference to it.
	 * @param writer  An reference to a file output object. We use one that allows
	 *                us to write to multiple files.
	 * @param prebuffer_size  The size of the prebuffer, in samples
	 * @param buffer_size     The size of the process buffer, in samples
	 */
	TriggeredWriter(filters::WindowDiscriminator<sample_t> &wd, 
			util::multisndfile &writer,
			nframes_t prebuffer_size, nframes_t buffer_size);

	/**
	 * This function overloads the () operator; that is, if we
	 * create an object "tw" of type TriggeredWriter, and call
	 * tw(), this function gets called.  It has the same signature
	 * as the process functions we wrote in previous examples.
	 */
	void operator() (sample_t *in, sample_t *out, nframes_t nframes, nframes_t time);

	/**
	 * We also need a function that can be run in the main loop of
	 * the program to flush data to disk.  Note that because
	 * different threads are going to be using this class, we need
	 * to be certain that all of the public functions are
	 * "re-entrant" - if they are called concurrently by two
	 * different threads, the internal state of the object will
	 * remain consistent.
	 *
	 * @returns the name of the file we wrote to
	 */
	const std::string &flush();

	/// Returns true if the window discriminator is open at the
	/// end of the last flush operation
	bool is_writing() const { return _wd.open(); }
	/// Returns the frame in which the window discriminator last opened.
	nframes_t start_time() const;
	/// Returns the frame in which the window disciminator last closed.
	nframes_t stop_time() const;

protected:

	/// Open a new entry and log it
	void next_entry(nframes_t time);

private:
	/*
	 * Variables declared in this section are not visible outside
	 * of the class member functions; this is where we store any
	 * data we want to encapsulate in the class.  Any objects that
	 * will be accessed by both operator() and flush() need to be
	 * threadsafe.
	 */

	/// This is our reference to the window discriminator
	filters::WindowDiscriminator<sample_t> &_wd;

	/// This is our reference to the soundfile writer
	util::multisndfile &_writer;
	// a typedef for pointers to the writer's write function
	typedef util::multisndfile::size_type (util::multisndfile::*writefun_t)(const sample_t *,
										util::multisndfile::size_type);

	/// A ringbuffer that the process thread can write to
	util::TimeRingbuffer<sample_t,nframes_t> _ringbuf;
	/// The prebuffer
	util::Prebuffer<sample_t> _prebuf;
};

/**
 * Another way of unifying our testing and the final module is to have
 * a common options class. As in the previous chapter, we'll derive
 * from Options, but in this case the class will be defined here.
 *
 * Because we want to be able to use the TriggerOptions with
 * either a JILL application, or a standalone test application, the
 * base class is parameterized (i.e. specified as a template argument)
 */
template <typename Base=Options>
class TriggerOptions : public Base {

public:

	TriggerOptions(const char *program_name, const char *program_version)
		: Base(program_name, program_version) { // this calls the superclass constructor 
		po::options_description tropts("Trigger options");
		tropts.add_options()
			("prebuffer", po::value<float>()->default_value(1000), 
			 "set prebuffer size (ms)")
			("period-size", po::value<float>()->default_value(100), 
			 "set analysis period size (ms)")
			("open-thresh", po::value<float>()->default_value(0.1), 
			 "set sample threshold for open gate (0-1.0)")
			("open-rate", po::value<float>()->default_value(100), 
			 "set crossing rate thresh for open gate (s^-1)")
			("open-periods", po::value<int>()->default_value(10), 
			 "set number of periods for open gate")
			("close-thresh", po::value<float>()->default_value(0.1), 
			 "set sample threshold for close gate")
			("close-rate", po::value<float>()->default_value(50), 
			 "set crossing rate thresh for close gate (s^-1)")
			("close-periods", po::value<int>()->default_value(10), 
			 "set number of periods for close gate");

		Base::cmd_opts.add(tropts);
		Base::cfg_opts.add(tropts);
		Base::visible_opts.add(tropts);
		Base::cmd_opts.add_options()
			("output-tmpl", po::value<std::string>(), "output file template");
		Base::pos_opts.add("output-tmpl", -1);
	} 

	std::string output_file_tmpl;

	float prebuffer_size_ms;  // in ms
	nframes_t prebuffer_size; // samples

	float open_threshold;
	float close_threshold;

	float open_crossing_rate;  // s^-1
	int open_count_thresh; // raw count
	float close_crossing_rate;
	int close_count_thresh;
	
	float period_size_ms; // in ms
	nframes_t period_size; // in samples

	int open_crossing_periods;
	int close_crossing_periods;

	/// adjust values for sample rate (in Hz)
	void adjust_values(nframes_t samplerate) {
		std::cout << samplerate << std::endl;
		prebuffer_size = prebuffer_size_ms * samplerate / 1000;
		period_size = period_size_ms * samplerate / 1000;
		// count thresh is count rate * integration period
		open_count_thresh = open_crossing_rate * period_size / 1000 * open_crossing_periods;
		close_count_thresh = close_crossing_rate * period_size / 1000 * close_crossing_periods;
	}
		

protected:

	void print_usage() {
		std::cout << "Usage: " << Base::_program_name << " [options] [output-file-template]\n"
			  << Base::visible_opts << std::endl
			  << "output-file-template:   specify output files (e.g. myrecording_%03d.wav)\n"
			  << "                        if omitted, events are logged but no data is written"
			  << std::endl;
	}

	void process_options() {
		Base::process_options();

		Base::assign(output_file_tmpl, "output-tmpl");

		Base::assign(prebuffer_size_ms, "prebuffer");
		Base::assign(period_size_ms, "period-size");

		Base::assign(open_threshold, "open-thresh");
		Base::assign(open_crossing_rate, "open-rate");
		Base::assign(open_crossing_periods, "open-periods");

		Base::assign(close_threshold, "close-thresh");
		Base::assign(close_crossing_rate, "close-rate");
		Base::assign(close_crossing_periods, "close-periods");
	}

};

template <typename Base>
std::ostream &operator<<(std::ostream &os, const TriggerOptions<Base> &o)
{
	using std::endl;

	os << "output template: " << o.output_file_tmpl << endl
	   << "prebuffer: " << o.prebuffer_size_ms << "->" << o.prebuffer_size << endl
	   << "period size: " << o.period_size_ms << "->" << o.period_size << endl
	   << "open thresh: " << o.open_threshold << endl
	   << "open count thresh: " << o.open_crossing_rate << "->" << o.open_count_thresh << endl
	   << "open periods: " << o.open_crossing_periods << endl
	   << "close thresh: " << o.close_threshold << endl
	   << "close count thresh: " << o.close_crossing_rate << "->" << o.close_count_thresh << endl
	   << "close periods: " << o.close_crossing_periods << endl;
	return os;
}	   

}

#endif
