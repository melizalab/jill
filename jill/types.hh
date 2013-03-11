/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _TYPES_HH
#define _TYPES_HH

#include <boost/noncopyable.hpp>
#include <jack/types.h>
#include <jack/transport.h>
#include <arf/types.hpp>
#include <stdexcept>

/**
 * @file types.hh
 * @brief Data types and forward declarations.
 */
namespace jill {

// forward declare some jill classes
class jack_client;

namespace dsp {
        class period_ringbuffer;
}

/** The data type holding samples. Inherited from JACK */
typedef jack_default_audio_sample_t sample_t;
/** The data type holding information about frame counts. Inherited from JACK */
typedef jack_nframes_t nframes_t;
/** Data type for microsecond time information */
typedef jack_time_t utime_t;
/** A data type holding extended position information. Inherited from JACK */
typedef jack_position_t position_t;

/** interface for a stimulus */
struct stimulus_t : boost::noncopyable {

        /** An identifier for the stimulus */
        virtual std::string const & path() const = 0;

        virtual nframes_t nframes() const = 0;
        virtual nframes_t samplerate() const = 0;
        float duration() const { return float(nframes()) / samplerate(); }

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
        virtual void load_samples(nframes_t samplerate=0) = 0;

};

/** Type for jack errors */
struct JackError : public std::runtime_error {
        JackError(std::string const & w) : std::runtime_error(w) { }
};

/** Thrown for errors related to filesystem access */
struct FileError : public std::runtime_error {
	FileError(std::string const & w) : std::runtime_error(w) { }
};

}

#endif
