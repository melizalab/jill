/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _STIMSET_HH
#define _STIMSET_HH

#include <vector>
#include <algorithm>
#include <boost/ptr_container/ptr_vector.hpp>
#include "../types.hh"

namespace jill { namespace util {


class stimset : boost::noncopyable {

public:
        typedef std::vector<jill::stimulus_t *>::iterator iterator;

        stimset(nframes_t samplerate);

        /** add a stimulus to the stimset. The stimset now owns the object */
        void add(jill::stimulus_t * stim, size_t nreps=1);

        /** shuffle the stimulus list and start at the beginning */
        void shuffle();

        /**
         * Get the next item in the stimset. Blocks until stimulus is loaded and
         * resampled.
         *
         * @return pointer to next stimulus, or 0 if at the end of the list.
         * Will only return 0 once and then returns to the top.
         */
        jill::stimulus_t const * next();

private:

        boost::ptr_vector<jill::stimulus_t> _stimuli;
        std::vector<jill::stimulus_t *> _stimlist; // doesn't own
        iterator _it;
        nframes_t _samplerate;

};

/**
 * Represents a two-element stimulus queue.  One thread is pulling data off the
 * current stimulus, while another is making sure the next stimulus is loaded.
 * The playback thread also needs to compare the current time against the last
 * time it played a stimulus.  The math for this is tricky, so it's included
 * here so it can be tested.
 */
struct stimqueue {
        jill::stimulus_t const * current_stim;     // currently playing stim
        jill::stimulus_t const * next_stim;        // next stim in queue
        size_t buffer_pos;                         // position in buffer
        nframes_t last_start;                      // last stimulus start time
        nframes_t last_stop;                       // last stimulus stop

        /** return true if the current stimulus is loaded */
        bool ready() const { return current_stim && current_stim->buffer(); }

        /** true if the stimulus is playing (samples have been read) */
        bool playing() const {
                return current_stim && (buffer_pos > 0) && (buffer_pos < current_stim->nframes());
        }

        /** the current buffer position */
        sample_t const * buffer() const { return current_stim->buffer() + buffer_pos; }

        /** the number of samples left in the stimulus */
        nframes_t nsamples() const { return current_stim->nframes() - buffer_pos; }

        /** set start time */
        void start(nframes_t time) { last_start = time; }

        /** set stop time and clear current stimulus */
        void stop(nframes_t time) {
                last_stop = time;
                buffer_pos = 0;
                current_stim = 0;
        }

        /** advance the read pointer  */
        void advance(nframes_t samples) { buffer_pos = std::min(nframes_t(buffer_pos + samples),
                                                                current_stim->nframes()); }

        /**
         * compare the current time against the last start and stop times and
         * determine if it's time to play the current stimulus
         *
         * @param time       the current sample count
         * @param min_start  minimum time since last stimulus started
         * @param min_stop   minimum time since last stimulus stopped
         *
         * @return the time to start the stimulus, relative to the current time
         */

        nframes_t offset(nframes_t time, nframes_t min_start, nframes_t min_stop) const {
                // deltas with last events - overflow is correct because time >= lastX
                nframes_t dstart = time - last_start;
                nframes_t dstop = time - last_stop;
                // now we have to check for overflow
                nframes_t ostart = (dstart > min_start) ? 0 : min_start - dstart;
                nframes_t ostop = (dstop > min_stop) ? 0 : min_stop - dstop;
                return std::max(ostart, ostop);
        }
};

}} // namespace jill::util

#endif
