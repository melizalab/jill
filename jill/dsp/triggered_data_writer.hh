#ifndef _TRIGGERED_DATA_WRITER_HH
#define _TRIGGERED_DATA_WRITER_HH

#include "buffered_data_writer.hh"

namespace jill { namespace dsp {

/**
 * This implementation of the disk_thread class derives from
 * buffered_data_writer, adding the logic needed to trigger recordings from
 * events in a MIDI port.
 *
 * The consumer thread will monitor the trigger channel for note_on, note_off,
 * stim_on, and stim_off events.  If not currently recording, an onset event
 * will cause the consumer to start a new entry; if recording, offset events
 * cause the consumer to close the current entry.
 *
 * "prebuffering" is provided, so that data before an onset event can be written
 * to disk.  Similarly, the object can be configured to continue writing for
 * some time after an offset event.
 */
class triggered_data_writer : public buffered_data_writer {
        friend class triggered_data_writer_test;
public:
        /**
         * Initialize buffered writer.
         *
         * @param writer              the sink for the data
         * @param trigger_port        id of channel carrying of trigger events
         * @param pretrigger_frames   the number of frames to record from before
         *                            trigger onset events
         * @param posttrigger_frames  the number of frames to record from after
         *                            trigger offset events
         */
        triggered_data_writer(boost::shared_ptr<data_writer> writer,
                              std::string trigger_port,
                              nframes_t pretrigger_frames, nframes_t posttrigger_frames);

        ~triggered_data_writer();

protected:

        /** @see buffered_data_writer::write() */
        void write(data_block_t const *);

private:
        /** start recording at time - pretrigger */
        void start_recording(nframes_t time);
        /** stop recording at time + posttrigger */
        void stop_recording(nframes_t time);

        std::string _trigger_port;
        const nframes_t _pretrigger;
        const nframes_t _posttrigger;

        bool _recording;        // flag to track whether data are being written
        nframes_t _last_offset; // track time since last offset
};

}}

#endif
