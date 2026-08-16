/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <chrono>
#include <iostream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <filesystem>

#include "../logging.hh"
#include "../net/zmq.hh"
#include "buffered_data_writer.hh"
#include "block_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;

using namespace jill::net;
using std::size_t;
using std::string;

/*
 * # Notes on buffered data_thread objects
 *
 * Wait-free functions are provided to the producer thread by using a
 * ringbuffer. The consumer thread pulls data off the ringbuffer and passes it
 * to the data_writer object. If there's no data in the ringbuffer, the consumer
 * writes any queued log messages and requests the writer to flush data to disk.
 * It then waits for a condition variable that's flagged when the consumer calls
 * push().
 *
 * Any thread may signal the consumer thread to start a new entry or to mark the
 * current entry with an xrun indicator by calling reset() or xrun(). These
 * functions use gcc atomic primitives to update the _reset and _xrun flags.
 * Similarly, calls to stop() atomically update the _state variable so that
 * calls to push() no longer add data to the ringbuffer and so that the consumer
 * thread exits when the ringbuffer is fully flushed.
 */

buffered_data_writer::buffered_data_writer(std::unique_ptr<data_writer> writer,
                                           size_t buffer_size,
                                           std::chrono::milliseconds poll_interval)
        : _state(Stopped),
          _writer(std::move(writer)),
          _buffer(new block_ringbuffer(buffer_size)),
          _dirty(false),
          _poll_interval(poll_interval),
          _socket(zmq::context::socket(ZMQ_DEALER)),
          _logger_bound(false)
{
        DBG << "buffered_data_writer initializing";
}

buffered_data_writer::~buffered_data_writer()
{
        DBG << "buffered_data_writer closing";
        // need to make sure synchrons are not in use
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
        zmq_close(_socket);
}

void
buffered_data_writer::push(nframes_t time, dtype_t dtype, char const * id,
                           size_t size, void const * data)
{
        if (_state != Stopping) {
                if (_buffer->push(time, dtype, id, size, data) == 0) {
                        xrun();
                }
        }
}

void
buffered_data_writer::xrun()
{
        // don't generate log message here
        bool expected = false;
        _xrun.compare_exchange_strong(expected, true);
}

void
buffered_data_writer::stop()
{
        /* Begins the process of shutting down the writer. If the writer is
         * Running, switches the state to Stopping (which drops any further data
         * sent to push()) and releases the _ready condition variable so that
         * the writer thread flushes any remaining data. If the writer is
         * Stopped, the state is still switched to Stopping to avoid an
         * inconsistent state if the thread is in the process of starting - this
         * can happen if a signal is delivered during startup.
         *
         * Note that the state has to change while the lock is held. The writer
         * thread evaluates its wait predicate under this lock and then enqueues
         * on the condition variable; those two steps are atomic only with
         * respect to other holders of the lock. Changing the state without it
         * lets the change land in between, so the predicate reads the old value
         * and the notification finds no waiter and is dropped -- after which
         * the thread sleeps forever and join() never returns. Measured at
         * roughly one run in three of test_data_writer.
         *
         * stop() has no next push, which is why needs to take a lock to ensure
         * the notification isn't lost. This also means it must never be called
         * from a signal handler.
         */
        {
                std::lock_guard<std::mutex> lck(_lock);
                state_t running = Running;
                if (!_state.compare_exchange_strong(running, Stopping)) {
                        state_t stopped = Stopped;
                        _state.compare_exchange_strong(stopped, Stopping);
                }
        }
        _ready.notify_one();
}


void
buffered_data_writer::reset()
{
        if (_state == Running) {
                bool expected = false;
                _reset.compare_exchange_strong(expected, true);
        }
}


void
buffered_data_writer::start()
{
        if (_state == Stopping) {
                // stop() was called before we got here; there is nothing to run
                return;
        }
        if (_state != Stopped) {
                throw std::runtime_error("Tried to start already running writer thread");
        }
        // Move to Running before launching, not from inside the thread, so that
        // stop() has something to act on if there's a signal during startup.
        _state = Running;
        _xrun = _reset = false;
        _thread = std::thread(&buffered_data_writer::thread, this);
}

void
buffered_data_writer::join()
{
        if (_thread.joinable())
                _thread.join();
}

