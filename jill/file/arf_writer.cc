#include <arf.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/format.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <sys/time.h>
#include <jack/jack.h>

#include "arf_writer.hh"
#include "../data_source.hh"
#include "../midi.hh"

#define JILL_LOGDATASET_NAME "jill_log"

using namespace std;
using namespace jill;
using namespace jill::file;

typedef boost::shared_ptr<jill::data_source> source_ptr;

/**
 * convert a midi message to hex
 * @param in   the midi message
 * @param size the length of the message
 * @param out  the output buffer
 */
template <typename T>
static std::string
to_hex(T const * in, std::size_t size)
{
        char out[size*2];
        for (std::size_t i = 0; i < size; ++i) {
                sprintf(out + i*2, "%02x", in[i]);
        }
        return out;
}

static boost::posix_time::time_duration
timeofday(boost::posix_time::ptime const & time)
{
        using namespace boost::gregorian;
        using namespace boost::posix_time;
        return time - ptime(date(1970,1,1));
}

// template specializations for compound data types
namespace arf { namespace h5t { namespace detail {

template<>
struct datatype_traits<message_t> {
	static hid_t value() {
                hid_t str = H5Tcopy(H5T_C_S1);
                H5Tset_size(str, H5T_VARIABLE);
                H5Tset_cset(str, H5T_CSET_UTF8);
                hid_t ret = H5Tcreate(H5T_COMPOUND, sizeof(message_t));
                H5Tinsert(ret, "sec", HOFFSET(message_t, sec), H5T_NATIVE_INT64);
                H5Tinsert(ret, "usec", HOFFSET(message_t, usec), H5T_NATIVE_INT64);
                H5Tinsert(ret, "message", HOFFSET(message_t, message), str);
                H5Tclose(str);
                return ret;
        }
};

template<>
struct datatype_traits<event_t> {
	static hid_t value() {
                hid_t str = H5Tcopy(H5T_C_S1);
                H5Tset_size(str, H5T_VARIABLE);
                H5Tset_cset(str, H5T_CSET_UTF8);
                hid_t ret = H5Tcreate(H5T_COMPOUND, sizeof(event_t));
                H5Tinsert(ret, "start", HOFFSET(event_t, start), H5T_NATIVE_UINT32);
                H5Tinsert(ret, "status", HOFFSET(event_t, status), H5T_NATIVE_UINT8);
                H5Tinsert(ret, "message", HOFFSET(event_t, message), str);
                H5Tclose(str);
                return ret;
        }
};

}}}

arf_writer::arf_writer(string const & sourcename,
                       string const & filename,
                       map<string,string> const & entry_attrs,
                       boost::weak_ptr<data_source> data_source,
                       int compression)
        : _data_source(data_source),
          _sourcename(sourcename),
          _attrs(entry_attrs),
          _compression(compression),
          _logstream(*this),
          _entry_start(0), _period_start(0), _entry_idx(0), _channel_idx(0)
{
        pthread_mutex_init(&_lock, 0);
        _file.reset(new arf::file(filename, "a"));

        // open/create log
        arf::h5t::wrapper<jill::file::message_t> t;
        arf::h5t::datatype logtype(t);
        if (_file->contains(JILL_LOGDATASET_NAME)) {
                _log.reset(new arf::h5pt::packet_table(_file->hid(), JILL_LOGDATASET_NAME));
                if (logtype != *(_log->datatype())) {
                        throw arf::Exception(JILL_LOGDATASET_NAME " has wrong datatype");
                }
        }
        else {
                _log.reset(new arf::h5pt::packet_table(_file->hid(), JILL_LOGDATASET_NAME,
                                                       logtype, 1024, _compression));
        }
        _get_last_entry_index();

        log() << "opened file: " << filename  << std::endl;
}

arf_writer::~arf_writer()
{
        pthread_mutex_lock(&_lock);
        _file->flush();
        pthread_mutex_unlock(&_lock);
        pthread_mutex_destroy(&_lock);
}

void
arf_writer::new_entry(nframes_t frame_count)
{
        using namespace boost::posix_time;
        utime_t frame_usec = 0;

        boost::format fmt("%1$s_%2$04d");
        fmt % _sourcename % _entry_idx++;

        close_entry();
        _entry_start = frame_count;

        ptime now(microsec_clock::universal_time());
        if (source_ptr source = _data_source.lock()) {
                // adjust time by difference between now and the frame_count
                frame_usec = source->time(_entry_start);
                now -= microseconds(frame_usec - source->time());
        }
        time_duration timestamp = timeofday(now);
        timeval tp = { timestamp.total_seconds(), timestamp.fractional_seconds() };

        pthread_mutex_lock(&_lock);
        _entry.reset(new arf::entry(*_file, fmt.str(), &tp));
        pthread_mutex_unlock(&_lock);

        log() << "created entry: " << _entry->name() << " (frame=" << _entry_start << ")" << std::endl;

        pthread_mutex_lock(&_lock);
        arf::h5a::node::attr_writer a = _entry->write_attribute();
        a("jack_frame",_entry_start)("jill_process",_sourcename);
        for_each(_attrs.begin(), _attrs.end(), a);
        if (frame_usec != 0) {
                a("jack_usec", frame_usec);
        }
        pthread_mutex_unlock(&_lock);
}

void
arf_writer::close_entry()
{
        _dsets.clear();         // release any old packet tables
        _channel_idx = 0;
        if (_entry) {
                std::ostream & o = log() << "closed entry: " << _entry->name();
                if (!aligned())
                        o << " (warning: datasets have unequal length)";
                o << std::endl;
        }
        _entry.reset();
}

