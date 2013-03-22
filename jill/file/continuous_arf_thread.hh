#ifndef _CONTINUOUS_ARF_THREAD_HH
#define _CONTINUOUS_ARF_THREAD_HH

#include "../dsp/multichannel_data_thread.hh"
#include "arf_writer.hh"

namespace jill { namespace file {

/**
 * This implementation of the disk_thread class stores data in an ARF file.
 */
class continuous_arf_thread : public dsp::multichannel_data_thread, protected arf_writer {
public:
        /**
         * Initialize an ARF writer thread.
         *
         * @param filename            the file to write to
         * @param entry_attrs  map of attributes to set on newly-created entries
         * @param jack_client  optional, client used to look up time and samplerate
         * @param compression  the compression level for new datasets
         */
        continuous_arf_thread(std::string const & filename,
                              std::map<std::string,std::string> const & entry_attrs,
                              jill::jack_client * jack_client=0,
                              int compression=0);
        ~continuous_arf_thread();

        // @see data_thread::log()
        void log(std::string const & msg);

protected:

        // @see multichannel_data_thread::write()
        void write(period_info_t const * info);

private:
};

}}

#endif
