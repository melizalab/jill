#ifndef _TRIGGERED_ARF_THREAD_HH
#define _TRIGGERED_ARF_THREAD_HH

#include "../dsp/multichannel_data_thread.hh"
#include "arf_writer.hh"

namespace jill { namespace file {

/**
 * This implementation of the disk_thread class stores data in an ARF file, and
 * triggers off events in a MIDI port.
 */
class triggered_arf_thread : public dsp::multichannel_data_thread, protected arf_writer {
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
        triggered_arf_thread(std::string const & filename,
                             jack_port_t const * trigger_port,
                             nframes_t pretrigger_frames, nframes_t posttrigger_frames,
                             std::map<std::string,std::string> const & entry_attrs,
                             jill::jack_client * jack_client=0,
                             int compression=0);
        ~triggered_arf_thread();

        /** @see data_thread::log() */
        void log(std::string const &);

        /**
         * Place the object in recording mode.
         *
         * @note Not normally called externally except in testing.
         */
        void start_recording(nframes_t time);

        /**
         * Stop recording mode.
         *
         * @note Not normally called externally except in testing.
         */
        void stop_recording(nframes_t time);

        /** @see multichannel_data_thread::resize_buffer() */
        nframes_t resize_buffer(nframes_t, nframes_t);

protected:

        /** @see multichannel_data_thread::write() */
        void write(period_info_t const *);

private:
        jack_port_t const * const _trigger_port;
        const nframes_t _pretrigger;
        const nframes_t _posttrigger;

        bool _recording;
        nframes_t _last_offset; // track time since last offset
};

}}

#endif
