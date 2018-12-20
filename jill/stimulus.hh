/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _STIMULUS_HH
#define _STIMULUS_HH

#include <boost/noncopyable.hpp>
#include "types.hh"

namespace jill {

/**
 * ABC for a stimulus. Each stimulus has a name, a sampling rate, and a
 * length/duration. In order to present the stimulus, the samples need to be
 * accessible in a contiguous array, and may have to be generated or loaded from
 * disk.
 *
 */
class stimulus_t : boost::noncopyable {
public:
        virtual ~stimulus_t() = default;

        /** An identifier for the stimulus */
        virtual char const * name() const = 0;

        /** The number of frames in the stimulus */
        virtual nframes_t nframes() const = 0;

        /** The sampling rate of the stimulus */
        virtual nframes_t samplerate() const = 0;

        /** The duration of the stimulus */
        virtual float duration() const { return float(nframes()) / samplerate(); }

        /**
         * The buffer for the stimulus. May be 0 if not loaded. If not 0, the
         * length of the array will be equal to nframes()
         */
        virtual sample_t const * buffer() const = 0;

        /**
         * Load samples and resample as needed.  Only needs to be called if
         * buffer() == 0, but may be called multiple times.
         *
         * @param samplerate - the target samplerate, or 0 to use the file's rate
         */
        virtual void load_samples(nframes_t samplerate=0) {}

        friend std::ostream & operator<< (std::ostream &, stimulus_t const &);
};

}

#endif
