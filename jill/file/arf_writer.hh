#ifndef _ARF_WRITER_HH
#define _ARF_WRITER_HH

#include <map>
#include <optional>
#include <string>
#include <iosfwd>
#include <arf.hpp>

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
        ~arf_writer() override = default;

        /* Owns the HDF5 file and the packet tables written into it, which are
         * themselves move-only handles. */
        arf_writer(arf_writer const &) = delete;
        arf_writer & operator=(arf_writer const &) = delete;

        /* data_writer overrides */
        bool ready() const override;
        void new_entry(nframes_t) override;
        void close_entry() override;
        void xrun() override;
        void write(data_block_t const *, nframes_t, nframes_t) override;
        void log(timestamp_t, std::string, std::string) override;
        void flush() override;

protected:
        /* arf 3 packet tables are move-only handles rather than shared_ptrs,
         * so this map has a move-only mapped type: use emplace, not
         * operator[]. Erasing an element closes the table. */
        typedef std::map<std::string, arf::h5pt::packet_table> dset_map_type;

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

        // owned resources. arf 3 handles own their identifier and are
        // move-only, so these are values rather than pointers; the file and the
        // log are built in the initializer list because neither is optional.
        arf::file _file;                           // output file
        std::map<std::string, std::string> _attrs; // attributes for new entries
        arf::h5pt::packet_table _log;              // log dataset
        // empty between entries, which is what ready() reports on
        std::optional<arf::entry> _entry;          // current entry (owned by thread)
        dset_map_type _dsets;                      // packet tables (owned)
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
