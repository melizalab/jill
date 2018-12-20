/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _DATA_WRITER_HH
#define _DATA_WRITER_HH

#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include "types.hh"


namespace jill {

using timestamp_t = boost::posix_time::ptime;

/**
 * ABC for classes that write (or otherwise consume) multichannel sampled and
 * event data.
 *
 * The basic usage pattern is to call write() for each block of data. There can
 * be multiple channels (ids), but the blocks within each channel must be
 * ordered correctly.
 *
 * Data can be split into multiple entries by calling new_entry() or
 * close_entry() at appropriate points in the data stream.
 *
 * The log() and xrun() methods are provided for callers to store messages or
 * indications that the data may have gaps.
 */
class data_writer : boost::noncopyable {

public:
        virtual ~data_writer() = default;

        /** true if an entry is open for recording */
        virtual bool ready() const = 0;

        /**
         * Create a new entry, closing the previous one if necessary. The caller
         * is responsible for ensuring that all the data for a period have been
         * written so that the same amount of data is present for each channel.
         *
         * @param frame   the frame index at the start of the entry
         */
        virtual void new_entry(nframes_t frame) = 0;

        /**
         * Close the current entry. The caller is responsible for ensuring that
         * all the data for a period have been written so that the same amount
         * of data is present for each channel.
         */
        virtual void close_entry() = 0;

        /** Store a record that an xrun occurred in the file */
        virtual void xrun() = 0;

        /**
         * Write a block of data to disk. Looks up the appropriate channel.
         *
         * @pre ready() is true
         *
         * @param data  pointer to header and data for period
         * @param start if nonzero, only write frames >= start
         * @param stop  if nonzero, only write frames < stop. okay if stop > info->nframes
         */
        virtual void write(data_block_t const * data, nframes_t start, nframes_t stop) = 0;

        /**
         * Write a log message to the file. May be a noop.
         *
         * @param time    time of the event
         * @param source  the source of the event
         * @param message a descriptive message. should include information
         *                about the source
         */
        virtual void log(timestamp_t const & time,
                         std::string const & source,
                         std::string const & message) {}

        /**
         * Request data to be flushed to disk. Implementing classes must flush data
         * to disk on cleanup or at appropriate intervals, but this function is
         * provided so callers can request a flush when the system load is light.
         */
        virtual void flush() {}

};

}
#endif
