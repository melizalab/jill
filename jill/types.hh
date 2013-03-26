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

#include <jack/types.h>
#include <jack/transport.h>
#include <arf/types.hpp>
#include <stdexcept>

#ifndef NDEBUG
#include <iostream>
#endif

/**
 * @file types.hh
 * @brief Data types and forward declarations.
 */
namespace jill {

// forward declare some jill classes
class jack_client;

/** The data type holding samples. Inherited from JACK */
typedef jack_default_audio_sample_t sample_t;
/** The data type holding information about frame counts. Inherited from JACK */
typedef jack_nframes_t nframes_t;
/** Data type for microsecond time information */
typedef jack_time_t utime_t;
/** A data type holding extended position information. Inherited from JACK */
typedef jack_position_t position_t;

/**
 * A data structure storing information about a period. Contains information
 * about timestamp, and frame count, as well as a void pointer that can be used
 * to reference additional data.
 */
struct period_info_t {
        nframes_t time;    // time of the current period
        nframes_t nframes; // number of frames in period
        // NB: requiring const is somewhat restrictive; may change
        void const * arg;  // pointer to external data

        /** The size of the period in bytes */
        std::size_t bytes() const { return nframes * sizeof(sample_t); }
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
