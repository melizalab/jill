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
        typedef std::vector<jill::stimulus_t *>::const_iterator const_iterator;

        stimset(nframes_t samplerate);

        /** add a stimulus to the stimset. The stimset now owns the object */
        void add(jill::stimulus_t * stim, size_t nreps=1);

        /**
         * Get the iterator for the stimset at the starting position.
         *
         * @param shuffle   if true, reshuffles the stimulus list
         * @return iterator at beginning position
         */
        const_iterator begin(bool shuffle=true);

        /** @return end position of stimulus list */
        const_iterator end();

private:

        boost::ptr_vector<jill::stimulus_t> _stimuli;
        std::vector<jill::stimulus_t *> _stimlist; // doesn't own
        nframes_t _samplerate;

};

}} // namespace jill::util

#endif
