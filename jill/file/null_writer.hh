#ifndef _NULL_WRITER_HH
#define _NULL_WRITER_HH

#include "../data_writer.hh"

namespace jill { namespace file {

/* a no-op implementation of data_writer */
class null_writer : public data_writer {

public:
        void new_entry(nframes_t) {}
        void close_entry() {}
        bool ready() const { return true; }
        bool aligned() const { return true; }
        void xrun() {}
        nframes_t write(period_info_t const * info, nframes_t start=0, nframes_t stop=0) {
                std::cout << "\rgot period: time=" << info->time << ", nframes=" << info->nframes << std::flush;
                return info->nframes;
        }
        void write_log(timestamp const &, std::string const & msg) {
                std::cout << msg << std::endl;
        }
};

}}

#endif
