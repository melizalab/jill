#include <iostream>
#include <vector>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/filesystem.hpp>

#include "../logging.hh"
#include "../zmq.hh"
#include "buffered_data_writer.hh"
#include "block_ringbuffer.hh"

using namespace jill;
using namespace jill::dsp;
using std::size_t;
using std::string;

buffered_data_writer::buffered_data_writer(boost::shared_ptr<data_writer> writer, nframes_t buffer_size)
        : _state(Stopped),
          _writer(writer),
          _buffer(new block_ringbuffer(buffer_size)),
          _context(zmq_init(1)), _socket(zmq_socket(_context, ZMQ_DEALER)),
          _logger_bound(false)
{
        DBG << "buffered_data_writer initializing";
        pthread_mutex_init(&_lock, 0);
        pthread_cond_init(&_ready, 0);
}

buffered_data_writer::~buffered_data_writer()
{
        DBG << "buffered_data_writer closing";
        // need to make sure synchrons are not in use
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
        // pthread_cancel(_thread_id);
        pthread_mutex_destroy(&_lock);
        pthread_cond_destroy(&_ready);
        zmq_close(_socket);
        zmq_term(_context);
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
        if (pthread_mutex_trylock (&_lock) == 0) {
                pthread_cond_signal (&_ready);
                pthread_mutex_unlock (&_lock);
        }
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
        __sync_bool_compare_and_swap(&_state, Running, Stopping);
        // release condition variable to prevent deadlock
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
                int ret = pthread_create(&_thread_id, NULL, buffered_data_writer::thread, this);
                if (ret != 0)
                        throw std::runtime_error("Failed to start writer thread");
        }
        else {
                throw std::runtime_error("Tried to start already running writer thread");
        }
}

void
buffered_data_writer::join()
{
        pthread_join(_thread_id, NULL);
}

size_t
buffered_data_writer::request_buffer_size(size_t bytes)
{
        // block until the bufferis empty
        pthread_mutex_lock(&_lock);
        if (bytes > _buffer->size()) {
                _buffer->resize(bytes);
        }
        pthread_mutex_unlock(&_lock);
        return _buffer->size();
}

void *
buffered_data_writer::thread(void * arg)
{
        buffered_data_writer * self = static_cast<buffered_data_writer *>(arg);
        data_block_t const * hdr;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&self->_lock);
        self->_state = Running;
        self->_xrun = self->_reset = false;
        INFO << "started writer thread";

        while (1) {
                if (__sync_bool_compare_and_swap(&self->_xrun, true, false)) {
                        self->_writer->xrun();
                }
                hdr = self->_buffer->peek_ahead();
                if (hdr == 0) {
                        /* if ringbuffer empty and Stopping, close loop */
                        if (self->_state == Stopping) {
                                break;
                        }
                        /* otherwise flush to disk and wait for more data */
                        else {

                                // TODO write log messages
                                self->write_messages();
                                self->_writer->flush();
                                pthread_cond_wait (&self->_ready, &self->_lock);
                        }
                }
                else {
                        self->write(hdr);
                }
        }
        self->_writer->close_entry();
        pthread_mutex_unlock(&self->_lock);
        self->_state = Stopped;
        INFO << "exited writer thread";
        return 0;
}

void
buffered_data_writer::write(data_block_t const * data)
{
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
                int64_t more = 1;
                size_t more_size = sizeof(more);
                std::vector<zmq::msg_ptr_t> messages;
                while (more) {
                        zmq::msg_ptr_t message = zmq::msg_init();
                        int rc = zmq_recv (_socket, message.get(), ZMQ_NOBLOCK);
                        if (rc != 0) return;
                        messages.push_back(message);
                        zmq_getsockopt (_socket, ZMQ_RCVMORE, &more, &more_size);
                }
                if (messages.size() >= 3) {
                        _writer->log(from_iso_string(zmq::msg_str(messages[1])),
                                     zmq::msg_str(messages[0]),
                                     zmq::msg_str(messages[2]));
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
