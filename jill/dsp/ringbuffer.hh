/*
 * JILL - C++ framework for JACK
 *
 * Based on virtual ringbuffer
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _RINGBUFFER_HH
#define _RINGBUFFER_HH

#include <algorithm>
#include <functional>
#include "../util/mirrored_memory.hh"

/**
 * @defgroup buffergroup Buffer data in a thread-safe manner
 *
 * A note on resource management. For native and aggregate/POD datatypes this is
 * not relevant, because all the data is stored in the ringbuffer. Objects *may*
 * contain references to other memory, but that memory needs to be allocated by the
 * object on construction (which will occur when the ringbuffer is initialized)
 * in order for the ringbuffer to be realtime safe.
 *
 * Then, when objects are pushed to the ringbuffer, the referenced data needs to
 * be copied into the preallocated memory. The push() method in dsp::ringbuffer
 * facilitates this by using std::copy, which will use the object's assignment
 * operator. Similarly, the pull() method uses std::copy, to avoid allocation,
 * to avoid having the memory overwritten by the writer thread after the objects
 * are released by the reader thread, and to avoid double freeing memory when
 * the ringbuffer is destroyed.
 */

namespace jill { namespace dsp {

namespace detail {

template <typename T>

// helper class for copying read
// TODO could be more clever with const traits
struct copyfrom {
        T* _buf;
        copyfrom(T * buf) : _buf(buf) {}
        std::size_t operator() (T const * src, std::size_t cnt, std::size_t index=0) {
                if (_buf) std::copy(src, src + cnt, _buf); // assume uses memcpy for POD
                return cnt;
        }
};

template <typename T>
struct copyto {
        T const * _buf;
        copyto(T const * buf) : _buf(buf) {}
        std::size_t operator() (T * dst, std::size_t cnt) {
                if (_buf) {
                        std::copy(_buf, _buf + cnt, dst);
                }
                // else {
                //         memset(dst, 0, cnt * sizeof(T));
                // }
                return cnt;
        }
};

}

std::size_t
inline next_pow2(std::size_t size) {
        std::size_t p2;
        for (p2 = 1U; 1U << p2 < size; ++p2);
        return 1U << p2;
}

/**
 * @ingroup buffergroup
 * @brief a lockfree ringbuffer
 *
 *  Many JILL applications will access an audio stream in both the real-time
 *  thread and a lower-priority main thread. This class template provides a
 *  type-safe, lock-free ringbuffer for moving data between threads. The
 *  emphasis is on ensuring that both operations always atomically access their
 *  pointers, preventing either thread from trespassing into the other's
 *  territory. Uses a virtual mirrored buffer trick to create two contiguous
 *  regions of memory, allowing reads and writes to use single calls (it also
 *  ensures that memory is aligned to cache lines). For zero-copy operations the
 *  class uses a visitor pattern, which ensures that indices remain in sync.
 *
 */
template <typename T>
class ringbuffer {
public:
        using data_type = T;
        using read_visitor_type = typename std::function<std::size_t (const data_type *, std::size_t)>;
        using write_visitor_type = typename std::function<std::size_t (data_type *, std::size_t)>;

        /**
         * Construct a ringbuffer with enough room to hold @a size
         * objects of type T.
         *
         * @param size The size of the ringbuffer (in objects)
         */
        explicit ringbuffer(std::size_t size)
                : _write_ptr(0), _read_ptr(0)
        {
                resize(size);
        }

        ~ringbuffer() = default;

        void resize(std::size_t size) {
                _buf.reset(new jill::util::mirrored_memory(next_pow2(size * sizeof(data_type)),
                                                           2,
                                                           true));
                _size_mask = this->size() - 1;
        }

        /// @return the size of the buffer (in objects)
        std::size_t size() const {
                return _buf->size() / sizeof(data_type);
        }

        /// @return the number of items that can be written to the ringbuffer
        std::size_t write_space() const {
                return _read_ptr + size() - _write_ptr;
        }

        /// @return the number of items that can be read from the ringbuffer
        std::size_t read_space() const {
                return _write_ptr - _read_ptr;
        };

        /**
         * Write data to the ringbuffer.  Uses std::copy, so object assignment
         * operator semantics matter. Specifically, make sure objects in the
         * ringbuffer own their resources.
         *
         * @param src Pointer to source buffer. If NULL, zeros are written to
         *            the buffer and the write pointer is advanced.
         * @param cnt The number of elements in the source buffer. Only as many
         *            elements as there is room will be written.
         *
         * @return The number of elements actually written
         */
        std::size_t push(data_type const * src, std::size_t cnt) {
                detail::copyto<data_type> copier(src);
                return push(copier, cnt);
        }
        std::size_t push(write_visitor_type data_fun, std::size_t cnt) {
                if (cnt > write_space())
                        cnt = write_space();
                cnt = data_fun(reinterpret_cast<data_type*>(buffer()) + write_offset(), cnt);
                // gcc-specific, use std::atomic?
                __sync_add_and_fetch(&_write_ptr, cnt);
                return cnt;
        }

        std::size_t push(data_type const & src) { return push(&src, 1); }

        /**
         * Read data from the ringbuffer. This version of the function
         * copies data to a destination buffer.  Uses std::copy (i.e., the
         * assignment operator).
         *
         * @param dest the destination buffer, which needs to be pre-allocated.
         *             if 0, does not write any data but still advances read pointer
         * @param cnt the number of elements to read (0 for all)
         *
         * @return the number of elements actually read
         */
        std::size_t pop(data_type * dest, std::size_t cnt=0) {
                detail::copyfrom<data_type> copier(dest);
                return pop(copier, cnt);
        }

        /**
         * Read data from the ringbuffer using a visitor function.
         *
         * @param data_fun The visitor function (@see read_visitor_type) NB: to avoid copying
         *                 the underlying object use boost::ref
         * @param cnt      The number of elements to process, or 0 for all
         *
         * @return the number of elements actually read
         */
        std::size_t pop(read_visitor_type data_fun, std::size_t cnt=0) {
                if (cnt==0 || cnt > read_space())
                        cnt = read_space();
                cnt = data_fun(buffer() + read_offset(), cnt);
                __sync_add_and_fetch(&_read_ptr, cnt);
                return cnt;
        }

        std::size_t write_offset() const {
                return _write_ptr & _size_mask;
        };

        std::size_t read_offset() const {
                return _read_ptr & _size_mask;
        };

        data_type * buffer() { return reinterpret_cast<data_type*>(_buf->buffer()); }
        data_type const * buffer() const { return reinterpret_cast<data_type const *>(_buf->buffer()); }


private:
        std::unique_ptr<jill::util::mirrored_memory> _buf;
        std::size_t _write_ptr;
        std::size_t _read_ptr;
        std::size_t _size_mask;
};

}} // namespace


#endif
