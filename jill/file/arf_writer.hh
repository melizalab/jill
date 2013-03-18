#ifndef _ARF_WRITER_HH
#define _ARF_WRITER_HH

#include <vector>
#include <map>
#include <string>
#include <arf/types.hpp>

#include "multichannel_writer.hh"

namespace jill { namespace file {

/**
 * @brief Storage format for log messages
 */
struct message_t {
        boost::int64_t sec;
        boost::int64_t usec;
        char const * message;       // descriptive
};

/**
 * @brief Storage format for event data
 */
struct event_t {
        boost::uint32_t start;  // relative to entry start
        boost::uint8_t status;  // see jill::event_t::midi_type
        char const * message;   // message (hex encoded for standard midi status)
};

/**
 * This implementation of the disk_thread class stores data in an ARF file.
 */
class arf_writer : public multichannel_writer {
public:
        /**
         * Initialize an ARF writer thread.
         *
         * @param filename     the file to write to
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param jack_client  optional, client used to look up time and samplerate
         * @param trigger_port optional, use this port to start/stop entries
         * @param compression  the compression level for new datasets
         */
        arf_writer(std::string const & filename,
                   std::map<std::string,std::string> const & entry_attrs,
                   jill::jack_client *jack_client=0,
                   jack_port_t *trigger_port=0,
                   int compression=0);
        virtual ~arf_writer();

        void log(std::string const & msg);

protected:
        void write(period_info_t const * info);
        void flush();

private:
        typedef std::map<std::string, arf::packet_table_ptr> dset_map_type;


        // would be nice to have a proxy for this to allow stream syntax
        void _do_log(std::string const & msg);     // no lock

        void _get_last_entry_index();
        void new_entry(nframes_t sample_count);
        // look up dataset, creating it as needed
        dset_map_type::iterator get_dataset(std::string const & name, bool is_sampled);

        // owned resources
        arf::file_ptr _file;                       // output file
        arf::packet_table_ptr _log;                // log dataset
        arf::entry_ptr _entry;                     // current entry (owned by thread)
        dset_map_type _dsets;                      // pointers to packet tables (owned)
        int _compression;                          // compression level for new datasets
        std::string _agent_name;                   // used in log messages

        // local state
        nframes_t _entry_start;                    // offset sample counts
        nframes_t _period_start;                   // assign chunks to channels
        std::size_t _entry_idx;                    // manage entry numbering
        std::size_t _channel_idx;                  // index channel

        // unowned resources
        std::map<std::string,std::string> const & _attrs;  // attributes for new entries
        jill::jack_client *_client;                        //
        jack_port_t *_trigger;                             // if set, breaks up entries
};

}}

#endif
