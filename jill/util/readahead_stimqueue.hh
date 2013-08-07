#ifndef _READAHEAD_STIMQUEUE_HH
#define _READAHEAD_STIMQUEUE_HH

#include <pthread.h>
#include <vector>
#include <boost/shared_ptr.hpp>
#include "stimqueue.hh"

namespace jill {

class event_logger;

namespace util {

/**
 * An implementation of stimqueue that provides a background thread for loading
 * data from disk and resampling.
 */
class readahead_stimqueue : public stimqueue {

public:
        typedef std::vector<stimulus_t *>::iterator iterator;
        typedef std::vector<stimulus_t *>::const_iterator const_iterator;

        readahead_stimqueue(iterator first, iterator last,
                            nframes_t samplerate,
                            bool loop=false);
        ~readahead_stimqueue();

        stimulus_t const * head();
        void release();
        void stop();
        void join();

private:
        static void * thread(void * arg); // thread entry point
        void loop();                      // called by thread

        iterator const _first;
        iterator const _last;
        iterator _it;                             // current position
        stimulus_t * _head;

        nframes_t const _samplerate;
        bool const _loop;

        pthread_t _thread_id;
        pthread_mutex_t _lock;
        pthread_cond_t  _ready;

};

}} // namespace jill::util

#endif
