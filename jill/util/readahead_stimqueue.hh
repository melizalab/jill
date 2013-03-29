#ifndef _READAHEAD_STIMQUEUE_HH
#define _READAHEAD_STIMQUEUE_HH

#include <pthread.h>
#include <vector>
#include <boost/ptr_container/ptr_vector.hpp>
#include "stimqueue.hh"

namespace jill { namespace util {

/**
 * Represents a two-element stimulus queue. One thread is pulling data off the
 * current stimulus, while another is making sure the next stimulus is loaded.
 *
 * Provides some synchronization between threads. The reader thread should check
 * ready(), use the frame buffer (advancing the read pointer as necessary), then
 * call release() when done. The (slow) writer thread should call enqueue()
 * whenever it has a stimulus loaded.  This will block until the reader calls
 * release(), at which point it will swap in the next stimulus.
 */
class readahead_stimqueue : public stimqueue {

public:
        readahead_stimqueue(nframes_t samplerate, bool loop=false);
        ~readahead_stimqueue();

        stimulus_t const * head() const { return _head; }
        bool release();
        void add(stimulus_t *, size_t nreps=1);
        void shuffle();

private:
        static void * thread(void * arg); // thread entry point
        void loop();                      // called by thread

        nframes_t const _samplerate;
        bool const _loop;
        boost::ptr_vector<stimulus_t> _stimuli;

        std::vector<stimulus_t *> _stimlist;
        std::vector<stimulus_t *>::iterator _it;
        stimulus_t const * _head;

        pthread_t _thread_id;
        pthread_mutex_t _lock;
        pthread_cond_t  _ready;

};

}} // namespace jill::util

#endif
