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
         * @param trigger_port        the source of trigger events
         * @param pretrigger_frames   the number of frames to record from before
         *                            trigger onset events
         * @param posttrigger_frames  the number of frames to record from after
         *                            trigger offset events
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param jack_client  optional, client used to look up time and samplerate
         * @param compression  the compression level for new datasets
         */
        triggered_data_writer(boost::shared_ptr<data_writer> writer,
                              std::string const & trigger_port,
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