size_t
buffered_data_writer::request_buffer_size(size_t bytes)
{
        /* Called from the JACK buffer size callback, which the server delivers
         * on its notification thread with the engine stopped -- so blocking and
         * allocating here are both fine, and no process callback is running to
         * push while this works.
         *
         * Grows only. A larger period fits in a buffer sized for a longer span
         * of audio, and shrinking would mean throwing away headroom to no end.
         */
        std::lock_guard<std::mutex> lck(_lock);
        if (bytes <= _buffer->size()) {
                return _buffer->size();
        }
        /* resize() discards, so anything still buffered has to reach the file
         * first. Taking the lock already implies the writer thread is parked,
         * and it only parks after draining and flushing -- but that is a
         * property of the loop above rather than a promise, and the cost of
         * being wrong is a silent hole in a recording. Drain explicitly. */
        data_block_t const * hdr;
        while ((hdr = _buffer->peek()) != nullptr) {
                write(hdr);
        }
        _writer->flush();
        _dirty = false;
        _buffer->resize(bytes);
        return _buffer->size();
}

void
buffered_data_writer::thread()
{
        data_block_t const * hdr;

        std::unique_lock<std::mutex> lck(_lock);
        // _state and the flags are set by start() before this thread is
        // launched, so that a stop() racing with startup cannot be lost
        DBG << "started writer thread";

        while (true) {
                bool had_xrun = true;
                if (_xrun.compare_exchange_strong(had_xrun, false)) {
                        _writer->xrun();
                }
                hdr = _buffer->peek_ahead();
                if (!hdr) {
                        write_messages();
                        /* if ringbuffer empty and Stopping, exit loop */
                        if (_state == Stopping) {
                                break;
                        }
                        /* otherwise flush anything outstanding and sleep. The
                         * flush is guarded because this pass now also happens
                         * on an idle timer, and flushing an unchanged file
                         * every interval would be pure overhead. */
                        else {
                                if (_dirty) {
                                        _writer->flush();
                                        _dirty = false;
                                }
                                _ready.wait_for(lck, _poll_interval,
                                                [this]{ return(_state == Stopping || _buffer->peek()); });
                        }
                }
                else {
                        write(hdr);
                }
        }
        _writer->close_entry();
        _state = Stopped;
        DBG << "exited writer thread";
}

void
buffered_data_writer::write(data_block_t const * data)
{
        // do we need to check that a complete period has been written?
        bool pending_reset = true;
        if (_reset.compare_exchange_strong(pending_reset, false)) {
                _writer->close_entry();
        }
        _writer->write(data, 0, 0);
        _buffer->release();
        _dirty = true;
}

void
buffered_data_writer::write_messages()
{
        using namespace boost::posix_time;

        // only record a limited number of messages on any given pass, in case
        // there's a huge backlog in the queue
        static const int max_messages = 100;
        if (!_logger_bound) return;
        for (int i = 0; i < max_messages; ++i) {
                // expect a three-part message: source, timestamp, message
                std::vector<std::string> messages = zmq::recv(_socket, ZMQ_DONTWAIT);
                if (messages.empty()) {
                        // nothing left; the loop used to run all hundred
                        // iterations regardless, which was merely wasteful when
                        // it ran on a signal and is worse now that it runs on a
                        // timer
                        break;
                }
                if (messages.size() >= 3) {
                        _writer->log(from_iso_string(messages[1]), messages[0], messages[2]);
                        _dirty = true;
                }
        }
}

void
buffered_data_writer::bind_logger(std::string const & server_name)
{
        if (_logger_bound) {
                DBG << "already bound to " << server_name;
                return;
        }
        namespace fs = std::filesystem;
        std::ostringstream endpoint;
        fs::path path("/tmp/org.meliza.jill");
        path /= server_name;
        if (!fs::exists(path)) {
                fs::create_directories(path);
        }
        path /= "msg";
        endpoint << "ipc://" << path.string();
        if (zmq_bind(_socket, endpoint.str().c_str()) < 0) {
                LOG << "unable to bind to endpoint " << endpoint.str();
        }
        else {
                INFO << "logger bound to " << endpoint.str();
                _logger_bound = true;
        }
}
