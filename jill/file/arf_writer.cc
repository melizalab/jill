#include <arf.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#define BOOST_UUID_NO_TYPE_TRAITS
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "arf_writer.hh"
#include "../version.hh"
#include "../logging.hh"
#include "../data_source.hh"
#include "../midi.hh"

#define JILL_LOGDATASET_NAME "jill_log"
#define ARF_CHUNK_SIZE 1024

using namespace std;
using namespace jill;
using namespace jill::file;
using namespace boost::posix_time;
using namespace boost::gregorian;

static const ptime epoch = ptime(date(1970,1,1));

/**
 * @brief Storage format for log messages
 */
struct message_t {
        boost::int64_t sec;
        boost::int64_t usec;
        char const * message;       // descriptive
};

/**
 * @brief Storage format for event data
 */
struct event_t {
        boost::uint32_t start;  // relative to entry start
        boost::uint8_t status;  // see jill::event_t::midi_type
        char const * message;   // message (hex encoded for standard midi status)
};

/**
 * convert a midi message to hex
 * @param in   the midi message
 * @param size the length of the message
 */
template <typename T>
char *
to_hex(T const * in, std::size_t size)
{
        char * out = new char[size * 2 + 2];
        sprintf(out, "0x");
        for (std::size_t i = 0; i < size; ++i) {
                sprintf(out + i*2 + 2, "%02x", in[i]);
        }
        return out;
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

arf_writer::arf_writer(string const & filename,
                       data_source const & source,
                       map<string,string> const & entry_attrs,
                       int compression)
        : _data_source(source),
          _attrs(entry_attrs),
          _compression(compression),
          _entry_start(0), _entry_idx(0)
{
        _base_usec = _data_source.time();
        _base_ptime = microsec_clock::universal_time();
        LOG << "registered system clock to usec clock at " << _base_usec;

        _file.reset(new arf::file(filename, "a"));
        LOG << "opened file: " << filename;
        if (!_file->has_attribute("file_creator")) {
                _file->write_attribute("file_creator", "org.meliza.jill/jrecord " JILL_VERSION);
        }

        // open/create log
        arf::h5t::wrapper<message_t> t;
        arf::h5t::datatype logtype(t);
        if (_file->contains(JILL_LOGDATASET_NAME)) {
                _log.reset(new arf::h5pt::packet_table(_file->hid(), JILL_LOGDATASET_NAME));
                if (logtype != *(_log->datatype())) {
                        throw arf::Exception(JILL_LOGDATASET_NAME " has wrong datatype");
                }
                INFO << "appending log messages to /" << JILL_LOGDATASET_NAME;
        }
        else {
                _log.reset(new arf::h5pt::packet_table(_file->hid(), JILL_LOGDATASET_NAME,
                                                       logtype, ARF_CHUNK_SIZE, _compression));
                INFO << "created log dataset /" << JILL_LOGDATASET_NAME;
        }
        _get_last_entry_index();
}

arf_writer::~arf_writer()
{}

void
arf_writer::new_entry(nframes_t frame_count)
{
        utime_t frame_usec = 0;

        std::ostringstream name;
        name << _data_source.name() << '_' << setw(4) << setfill('0') << _entry_idx++;

        close_entry();
        _entry_start = frame_count;

        time_duration ts;
        frame_usec = _data_source.time(_entry_start);
        ts = (_base_ptime + microseconds(frame_usec - _base_usec)) - epoch;

        _entry.reset(new arf::entry(*_file, name.str(),
                                    ts.total_seconds(), ts.fractional_seconds()));

        LOG << "created entry: " << _entry->name() << " (frame=" << _entry_start << ")" ;

        arf::h5a::node::attr_writer a = _entry->write_attribute();
        a("jack_frame", _entry_start);
        a("jack_usec", frame_usec);
        a("jack_sampling_rate", _data_source.sampling_rate());
        a("entry_creator", "org.meliza.jill/jrecord " JILL_VERSION);
        for_each(_attrs.begin(), _attrs.end(), a);
}

void
arf_writer::close_entry()
{
        _dsets.clear();         // release any old packet tables
        if (_entry) {
                LOG << "closed entry: " << _entry->name() << " (frame=" << _last_frame << ")";
                _entry->write_attribute("trial_off", _last_frame - _entry_start);
                // if (!aligned())
                //         o << " (warning: unequal dataset length)";
        }
        _entry.reset();
}

bool
arf_writer::ready() const
{
        return _entry;
}

void
arf_writer::xrun()
{
        LOG << "ERROR: xrun" ;
        if (_entry) {
                // tag entry as possibly corrupt
                _entry->write_attribute("jill_error","data xrun");
        }
}

void
arf_writer::write(data_block_t const * data, nframes_t start_frame, nframes_t stop_frame)
{
        if (data->sz_data == 0) return;
        std::string id = data->id();
        nframes_t nframes = data->nframes();
        dset_map_type::iterator dset;
        stop_frame = (stop_frame > 0) ? std::min(stop_frame, nframes) : nframes;

        // check for overflow of sample counter
        if (_entry && (data->time + start_frame) < _entry_start) {
                LOG << "sample count overflow (entry=" << _entry_start
                    << ", data=" << (data->time + start_frame) << ")";
                close_entry();
        }
        if (!_entry) {
                new_entry(data->time);
        }
        /* write the data */
        if (data->dtype == SAMPLED) {
                dset = get_dataset(id, true);
                sample_t const * samples = reinterpret_cast<sample_t const *>(data->data());
                dset->second->write(samples + start_frame, stop_frame - start_frame);
        }
        else if (data->dtype == EVENT) {
                char * message = 0;
                dset = get_dataset(id, false);
                char const * buffer = reinterpret_cast<char const *>(data->data());
                event_t e = {data->time - _entry_start, (uint8_t)buffer[0], buffer+1};
                if (e.status >= midi::note_off) {
                        // hex-encode standard midi events
                        e.message = message = to_hex(buffer + 1, data->sz_data - 1);
                }
                DBG << "event: t=" << data->time << " id=" << id << " status=" << int(e.status)
                    << " message=" << e.message;
                dset->second->write(&e, 1);
                if (message) delete[] message;
        }
        _last_frame = data->time + stop_frame;
}

void
arf_writer::flush()
{
        _file->flush();
}

void
arf_writer::log(timestamp_t const &utc, string const & source, string const & msg)
{
        char m[msg.length() + source.length() + 4];
        sprintf(m, "[%s] %s", source.c_str(), msg.c_str());

        time_duration t = utc - epoch;
        message_t message = { t.total_seconds(), t.fractional_seconds(), m };
        _log->write(&message, 1);
}

void
arf_writer::_get_last_entry_index()
{
        unsigned int val;
        vector<string> entries = _file->children();  // read-only
        for (vector<string>::const_iterator it = entries.begin(); it != entries.end(); ++it) {
                char const * match = strstr(it->c_str(), _data_source.name());
                if (match == 0) continue;
                int rc = sscanf(match + strlen(_data_source.name()), "_%ud", &val);
                val += 1;
                if (rc == 1 && val > _entry_idx) _entry_idx = val;
        }
        INFO << "last entry index: " << _entry_idx;
}


arf_writer::dset_map_type::iterator
arf_writer::get_dataset(string const & name, bool is_sampled)
{
        map<string, string>::iterator uuid = _dset_uuids.find(name);
        if (uuid == _dset_uuids.end()) {
                // generate new uuid for dataset name if it doesn't exist
                string _uuid = boost::uuids::to_string(boost::uuids::random_generator()());
                uuid = _dset_uuids.insert(uuid, make_pair(name, _uuid));
                INFO << "uuid for " << name << ": " << uuid->second;
        }

        dset_map_type::iterator dset = _dsets.find(name);
        if (dset == _dsets.end()) {
                arf::packet_table_ptr pt;
                if (is_sampled) {
                        pt = _entry->create_packet_table<sample_t>(name, "", arf::UNDEFINED,
                                                                   false, ARF_CHUNK_SIZE,
                                                                   _compression);
                }
                else {
                        pt = _entry->create_packet_table<event_t>(name, "samples", arf::EVENT,
                                                                  false, ARF_CHUNK_SIZE,
                                                                  _compression);
                }
                pt->write_attribute("sampling_rate", _data_source.sampling_rate());
                pt->write_attribute("uuid", uuid->second);
                LOG << "created dataset: " << pt->name();
                dset = _dsets.insert(dset, make_pair(name,pt));
        }

        return dset;

}

