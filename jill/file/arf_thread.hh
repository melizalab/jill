/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *! @file
 *! @brief Classes for writing data to ARF files
 *!
 */

#ifndef _ARF_THREAD_HH
#define _ARF_THREAD_HH

#include <pthread.h>
#include <vector>
#include <map>
#include <string>
#include "../types.hh"

/* TODO move these into the arf_thread class */
extern pthread_mutex_t disk_thread_lock;
extern pthread_cond_t  data_ready;

namespace jill { namespace file {

/*
 * Notes on synchronization
 *
 * During normal operation, the disk thread will read data from the ringbuffer
 * and write it to the current entry (or the prebuffer, if not in continuous
 * mode). It will lock disk_thread_lock while running, and wait on data_ready
 * after all available data has been handled.
 */

class arf_thread {

public:
        arf_thread(std::string const & filename,
                   std::vector<port_info_t> const * ports,
                   std::map<std::string,std::string> const * attrs,
                   jack_client * client, dsp::period_ringbuffer * ringbuf,
                   int compression=0);

        /** arf_thread destructor. locks disk_thread_lock while releasing file */
        ~arf_thread();

        /**
         *  Write a message to the arf log dataset. Thread-safe, may block.
         *
         *  @param msg       the message to write
         */
        void log(std::string const & msg);

        /** Increment the xrun counter */
        long xrun() { return _xruns++; }


        /** Start the thread */
        void start();

        /** Signal the thread to flush the rest of the data in the ringbuffer */
        void stop();

        /** Join the thread */
        void join();

        /** Thread entry point for continous writes */
        static void * write_continuous (void *);

protected:

        void open_arf(std::string const & filename);

        void new_entry(nframes_t frame, timeval const * timestamp);

        void new_datasets();

private:
        pthread_t _thread_id;                      // disk thread
        jack_client *_client;                      // used to log and look up some things
        dsp::period_ringbuffer *_ringbuf;          // the source of data (not owned)
        arf::file_ptr _file;                       // output file
        arf::entry_ptr _entry;                     // current entry (owned by thread)
        std::vector<port_info_t> const * _ports;   // the client's ports (not owned)
        std::map<std::string,std::string> const * _attrs;  // attributes for new entries
        std::vector<arf::packet_table_ptr> _dsets; // pointers to packet tables (owned)
        long _xruns;                               // number of xruns. incremented by other threads
        int _stop;                                 // flag to signal stop
        int _compression;                          // compression level for new datasets
};

// /**
//  * @ingroup iogroup
//  * @brief write data to an ARF (hdf5) file
//  *
//  * Uses ARF as a container for data. Each entry is an HDF5 group in the ARF
//  * file. Supports splitting across multiple files.
//  */
// class arffile : boost::noncopyable {

// public:
// 	typedef jack_default_audio_sample_t storage_type;

// 	/**
// 	 * Open a new or existing ARF file for writing.
// 	 *
// 	 * @param basename   the name of the file (or basename if max_size > 0)
// 	 * @param max_size   the maximum size the file can get, in
// 	 *                   MB. If zero or less, file is allowed to grow
// 	 *                   indefinitely. If nonnegative, files will be indexed, and
// 	 *                   when the file size exceeds this (checked after
// 	 *                   each entry is closed), a new file will be
// 	 *                   created.
// 	 */
// 	arffile(std::string const & basename);
//         ~arffile();

//         /**
//          * Open a new entry (closing current one if necessary). Sets required
//          * attributes and creates new datasets for each of the channels.
//          *
//          */
//         arf::entry_ptr new_entry(nframes_t frame, utime_t usec, nframes_t sampling_rate,
//                                  std::vector<port_info_t> const * channels=0,
//                                  std::map<std::string,std::string> const * attributes=0,
//                                  timeval const * timestamp=0);

//         /**
//          * Append data to a channel's dataset
//          */

//         /**
//          * @brief compare file size against max size and opens new file if needed
//          *
//          * @note the current file size as reported by the API may not reflect
//          * recently written data, so call file()->flush() if precise values are
//          * needed.
//          *
//          * @returns true iff a new file was opened
//          */
// 	bool check_filesize();

//         /** return name of current file */
//         std::string const & name() const {
//                 return _current_filename;
//         }

//         /** log message to current file */
//         void log(std::string const & msg);

// private:
//         std::string _base_filename;
//         hsize_t _max_size;      // in bytes
// 	int _file_index;
//         int _entry_index;
//         std::string _current_filename;

// 	arf::file_ptr _file;
//         arf::entry_ptr _entry;
//         std::vector<arf::packet_table_ptr> _dsets;
// };

}} // namespace jill::file

#endif
