/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _READAHEAD_STIMQUEUE_HH
#define _READAHEAD_STIMQUEUE_HH

#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "stimqueue.hh"

namespace jill {

class event_logger;

namespace util {

/**
 * An implementation of stimqueue that provides a background thread for loading
 * data from disk and resampling.
 */
class readahead_stimqueue : public stimqueue {

public:
        using iterator = std::vector<stimulus_t *>::iterator;
        using const_iterator = std::vector<stimulus_t *>::const_iterator;

        /**
         * Initialize the queue with a sequence of stimuli.
         *
         * @param first  iterator pointing to the start of the sequence
         * @param last   iterator pointing to the end of the sequence
         * @param samplerate   the sampling rate needed by the consumer
         * @param loop         whether to keep repeating the queue
         */
        readahead_stimqueue(iterator first, iterator last,
                            nframes_t samplerate,
                            bool loop=false);

        /**
         * Stops the background thread and waits for it.
         *
         * Callers should still stop and join explicitly rather than relying on
         * this: the thread dereferences the stimuli it was given, and if the
         * queue is destroyed during static destruction those may already be
         * gone.
         */
        ~readahead_stimqueue() override;

        stimulus_t const * head() override;
	stimulus_t const * previous();
        void release() override;
        void stop() override;
        void join() override;

private:
        void loop();                      // called by thread

        iterator const _first;
        iterator const _last;
        iterator _it;                             // current position
        /* Written by the worker thread under _lock, and read and written by
         * the realtime thread through head(), previous() and release(), which
         * take no lock in order to stay wait-free. ThreadSanitizer reports
         * this on every run of test_stimset. Default ordering: one access per
         * period, so nothing to gain from tuning it. Making the race
         * well-defined is not the same as making the protocol correct -- what
         * guarantees release() owes the realtime thread is still open. */
        std::atomic<stimulus_t *> _head;
        std::atomic<stimulus_t *> _previous;

        nframes_t const _samplerate;
        bool const _loop;
        bool _running;

	// Declaration order matters here. Need to initialize the mutex and
	// condition variable before the thread starts, and shut down the thread
	// before destroying the variables it waits on.
        std::mutex _lock;
        std::condition_variable  _ready;
        std::thread _thread;
};

}} // namespace jill::util

#endif
