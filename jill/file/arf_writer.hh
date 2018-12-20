#ifndef _ARF_WRITER_HH
#define _ARF_WRITER_HH

#include <map>
#include <string>
#include <iosfwd>
#include <arf/types.hpp>

#include "../data_writer.hh"

namespace jill {

        class data_source;

namespace file {

/**
 * Class for storing data in an ARF file. Access is not thread-safe.
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
        arf_writer(std::string const & filename,
                   jill::data_source const & source,
                   std::map<std::string,std::string> entry_attrs,
                   int compression=0);
        ~arf_writer() = default;

        /* data_writer overrides */
        bool ready() const;
        void new_entry(nframes_t);
        void close_entry();
        void xrun();
        void write(data_block_t const *, nframes_t, nframes_t);
        void log(timestamp_t const &, std::string const &, std::string const &);
        void flush();

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
        /* find last entry index */
        void _get_last_entry_index();

        // references
        jill::data_source const & _data_source;

        // owned resources
        arf::file_ptr _file;                       // output file
        std::map<std::string, std::string> _attrs; // attributes for new entries
        arf::packet_table_ptr _log;                // log dataset
        arf::entry_ptr _entry;                     // current entry (owned by thread)
        dset_map_type _dsets;                      // pointers to packet tables (owned)
        std::map<std::string, std::string> _dset_uuids; // session/channel uuid
        int _compression;                          // compression level for new datasets

        // these variables allow more precise timestamps; they are registered to
        // each other when set_data_source is called
        timestamp_t _base_ptime;
        utime_t   _base_usec;

        // local state
        nframes_t _entry_start;                    // offset sample counts
        nframes_t _last_frame;                     // last frame written to the
                                                   // current entry
        std::size_t _entry_idx;                    // manage entry numbering

};

}}

#endif
