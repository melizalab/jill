/*
 * JILL - C++ framework for JACK
 *
 * Allocates memory which has a contiguous virtual mirror. This is extremely
 * useful for ringbuffers because read and write functions can access their
 * space as a single unbroken array.  Based on virtual ringbuffer.
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dmeliza@uchicago.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <boost/noncopyable.hpp>

namespace jill { namespace util {

class mirrored_memory : boost::noncopyable
{
public:
        mirrored_memory(std::size_t req_size=0, std::size_t guard_size=0, bool lock_pages=true);
        ~mirrored_memory();

        char * buffer() { return lower_ptr; }
        char const * buffer() const { return lower_ptr; }
        std::size_t size() const { return _size; }

protected:

        // total size including guards
        std::size_t total_size() const;

        char *mem_ptr;
        char *upper_ptr;
        char *lower_ptr;
        std::size_t _size;

};

}}
