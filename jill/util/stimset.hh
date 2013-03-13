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
#include <boost/ptr_container/ptr_vector.hpp>
#include "../stimulus.hh"

namespace jill { namespace util {

/** represents the end of the list of stimuli */
struct nullstim : public jill::stimulus_t {
        char const * name() const { return "end of queue"; }
        nframes_t nframes() const { return 0; }
        nframes_t samplerate() const { return 0; }
        float duration() const { return 0.0; }
        sample_t const * buffer() const { return 0; }
};

/**
 * Represents a list of stimuli to be presented. Stimuli can be present in the
 * list more than once, and the list can be shuffled.  Each element of the list
 * is released in order.
 */
class stimset : boost::noncopyable {

public:
        typedef std::vector<jill::stimulus_t *>::iterator iterator;
        static const nullstim end;

        stimset(nframes_t samplerate);

        /** add a stimulus to the stimset. The stimset now owns the object */
        void add(jill::stimulus_t * stim, size_t nreps=1);

        /** shuffle the stimulus list and start at the beginning */
        void shuffle();

        /**
         * Get the next item in the stimset. Blocks until stimulus is loaded and
         * resampled.
         *
         * @return pointer to next stimulus, or an instance of nullstim if at the end of the list.
         * Will only return nullstim once and then returns to the top.
         */
        jill::stimulus_t const * next();

private:

        boost::ptr_vector<jill::stimulus_t> _stimuli;
        std::vector<jill::stimulus_t *> _stimlist; // doesn't own
        iterator _it;
        nframes_t _samplerate;

};

}} // namespace jill::util

#endif
