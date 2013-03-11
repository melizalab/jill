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
         * Get the next item in the stimset. Guaranteed to be loaded and
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

}} // namespace jill::util

#endif
