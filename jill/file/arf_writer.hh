#ifndef _ARF_WRITER_HH
#define _ARF_WRITER_HH

#include <boost/noncopyable.hpp>
#include <vector>
#include <map>
#include <string>
#include <arf/types.hpp>

#include "../types.hh"

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
 * Class for storing data in an ARF file. It's intended as a mixin for data
 * writing threads (use protected inheritance)
 */
class arf_writer : boost::noncopyable {
public:
        typedef std::map<std::string, arf::packet_table_ptr> dset_map_type;

        /**
         * Initialize an ARF writer.
         *
         * @param filename     the file to write to
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param jack_client  optional, client used to look up time and samplerate
         * @param compression  the compression level for new datasets
         */
        arf_writer(std::string const & filename,
                   std::map<std::string,std::string> const & entry_attrs,
                   jill::jack_client * jack_client=0,
                   int compression=0);
        ~arf_writer();

        /** Create a new entry starting at frame_count */
        void new_entry(nframes_t frame_count);

        /** Close the current entry */
        void close_entry();

        /**
         * Look up dataset in current entry, creating as needed.
         *
         * @param name         the name of the dataset (channel)
         * @param is_sampled   whether the dataset holds samples or events
         * @return derefable iterator for appropriate dataset
         */
        dset_map_type::iterator get_dataset(std::string const & name, bool is_sampled);

        /**
         * Write a message to the log. Not thread safe.
         */
        void log(std::string const & msg);

        /**
         * Write a period to disk. Looks up the appropriate channel.
         *
         * @param info  pointer to header and data for period
         * @param start if nonzero, only write frames >= start
         * @param stop  if nonzero, only write frames < stop
         *
         * @pre the entry for storing the data has been created
         */
        void write(period_info_t const * info, nframes_t start=0, nframes_t stop=0);

        /**
         * Store a record than an xrun occurred in the file
         */
        void xrun();

        /** The number of channels received in the current period */
        nframes_t channels() const { return _channel_idx; }
        /** The start time of the current entry */
        nframes_t entry_start() const { return _entry_start; }

        jill::jack_client * const _client; // pointer to jack client for samplerate etc lookup

private:
        void _get_last_entry_index();

        // owned resources
        arf::file_ptr _file;                       // output file
        std::map<std::string,std::string> _attrs;  // attributes for new entries
        arf::packet_table_ptr _log;                // log dataset
        arf::entry_ptr _entry;                     // current entry (owned by thread)
        dset_map_type _dsets;                      // pointers to packet tables (owned)
        int _compression;                          // compression level for new datasets
        std::string _agent_name;           // used in log messages

        // local state
        nframes_t _entry_start;                    // offset sample counts
        nframes_t _period_start;                   // assign chunks to channels
        std::size_t _entry_idx;                    // manage entry numbering
        std::size_t _channel_idx;                  // index channel

};

}}

#endif
