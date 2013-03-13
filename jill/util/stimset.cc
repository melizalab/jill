/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <algorithm>
#include "stimset.hh"

using namespace jill::util;

// static member initialization
const nullstim stimset::end;

stimset::stimset(nframes_t samplerate)
        : _samplerate(samplerate)
{}

void
stimset::add(jill::stimulus_t * stim, size_t nreps)
{
        _stimuli.push_back(stim);
        for (size_t i = 0; i < nreps; ++i) {
                _stimlist.push_back(stim);
        }
        _it = _stimlist.begin();
}

void
stimset::shuffle()
{
        std::random_shuffle(_stimlist.begin(), _stimlist.end());
        _it = _stimlist.begin();
}

jill::stimulus_t const *
stimset::next()
{
        if (_it == _stimlist.end()) {
                _it = _stimlist.begin();
                return &end;
        }
        else {
                jill::stimulus_t *ret = *_it;
                ret->load_samples(_samplerate);
                ++_it;
                return ret;
        }
}
