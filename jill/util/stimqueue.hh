#ifndef _STIMQUEUE_HH
#define _STIMQUEUE_HH

#include <boost/noncopyable.hpp>
#include "../stimulus.hh"

namespace jill { namespace util {

/**
 * Represents a thread-safe stimulus queue. One thread is reading data from the
 * stimulus at the head of the queue, while an internal thread takes care of
 * loading stimuli.
 */
class stimqueue : boost::noncopyable {

public:
        virtual ~stimqueue() {}

        /**
         * Get a pointer to the current position in the current stimulus.
         *
         * @note for reader thread, wait-free
         *
         * @return ponter to current position or nullptr if no stimulus is
         * available.
         */
        // virtual sample_t const * buffer() const = 0;

        /**
         * Get the stimulus currently at the head of the queue.
         *
         * @note for reader thread, wait-free
         *
         * @return pointer to current stimulus, or nullptr if no stimulus is
         * available
         */
        virtual stimulus_t const * head() const = 0;

        /**
         * Release the current stimulus and notify the queue to load a new one.
         *
         * @note for reader thread, wait-free
         *
         * @return true if the stimulus was released
         */
        virtual bool release() = 0;

        /**
         * Add a stimulus to the queue.  May reset the queue to the beginning;
         * may block.
         *
         * @note for the writer thread
         *
         * @param stim  pointer to stimulus object. samples do not need to be
         * loaded yet. The queue will own the object.
         * @param nreps  the number of times to play the stimulus in each block
         */
        virtual void add(stimulus_t * stim, size_t nreps=1) = 0;

        /**
         * Shuffle the stimulus list and start at the beginning. May block.
         *
         * @param for the writer thread
         */
        virtual void shuffle() {}

};

}} // namespace jill::util

#endif
