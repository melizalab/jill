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
#ifndef _STIMULUS_HH
#define _STIMULUS_HH

#include "types.hh"

namespace jill {

/** interface for a stimulus */
class stimulus_t : boost::noncopyable {
public:
        virtual ~stimulus_t() {}

        /** An identifier for the stimulus */
        virtual char const * name() const = 0;

        virtual nframes_t nframes() const = 0;
        virtual nframes_t samplerate() const = 0;
        virtual float duration() const { return float(nframes()) / samplerate(); }

        /**
         * @return framebuffer for stimulus, or 0 if not loaded. Length will be
         * equal to @a nframes()
         */
        virtual sample_t const * buffer() const = 0;

        /**
         * Load samples and resample as needed.
         *
         * @param samplerate - the target samplerate, or 0 to use the file's rate
         */
        virtual void load_samples(nframes_t samplerate=0) {}

        friend std::ostream & operator<<(std::ostream &, stimulus_t const &);
};

}

#endif
