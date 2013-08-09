#ifndef _DATA_SOURCE_HH
#define _DATA_SOURCE_HH

#include <boost/noncopyable.hpp>
#include "types.hh"

namespace jill {

/**
 * ABC representing a general data source. The interface mostly consists of
 * functions related to time, which is measured in terms of samples and seconds.
 */
class data_source : boost::noncopyable {

public:
        virtual ~data_source() {}

        /** An identifier for the data source */
        virtual char const * name() const = 0;

	/** The sample rate of the data */
	virtual nframes_t sampling_rate() const = 0;

	/** The current frame in the data stream (since client start) */
	virtual nframes_t frame() const = 0;

        /** Convert microsecond time to frame count */
        virtual nframes_t frame(utime_t) const = 0;

        /** Convert frame count to microseconds */
        virtual utime_t time(nframes_t) const = 0;

        /** Get current time in microseconds */
        virtual utime_t time() const = 0;
};

}

#endif
