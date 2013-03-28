#ifndef _ARF_WRITER_HH
#define _ARF_WRITER_HH

#include <map>
#include <string>
#include <iosfwd>
#include <pthread.h>
#include <boost/iostreams/stream.hpp>
#include <boost/weak_ptr.hpp>
#include <arf/types.hpp>

#include "../data_writer.hh"

namespace jill {

        class data_source;

namespace file {

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
 * Class for storing data in an ARF file.  Access is thread-safe, and calls to
 * non-const member functions may block.
 */
class arf_writer : public data_writer {
public:
        /**
         * Initialize an ARF writer.
         *
         * @param sourcename   identifier of the program/process writing the data
         * @param filename     the file to write to
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param data_source  the source of the data. may be null
         * @param compression  the compression level for new datasets
         */
        arf_writer(std::string const & sourcename,
                   std::string const & filename,
                   std::map<std::string,std::string> const & entry_attrs,
                   boost::weak_ptr<data_source> data_source,
                   int compression=0);
        ~arf_writer();

        /* data_writer overrides */
        void new_entry(nframes_t);
        void close_entry();
        bool ready() const;
        bool aligned() const;
        void xrun();
        nframes_t write(period_info_t const *, nframes_t start=0, nframes_t stop=0);

        /* event_logger overrides */
        std::ostream & log();
        void redirect(event_logger &);

protected:
        typedef std::map<std::string, arf::packet_table_ptr> dset_map_type;

        /**
         * Look up dataset in current entry, creating as needed.
         *
         * @param name         the name of the dataset (channel)
         * @param is_sampled   whether the dataset holds samples or events
         * @return derefable iterator for appropriate dataset
         */
        dset_map_type::iterator get_dataset(std::string const & name, bool is_sampled);

private:
        /* implement event_logger::log */
        std::streamsize log(char const *, std::streamsize);
        /* find last entry index */
        void _get_last_entry_index();

        // references
        boost::weak_ptr<jill::data_source> _data_source;

        // owned resources
        pthread_mutex_t _lock;                     // mutex for disk operations
        std::string _sourcename;                   // who's doing the writing
        arf::file_ptr _file;                       // output file
        std::map<std::string,std::string> _attrs;  // attributes for new entries
        arf::packet_table_ptr _log;                // log dataset
        arf::entry_ptr _entry;                     // current entry (owned by thread)
        dset_map_type _dsets;                      // pointers to packet tables (owned)
        int _compression;                          // compression level for new datasets
        boost::iostreams::stream<logger_proxy> _logstream; // provides stream logging

        // local state
        nframes_t _entry_start;                    // offset sample counts
        nframes_t _period_start;                   // assign chunks to channels
        std::size_t _entry_idx;                    // manage entry numbering
        std::size_t _channel_idx;                  // index channel

};

}}

#endif
