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
        ~readahead_stimqueue() override = default;

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
        stimulus_t * _head;
        stimulus_t * _previous;

        nframes_t const _samplerate;
        bool const _loop;
        bool _running;

        std::thread _thread;
        std::mutex _lock;
        std::condition_variable  _ready;
};

}} // namespace jill::util

#endif
