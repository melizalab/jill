#ifndef _NULL_WRITER_HH
#define _NULL_WRITER_HH

#include "../data_writer.hh"

namespace jill { namespace file {

/* a no-op implementation of data_writer */
class null_writer : public data_writer {

public:
        null_writer() : _entry(0) {}
        void new_entry(nframes_t frame) {
                std::cout << "\nnew entry " << _entry << ", frame=" << frame << std::endl;
        }
        void close_entry() {
                std::cout << "\nclosed entry " <<  _entry << std::endl;
                _entry += 1;
        }
        void xrun() {
                std::cout << "\ngot xrun" << std::endl;
        }
        bool ready() const { return true; }
        bool aligned() const { return true; }
        void write(data_block_t const * data, nframes_t start, nframes_t stop) {
                std::cout << "\rgot period: time=" << data->time << ", id=" << data->id()
                          << ", type=" << data->dtype << ", bytes=" << data->sz_data << std::flush;
        }
private:
        int _entry;
};

}}

#endif
