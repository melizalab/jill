/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _MIRRORED_MEMORY_HH
#define _MIRRORED_MEMORY_HH

#include <boost/noncopyable.hpp>

namespace jill { namespace util {

/**
 * Allocates memory with a contiguous virtual mirror. Reading and writing
 * operations that extend past the end of the allocated buffer will wrap around
 * to the beginning. This is extremely useful for ringbuffers because read and
 * write functions can access their space as a single unbroken array. Based on
 * virtual ringbuffer by Philip Howard (http://vrb.slashusr.org/)
 */
class mirrored_memory : boost::noncopyable
{
public:
        /** Request mirrored memory of at least size req_size bytes
         *
         * @param req_size the requested number of bytes. Will be rounded up to
         *                 multiple of the page size
         *
         * @param guard_size  requested size guard pages on either side of the
         *                 allocated memory. Not implemented
         *
         * @param lock_pages  try to lock the buffer in memory
         */
        mirrored_memory(std::size_t req_size=0, std::size_t guard_size=0, bool lock_pages=true);
        ~mirrored_memory();

        /** Pointer to the allocated buffer */
        char * buffer() { return _buf; }

        /** Read-only pointer to the allocated buffer */
        char const * buffer() const { return _buf; }

        /** Size of the buffer */
        std::size_t size() const { return _size; }

protected:

        /** total (virtual) size including guards */
        std::size_t total_size() const;

        char *_buf;
        std::size_t _size;

private:
        // only used for cleanup
        char *mem_ptr;
        char *upper_ptr;

};

}}

#endif
