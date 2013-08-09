#include <boost/type_traits/make_signed.hpp>

#include "triggered_data_writer.hh"
#include "../types.hh"
#include "../logger.hh"
#include "../midi.hh"
#include "../dsp/block_ringbuffer.hh"

using namespace std;
using namespace jill;
using namespace jill::dsp;

/** A data type for comparing differences between frame counts */
typedef boost::make_signed<nframes_t>::type framediff_t;

namespace jill {

std::ostream &
operator<<(std::ostream & os, data_block_t const & b)
{
        os << "time=" << b.time << ", id=" << b.id() << ", type=" << b.dtype
           << ", frames=" << b.nframes();
        return os;
}

}

triggered_data_writer::triggered_data_writer(boost::shared_ptr<data_writer> writer,
                                             string const & trigger_port,
                                             nframes_t pretrigger_frames, nframes_t posttrigger_frames)
        : buffered_data_writer(writer),
          _trigger_port(trigger_port),
          _pretrigger(pretrigger_frames),
          _posttrigger(std::max(posttrigger_frames, 1U)),
          _recording(false)
{
        DBG << "triggered_data_writer initializing";
}

triggered_data_writer::~triggered_data_writer()
{
        DBG << "triggered_data_writer closing";
        stop();
        join();
}

void
triggered_data_writer::start_recording(nframes_t event_time)
{
        nframes_t onset = event_time - _pretrigger;
        _writer->new_entry(onset);

        INFO << "writing pretrigger data from " << onset << "--" << event_time;
        /* start at tail of queue and find onset */
        data_block_t const * ptr = _buffer->peek();
        assert(ptr);

        /* skip any earlier periods */
        while (ptr->time + ptr->nframes() < onset) {
                _buffer->release();
                ptr = _buffer->peek();
        }

        /* write partial period(s) */
        while (ptr->time <= onset) {
                DBG << "prebuf frame: t=" << ptr->time << ", on=" << onset - ptr->time
                    << ", id=" << ptr->id() << ", dtype=" << ptr->dtype;
                _writer->write(ptr, onset - ptr->time, 0);
                _buffer->release();
                ptr = _buffer->peek();
        }

        /* write additional periods in prebuffer, up to current period */
        while (ptr->time + ptr->nframes() <= event_time) {
                _writer->write(ptr, 0, 0);
                _buffer->release();
                ptr = _buffer->peek();
        }

        _recording = true;
}

void
triggered_data_writer::stop_recording(nframes_t event_time)
{
        _recording = false;
        _last_offset = event_time + _posttrigger;
        INFO << "writing posttrigger data from " << event_time << "--" << _last_offset;
}

void
triggered_data_writer::write(data_block_t const * data)
{
        std::string id = data->id();
        nframes_t nframes = data->nframes();
        /* handle trigger channel */
        if (data->dtype == EVENT && id == _trigger_port) {
                if (_recording) {
                        if (midi::is_offset(data->data(), data->sz_data)) {
                                DBG << "trigger off event: time=" << data->time;
                                stop_recording(data->time);
                        }
                }
                else {
                        if (midi::is_onset(data->data(), data->sz_data)) {
                                DBG << "trigger on event: time=" << data->time;
                                start_recording(data->time);
                        }
                }
        }

        if (_recording) {
                // write full block
                // check if data is flushed. can't compare pointers directly
                // because the same data may have multiple addresses in the buffer
                data_block_t const * tail = _buffer->peek();
                assert(tail->time == data->time && tail->id() == id);
                _writer->write(data, 0, 0);
                _buffer->release();
                if (__sync_bool_compare_and_swap(&_reset, true, false)) {
                        stop_recording(data->time + nframes);
                }
        }
        else if (_writer->ready()) {
                // write post-trigger periods
                framediff_t compare = _last_offset - data->time;
                if (compare < 0) {
                        _writer->close_entry();
                }
                else {
                        _writer->write(data, 0, (nframes_t)compare);
                }
                _buffer->release();
        }
        else {
                // not writing: drop blocks on tail of queue as needed
                data_block_t const * tail = _buffer->peek();
                while (tail && (data->time + nframes) - (tail->time + nframes) > _pretrigger) {
                        _buffer->release();
                        tail = _buffer->peek();
                }
        }
}
