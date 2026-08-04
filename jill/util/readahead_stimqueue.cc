/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include "../logging.hh"
#include "readahead_stimqueue.hh"

using namespace jill::util;

readahead_stimqueue::readahead_stimqueue(iterator first, iterator last,
                                         nframes_t samplerate,
                                         bool loop)
        :  _first(first), _last(last), _it(first), _head(nullptr), _previous(nullptr),
           _samplerate(samplerate), _loop(loop), _running(true), _finished(false),
           _thread(&readahead_stimqueue::loop, this)
{}

void
readahead_stimqueue::stop()
{
        LOG << "stimulus queue terminated by stop()";
        {
                std::lock_guard<std::mutex> lck(_lock);
                _running = false;
        }
        _ready.notify_one();
}

readahead_stimqueue::~readahead_stimqueue()
{
        // a joinable thread reaching ~std::thread calls std::terminate
        stop();
        join();
}

void
readahead_stimqueue::join()
{
        // guarded so that an explicit join followed by the destructor, which
        // is the normal sequence, does not throw
        if (_thread.joinable())
                _thread.join();
}


/*
 * Threading notes: background thread locks mutex while it's working, and waits
 * on a condition variable when it's not (releasing the mutex).  RT threads call
 * head() and release(), both of which may need to notify the worker to load the
 * next stimulus. To keep these calls waitfree, the mutex is not locked prior to
 * signaling the condition variable.
 *
 * I'm not sure if _head is properly protected, because both release() can
 * modify it even when loop() has the lock. I think it's probably okay though,
 * because release() doesn't modify the underlying object, and loop() only
 * performs a simple assignment. The consumer needs to avoid calling release()
 * when head is null to avoid any weird issues like tearing, but it's unlikely
 * to do so because it should check this condition.
 */
void
readahead_stimqueue::loop()
{
        jill::stimulus_t * ptr;
        std::unique_lock<std::mutex> lck(_lock);

        while (_running) {
                // load data
                if (!_head) {
                        if (_it == _last) {
                                if (_loop) _it = _first;
                                else break;
                        }
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                        _head = ptr;
                        LOG << "pre-loaded next stim: " << ptr->name() << " (" << ptr->duration() << " s)";
                        _it += 1;
                }

                // read ahead
                if (_it != _last) {
                        ptr = *_it;
                        ptr->load_samples(_samplerate);
                }

                // wait for process thread to call release(). Calling stop()
                // will also break the wait
                _ready.wait(lck, [this]{ return (!_head || !_running);});
        }
        LOG << "end of stimulus list";
        // published last, so a caller that sees finished() also sees
        // everything the worker did before it
        _finished.store(true, std::memory_order_release);
}

jill::stimulus_t const *
readahead_stimqueue::head()
{
        // load once: testing and returning separately could observe two
        // different values, since release() may run concurrently
        stimulus_t * ptr = _head.load();
        if (ptr) return ptr;
        // It might be necessary to call this, but I'm not sure b/c loop()
        // checks for spurious/early release.
        // _ready.notify_one();
        return nullptr;
}

jill::stimulus_t const *
readahead_stimqueue::previous()
{
        stimulus_t * ptr = _previous.load();
        if (ptr) return ptr;
        // It might be necessary to call this, but I'm not sure b/c loop()
        // checks for spurious/early release.
        // _ready.notify_one();
        return nullptr;
}


void
readahead_stimqueue::release()
{
        // potential race condition with loop?
        _previous.store(_head.load());
        _head.store(nullptr);
        // signal loop to advance the iterator
        _ready.notify_one();
}
