
#include <boost/type_traits/make_signed.hpp>

#include "triggered_arf_thread.hh"
#include "../types.hh"
#include "../dsp/period_ringbuffer.hh"
#include "../midi.hh"

using namespace std;
using namespace jill;
using namespace jill::file;
//using jill::util::make_string;

/** A data type for comparing differences between frame counts */
typedef boost::make_signed<nframes_t>::type framediff_t;


triggered_arf_thread::triggered_arf_thread(string const & filename,
                                           jack_port_t const * trigger_port,
                                           nframes_t pretrigger_frames, nframes_t posttrigger_frames,
                                           map<string,string> const & entry_attrs,
                                           jack_client * jack_client,
                                           int compression)
        : multichannel_data_thread(pretrigger_frames),
          arf_writer(filename, entry_attrs, jack_client, compression),
          _trigger_port(trigger_port),
          _pretrigger(pretrigger_frames),
          _posttrigger(posttrigger_frames),
          _recording(false)
{}

triggered_arf_thread::~triggered_arf_thread()
{
        stop();
        join();
}

void
triggered_arf_thread::log(string const & msg)
{
        pthread_mutex_lock (&_lock);
        arf_writer::log(msg);
        pthread_mutex_unlock (&_lock);
}

nframes_t
triggered_arf_thread::resize_buffer(nframes_t nframes, nframes_t nchannels)
{
        return multichannel_data_thread::resize_buffer(nframes + _pretrigger, nchannels);
}

void
triggered_arf_thread::start_recording(nframes_t event_time)
{
        nframes_t onset = event_time - _pretrigger;
        new_entry(onset);

        /* start at tail of queue and find onset */
        period_info_t const * ptr = _buffer->peek();
        assert(ptr);

        /* skip any earlier periods */
        while (ptr->time + ptr->nframes < onset) {
                _buffer->release();
                ptr = _buffer->peek();
        }

        /* write partial period(s) */
        while (ptr->time <= onset) {
                arf_writer::write(ptr, onset - ptr->time);
                _buffer->release();
                ptr = _buffer->peek();
        }

        /* write additional periods in prebuffer, up to current period */
        while (ptr->time + ptr->nframes < event_time) {
                arf_writer::write(ptr);
                _buffer->release();
                ptr = _buffer->peek();
        }

        _recording = true;
}

void
triggered_arf_thread::stop_recording(nframes_t event_time)
{
        _recording = false;
        _last_offset = event_time + _posttrigger;
}

void
triggered_arf_thread::write(period_info_t const * info)
{
        jack_port_t const * port = static_cast<jack_port_t const *>(info->arg);
        void * data = const_cast<period_info_t*>(info+1);

        /* handle xruns whenever they're detected */
        if (_xruns) {
                arf_writer::xrun(); // marks entry and logs xrun
                __sync_add_and_fetch(&_xruns, -1);
        }

        /* handle trigger channel */
        if (port && port == _trigger_port) {
                // scan for onset & offset events
                int event_time = midi::find_trigger(data, !_recording);
                if (event_time > 0) {
                        if (!_recording)
                                start_recording(info->time + event_time);
                        else {
                                stop_recording(info->time + event_time);
                        }
                        assert(info == _buffer->peek()); // is all old data flushed?
                }
        }
        period_info_t const * tail = _buffer->peek();

        if (_recording) {
                // this should only apply in a test case, but it's cheap to be safe
                while (info != tail) {
                        arf_writer::write(tail);
                        _buffer->release();
                        tail = _buffer->peek();
                }
                arf_writer::write(info);
                _buffer->release();
        }
        else if (current_entry()) {
                // write post-trigger periods
                framediff_t compare = _last_offset - info->time;
                if (compare < 0) {
                        arf_writer::close_entry();
                }
                else {
                        nframes_t to_write = std::min((nframes_t)compare, info->nframes);
                        arf_writer::write(info, 0, to_write);
                }
                _buffer->release();
        }
        else {
                // deal with tail of queue
                if (tail && (info->time + info->nframes) - (tail->time + info->nframes) > _pretrigger)
                        _buffer->release();
        }
}
