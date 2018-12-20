/*
 * JILL - C++ framework for JACK
 *
 * Allocates memory which has a contiguous virtual mirror. This is extremely
 * useful for ringbuffers because read and write functions can access their
 * space as a single unbroken array.  Based on virtual ringbuffer.
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <sys/mman.h>
#include <sys/shm.h>
#include <cstring>
#include <unistd.h>
#include <stdexcept>
#include "mirrored_memory.hh"

using namespace jill::util;
using std::size_t;

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

mirrored_memory::mirrored_memory(size_t arg_size, size_t guard_size, bool lock_pages)
{
        int shm_id;
        size_t page_size = getpagesize();

        // make sure size will not overflow size_t arithmetic
        if (arg_size > ( ( (~(size_t)0) >> 2 ) - page_size))
                throw std::out_of_range("Argument size exceeds address space");

        // round to multiple of page size
        if ( ! arg_size ) arg_size = 1;
        _size = arg_size + ( page_size - 1 );
        _size -= _size & ( page_size - 1 );

        // The mmap call ensures that there are two contiguous pages in virtual
        // address space.
        mem_ptr = (char*) mmap (nullptr,
                        _size + _size,
                        PROT_NONE,
                        MAP_ANONYMOUS | MAP_PRIVATE,
                        -1,
                        0);

        if (mem_ptr == MAP_FAILED)
                throw std::runtime_error("anonymous mmap failed");

        if (lock_pages)
                mlock(mem_ptr, total_size());

        _buf = mem_ptr;
        upper_ptr = _buf + _size;

        // this seems like a potential race condition. maybe this is what the
        // guard pages are for?
        if ( 0 > munmap( _buf, _size + _size ) )
                throw std::runtime_error("munmap failed");

        //-- Create a private shared memory segment.
        if ( 0 > ( shm_id = shmget( IPC_PRIVATE, _size, IPC_CREAT | 0700 ) ) )
                throw std::runtime_error("shared memory allocation failed");

        if ( _buf != shmat( shm_id, _buf, 0 ) ) {
                shmctl( shm_id, IPC_RMID, nullptr );
                throw std::runtime_error("failed to attach lower pointer to shared memory");
        }
        if ( upper_ptr != shmat( shm_id, upper_ptr, 0 ) ) {
                shmctl( shm_id, IPC_RMID, nullptr );
                throw std::runtime_error("failed to attach upper pointer to shared memory");
        }

        if ( 0 > shmctl( shm_id, IPC_RMID, nullptr ) )
                throw std::runtime_error("failed to tag shared memory for deletion");

        // zero out the memory
        memset(_buf, 0, _size);
}

mirrored_memory::~mirrored_memory()
{
        // clean up mmaps and shm attaches. all these calls are safe to make
        // even if they failed or were already called in the constructor
        munlock(mem_ptr, total_size());
        munmap(mem_ptr, total_size());
        shmdt(upper_ptr);
        shmdt(_buf);
}

size_t
mirrored_memory::total_size() const
{
        return _size + _size;
}