bool
arf_writer::ready() const
{
        return _entry;
}

bool
arf_writer::aligned() const
{
        return (_period_start != _entry_start) && (_channel_idx == _dsets.size());
}

void
arf_writer::xrun()
{
        log() << "ERROR: xrun" << std::endl;
        if (_entry) {
                // tag entry as possibly corrupt
                pthread_mutex_lock(&_lock);
                _entry->write_attribute("jill_error","data xrun");
                pthread_mutex_unlock(&_lock);
        }
}

nframes_t
arf_writer::write(period_info_t const * info, nframes_t start_frame, nframes_t stop_frame)
{
        std::string dset_name;
        bool is_sampled = true;
        jack_port_t const * port = static_cast<jack_port_t const *>(info->arg);

        if (!_entry) new_entry(info->time);
        stop_frame = (stop_frame > 0) ? std::min(stop_frame, info->nframes) : info->nframes;

        /* does this start a new period */
        if (info->time != _period_start) {
                // new period
                _channel_idx = 0;
                _period_start = info->time;
        }

        /* get channel information */
        if (port) {
                // use name information in port to look up dataset
                dset_name = jack_port_short_name(port);
                is_sampled = strcmp(jack_port_type(port),JACK_DEFAULT_AUDIO_TYPE)==0;
        }
        else {
                // no port information (primarily a test case)
                // use time to infer channel
                boost::format fmt("pcm_%|03|");
                // assume the port type is audio - not really any way to
                // simulate midi data outside jack framework
                dset_name = (fmt % _channel_idx).str();
        }
        _channel_idx += 1;
        dset_map_type::iterator dset = get_dataset(dset_name, is_sampled);

        /* write the data */
        if (is_sampled) {
                assert (stop_frame <= info->nframes);
                sample_t const * data = reinterpret_cast<sample_t const *>(info + 1);
                pthread_mutex_lock(&_lock);
                dset->second->write(data + start_frame, stop_frame - start_frame);
                pthread_mutex_unlock(&_lock);
        }
        else {
                // based on my inspection of jackd source, JACK api should
                // declare port_buffer argument const, so this cast should be
                // safe
                void * data = const_cast<period_info_t*>(info+1);
                jack_midi_event_t event;
                nframes_t nevents = jack_midi_get_event_count(data);
                string s;
                for (nframes_t j = 0; j < nevents; ++j) {
                        jack_midi_event_get(&event, data, j);
                        if (event.size == 0) continue;
                        if (event.time < start_frame || event.time >= stop_frame) continue;
                        event_t e = { info->time - _entry_start,
                                      event.buffer[0],
                                      "" };
                        // hex encode standard midi events
                        if (event.size > 1) {
                                if (e.status < midi::note_off)
                                        e.message = reinterpret_cast<char*>(event.buffer+1);
                                else {
                                        s = to_hex(event.buffer+1,event.size-1);
                                        e.message = s.c_str();
                                }
                        }
                        pthread_mutex_lock(&_lock);
                        dset->second->write(&e, 1);
                        pthread_mutex_unlock(&_lock);
                }
        }
        return stop_frame - start_frame;

}

ostream &
arf_writer::log()
{
        return _logstream << "[" << _sourcename << "] ";
}

void
arf_writer::redirect(event_logger &w)
{
        _logstream.close();
        _logstream.open(w);
}


streamsize
arf_writer::log(char const * msg, streamsize n)
{
        using namespace boost::posix_time;
        time_duration t = timeofday(microsec_clock::universal_time());

        // strip final \n
        boost::scoped_array<char> m(new char[n]);
        memcpy(m.get(),msg,n);
        m[n-1] = 0x0;
        jill::file::message_t message = { t.total_seconds(), t.fractional_seconds(), m.get() };
        pthread_mutex_lock(&_lock);
        _log->write(&message, 1);
        pthread_mutex_unlock(&_lock);
        std::cout << to_iso_string(microsec_clock::local_time()) << ' ' << m.get() << std::endl;
        return n;
}

void
arf_writer::_get_last_entry_index()
{
        size_t val;
        vector<string> entries = _file->children();  // read-only
        for (vector<string>::const_iterator it = entries.begin(); it != entries.end(); ++it) {
                char const * match = strstr(it->c_str(), _sourcename.c_str());
                if (match == 0) continue;
                int rc = sscanf(match + _sourcename.length(), "_%ud", &val);
                val += 1;
                if (rc == 1 && val > _entry_idx) _entry_idx = val;
        }
}


arf_writer::dset_map_type::iterator
arf_writer::get_dataset(string const & name, bool is_sampled)
{
        dset_map_type::iterator dset = _dsets.find(name);
        if (dset == _dsets.end()) {
                arf::packet_table_ptr pt;
                pthread_mutex_lock(&_lock);
                if (is_sampled) {
                        pt = _entry->create_packet_table<sample_t>(name, "", arf::UNDEFINED,
                                                                          false, 1024, _compression);
                }
                else {
                        pt = _entry->create_packet_table<event_t>(name, "samples", arf::EVENT,
                                                                          false, 1024, _compression);
                }
                if (source_ptr source = _data_source.lock()) {
                        pt->write_attribute("sampling_rate", source->sampling_rate());
                }
                pthread_mutex_unlock(&_lock);
                log() << "created dataset: " << pt->name() << std::endl;
                dset = _dsets.insert(dset, make_pair(name,pt));
        }
        return dset;
}

