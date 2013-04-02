#ifndef _TRIGGERED_DATA_WRITER_HH
#define _TRIGGERED_DATA_WRITER_HH

#include "buffered_data_writer.hh"

namespace jill { namespace dsp {

/**
 * This implementation of the disk_thread class stores data in an ARF file, and
 * triggers off events in a MIDI port.
 */
class triggered_data_writer : public buffered_data_writer {
        friend class triggered_data_writer_test;
public:
        /**
         * Initialize an ARF writer thread.
         *
         * @param filename            the file to write to
         * @param trigger_port        he source of trigger events
         * @param pretrigger_frames   the number of frames to record from before
         *                            trigger onset events
         * @param posttrigger_frames  the number of frames to record from after
         *                            trigger offset events
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param jack_client  optional, client used to look up time and samplerate
         * @param compression  the compression level for new datasets
         */
        triggered_data_writer(boost::shared_ptr<data_writer> writer,
                              jack_port_t const * trigger_port,
                              nframes_t pretrigger_frames, nframes_t posttrigger_frames);

        ~triggered_data_writer();

        /** @see buffered_data_writer::resize_buffer() */
        nframes_t resize_buffer(nframes_t, nframes_t);

        /**
         * Close entry at event_time plus the post-trigger frames. It's okay
         * to call from another thread before the data writer thread has reached
         * event_time in the data stream.
         */
        void close_entry(nframes_t event_time);

protected:

        /** @see buffered_data_writer::write() */
        void write(period_info_t const *);

private:
        /** Place the object in recording mode. */
        void start_recording(nframes_t time);

        jack_port_t const * const _trigger_port;
        const nframes_t _pretrigger;
        const nframes_t _posttrigger;

        bool _recording;
        nframes_t _last_offset; // track time since last offset
};

}}

#endif
