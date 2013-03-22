
#include "continuous_arf_thread.hh"
#include "../dsp/period_ringbuffer.hh"

using namespace std;
using namespace jill;
using namespace jill::file;

continuous_arf_thread::continuous_arf_thread(string const & filename,
                                             map<string,string> const & entry_attrs,
                                             jack_client * jack_client,
                                             int compression)
        : arf_writer(filename, entry_attrs, jack_client, compression)
{}

continuous_arf_thread::~continuous_arf_thread()
{
        // make sure writer thread is stopped before cleanup of this class's
        // members begins.
        stop();
        join();
}

/*
 * Write a message to the root-level log dataset in the output file. This
 * function attempts to acquire a lock, in order to prevent multiple
 * threads from modifying the ARF file simultaneously.  If this is a problem,
 * could be implemented by copying the message to a queue and letting the
 * writer thread take care of it.
 */
void
continuous_arf_thread::log(string const & msg)
{
        pthread_mutex_lock (&_lock);
        arf_writer::log(msg);
        pthread_mutex_unlock (&_lock);
}

void
continuous_arf_thread::write(period_info_t const * info)
{
        /* handle xruns in first period of the channel */
        if (_xruns && channels() == 0) {
                arf_writer::xrun();
                arf_writer::close_entry(); // new entry
                __sync_add_and_fetch(&_xruns, -1);
        }

        /* create new entry if frame counts will overflow */
        if (info->time < entry_start() || (info->time + info->nframes) < entry_start()) {
                new_entry(info->time);
        }

        arf_writer::write(info);

        /* release data */
        _buffer->release();
}
