/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
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
#include <iosfwd>
#include <stdexcept>
#include <string>

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

/** The kinds of data moved through JILL. Corresponds to jack port types */
enum dtype_t {
        SAMPLED = 0,
        EVENT = 1,
        VIDEO = 2
};

/**
 * Represents a block of data and provides some help serializing it for use in
 * ringbuffers.
 *
 * This class does not fully encapsulate the data, but instead should be used as
 * a header that precedes the data. The header specifies the time of the data,
 * its type, and the size of two arrays that follow the header. The first array
 * gives the id (channel) of the data, and the second gives the data.
 *
 * For sampled data, the data is an array of sample_t elements representing a
 * time series starting at time. For event data, the data is an array of
 * (unsigned) chars describing the event. See midi.hh for the layout of this
 * data.
 *
 * The id() and data() members are only valid if the header precedes the two
 * data arrays.
 */
struct data_block_t {
        nframes_t time;         // the time of the block, in frames
        dtype_t dtype;          // the type of data in the block
        std::size_t sz_id;      // the number of bytes in the id
        std::size_t sz_data;    // the number of bytes in the data

        /** total size of the data, including header */
        std::size_t size() const { return sizeof(data_block_t) + sz_id + sz_data; }

        /** the id of the block, copied into a new string */
        std::string id() const {
                return std::string(reinterpret_cast<char const *>(this) + sizeof(data_block_t),
                                   sz_id);
        }

        /** pointer to the block's data */
        void const * data() const {
                return reinterpret_cast<char const *>(this) + sizeof(data_block_t) + sz_id;
        }

        /** number of frames in the block; always 1 for event data */
        nframes_t nframes() const {
                // TODO change if multiple events in a block
                return (dtype == SAMPLED) ? sz_data / sizeof(sample_t) : 1;
        }
}; // does this need to be packed?

/** Base type for all jill errors */
struct Error : public std::runtime_error {
        Error(std::string const & w) : std::runtime_error(w) { }
};

/** Type for jack errors */
struct JackError : public Error {
        JackError(std::string const & w) : Error(w) { }
};

/** Thrown for errors related to filesystem access */
struct FileError : public Error {
        FileError(std::string const & w) : Error(w) { }
};

}

#endif
