#include <boost/type_traits/make_signed.hpp>

#include "triggered_data_writer.hh"
#include "../types.hh"
#include "../dsp/period_ringbuffer.hh"
#include "../midi.hh"

using namespace std;
using namespace jill;
using namespace jill::dsp;

/** A data type for comparing differences between frame counts */
typedef boost::make_signed<nframes_t>::type framediff_t;


triggered_data_writer::triggered_data_writer(boost::shared_ptr<data_writer> writer,
                                             jack_port_t const * trigger_port,
                                             nframes_t pretrigger_frames, nframes_t posttrigger_frames)
        : buffered_data_writer(writer),
          _trigger_port(trigger_port),
          _pretrigger(pretrigger_frames),
          _posttrigger(std::max(posttrigger_frames,1U)),
          _recording(false)
{}

triggered_data_writer::~triggered_data_writer()
{
        stop();
        join();
}

nframes_t
triggered_data_writer::resize_buffer(nframes_t nframes, nframes_t nchannels)
{
        return buffered_data_writer::resize_buffer(nframes + _pretrigger, nchannels);
}

void
triggered_data_writer::start_recording(nframes_t event_time)
{
        nframes_t onset = event_time - _pretrigger;
        _writer->new_entry(onset);

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
                _writer->write(ptr, onset - ptr->time);
                _buffer->release();
                ptr = _buffer->peek();
        }

        /* write additional periods in prebuffer, up to current period */
        while (ptr->time + ptr->nframes <= event_time) {
                _writer->write(ptr);
                _buffer->release();
                ptr = _buffer->peek();
        }

        _recording = true;
}

void
triggered_data_writer::close_entry(nframes_t event_time)
{
        // this will be a noop if another thread has closed or is closing the entry
        if (__sync_bool_compare_and_swap(&_recording,true,false)) {
                __sync_bool_compare_and_swap(&_last_offset,_last_offset,event_time + _posttrigger);
        }
}

void
triggered_data_writer::write(period_info_t const * period)
{
        jack_port_t const * port;

        /* handle xruns whenever detected. some additional samples may be dropped */
        if (_xrun) {
                __sync_bool_compare_and_swap(&_xrun, true, false);
                _writer->xrun(); // marks entry and logs xrun
        }

        if (period == 0) return;
        port = static_cast<jack_port_t const *>(period->arg);

        /* handle trigger channel */
        if (port && port == _trigger_port) {
                void const * data = static_cast<void const *>(period+1);
                // scan for onset & offset events
                if (!_writer->ready()) {
                        int event_time = midi::find_trigger(data, true);
                        if (event_time > -1) {
                                start_recording(period->time + event_time);
                        }
                }
                else {
                        int event_time = midi::find_trigger(data, false);
                        if (event_time > -1)
                                close_entry(period->time + event_time);
                }
        }

        if (_recording) {
                // is all old data flushed (this will be optimized out)
                period_info_t const * tail = _buffer->peek();
                assert(tail->time == period->time && tail->arg == period->arg);
                // write complete period
                _writer->write(period);
                _buffer->release();
        }
        else if (_writer->ready()) {
                // write post-trigger periods
                framediff_t compare = _last_offset - period->time;
                if (compare < 0) {
                        _writer->close_entry();
                }
                else {
                        nframes_t to_write = std::min((nframes_t)compare, period->nframes);
                        _writer->write(period, 0, to_write);
                }
                _buffer->release();
        }
        else {
                period_info_t const * tail = _buffer->peek();
                // deal with tail of queue
                if (tail && (period->time + period->nframes) - (tail->time + period->nframes) > _pretrigger)
                        _buffer->release();
        }
}
