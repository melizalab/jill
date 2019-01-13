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
#include <iostream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/filesystem.hpp>

#include "../logging.hh"
#include "../zmq.hh"
#include "buffered_data_writer.hh"
#include "block_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;
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

buffered_data_writer::buffered_data_writer(std::unique_ptr<data_writer> writer, size_t buffer_size)
        : _state(Stopped),
          _writer(std::move(writer)),
          _buffer(new block_ringbuffer(buffer_size)),
          _context(zmq_init(1)), _socket(zmq_socket(_context, ZMQ_DEALER)),
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
        zmq_ctx_destroy(_context);
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
buffered_data_writer::data_ready()
{
        _ready.notify_one();
}


void
buffered_data_writer::xrun()
{
        // don't generate log message here
        __sync_bool_compare_and_swap(&_xrun, false, true);
}

void
buffered_data_writer::stop()
{
        // release condition variable to prevent deadlock
        if (__sync_bool_compare_and_swap(&_state, Running, Stopping))
                data_ready();
}


void
buffered_data_writer::reset()
{
        if (_state == Running)
                __sync_bool_compare_and_swap(&_reset, false, true);
}


void
buffered_data_writer::start()
{
        if (_state == Stopped) {
                _thread = std::thread(&buffered_data_writer::thread, this);
        }
        else {
                throw std::runtime_error("Tried to start already running writer thread");
        }
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
        // block until the buffer is empty
        std::lock_guard<std::mutex> lck(_lock);
        if (bytes > _buffer->size()) {
                _buffer->resize(bytes);
        }
        return _buffer->size();
}

void
buffered_data_writer::thread()
{
        data_block_t const * hdr;

        std::unique_lock<std::mutex> lck(_lock);
        _state = Running;
        _xrun = _reset = false;
        DBG << "started writer thread";

        while (true) {
                if (__sync_bool_compare_and_swap(&_xrun, true, false)) {
                        _writer->xrun();
                }
                hdr = _buffer->peek_ahead();
                if (!hdr) {
                        write_messages();
                        /* if ringbuffer empty and Stopping, exit loop */
                        if (_state == Stopping) {
                                break;
                        }
                        /* otherwise flush to disk and wait for more data */
                        else {
                                _writer->flush();
                                _ready.wait(lck,
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
        if (__sync_bool_compare_and_swap(&_reset, true, false)) {
                _writer->close_entry();
        }
        _writer->write(data, 0, 0);
        _buffer->release();
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
                std::vector<std::string> messages = zmq::recv(_socket);
                if (messages.size() >= 3) {
                        _writer->log(from_iso_string(messages[1]), messages[0], messages[2]);
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
        namespace fs = boost::filesystem;
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
