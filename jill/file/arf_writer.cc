

#include <arf.hpp>
#include <boost/format.hpp>
#include <sys/time.h>

#include "arf_writer.hh"
#include "../jack_client.hh"
#include "../midi.hh"
#include "../util/string.hh"

#define JILL_LOGDATASET_NAME "jill_log"

using namespace std;
using namespace jill;
using namespace jill::file;
using jill::util::make_string;

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
                       map<string,string> const & entry_attrs,
                       jack_client *jack_client,
                       jack_port_t *trigger_port,
                       int compression)
        : _compression(compression),
          _entry_start(0), _period_start(0), _entry_idx(0), _channel_idx(0),
          _attrs(entry_attrs),
          _client(jack_client),
          _trigger(trigger_port)
{
        if (_client) _agent_name = _client->name();
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
        _do_log(make_string() << "[" << _agent_name << "] opened file: " << filename);

        _get_last_entry_index();
}

arf_writer::~arf_writer()
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
arf_writer::log(string const & msg)
{
        pthread_mutex_lock (&_lock);
        if (_file && _log) {
                _do_log(msg);
        }
	pthread_mutex_unlock (&_lock);
}

void
arf_writer::_do_log(string const & msg)
{
        struct timeval tp;
        gettimeofday(&tp,0);
        jill::file::message_t message = { tp.tv_sec, tp.tv_usec, msg.c_str() };
        _log->write(&message, 1);
}

void
arf_writer::_get_last_entry_index()
{
        char const * templ = "jrecord_%ud";
        size_t val;
        vector<string> entries = _file->children();
        for (vector<string>::const_iterator it = entries.begin(); it != entries.end(); ++it) {
                int rc = sscanf(it->c_str(), templ, &val);
                val += 1;
                if (rc == 1 && val > _entry_idx) _entry_idx = val;
        }
}

void
arf_writer::new_entry(nframes_t sample_count)
{
        boost::format fmt("jrecord_%|04|");

        fmt % _entry_idx++;

        _dsets.clear();         // release old packet tables
        _entry_start = sample_count;

        if (_entry) {
                _do_log(make_string() << "[" << _agent_name << "] closed entry: " << _entry->name());
        }

        timeval tp;
        gettimeofday(&tp, 0);
        _entry.reset(new arf::entry(*_file, fmt.str(), &tp));
        _do_log(make_string() << "[" << _agent_name << "] created entry: " << _entry->name());

        arf::h5a::node::attr_writer a = _entry->write_attribute();
        a("jack_frame",_entry_start);
        for_each(_attrs.begin(), _attrs.end(), a);
        if (_client) {
                a("jack_usec",_client->time(_entry_start));
        }
}

arf_writer::dset_map_type::iterator
arf_writer::get_dataset(string const & name, bool is_sampled)
{
        dset_map_type::iterator dset = _dsets.find(name);
        if (dset == _dsets.end()) {
                arf::packet_table_ptr pt;
                // std::pair<std::string, arf::packet_table_ptr> val;
                // val.first = name;
                if (is_sampled) {
                        pt = _entry->create_packet_table<sample_t>(name, "", arf::UNDEFINED,
                                                                          false, 1024, _compression);
                }
                else {
                        pt = _entry->create_packet_table<event_t>(name, "samples", arf::EVENT,
                                                                          false, 1024, _compression);
                }
                if (_client) {
                        pt->write_attribute("sampling_rate", _client->sampling_rate());
                }
                _do_log(make_string() << "[" << _agent_name << "] created dataset: " << pt->name());
                dset = _dsets.insert(dset, make_pair(name,pt));
        }
        return dset;
}

void
arf_writer::write(period_info_t const * info)
{
        std::string dset_name;
        bool is_sampled = true;
        // start new entry?
        // was there an xrun?
        if (_xruns) {
                _do_log(make_string() << "[" << _agent_name << "] ERROR: xrun");
                _entry->write_attribute("jill_error","data xrun");
                if (!_trigger) {
                        _entry.reset(); // new entry
                }
                __sync_add_and_fetch(&_xruns, -1);
        }

        // in triggered mode?
        if (!_entry && _trigger) {
                //new_entry(info->time);
        }
        // in continuous mode, will we overflow the sample count?
        else if (!_entry || info->time < _entry_start) {
                new_entry(info->time);
        }

        // get jack port information
        if (info->arg) {
                jack_port_t const * port = static_cast<jack_port_t const *>(info->arg);
                // use name information in port to look up dataset
                dset_name = jack_port_short_name(port);
                is_sampled = strcmp(jack_port_type(port),JACK_DEFAULT_AUDIO_TYPE)==0;
        }
        else {
                // no port information (primarily a test case)
                // use time to infer channel
                boost::format fmt("pcm_%|03|");
                if (info->time > _period_start) {
                        // new period
                        _channel_idx = 0;
                        _period_start = info->time;
                }
                // assume the port type is audio - not really any way to
                // simulate midi data outside jack framework
                dset_name = (fmt % _channel_idx++).str();
        }
        dset_map_type::iterator dset = get_dataset(dset_name, is_sampled);

        if (is_sampled) {
                dset->second->write(info + 1, info->nframes);
        }
        else {
                // based on my inspection of jackd source, JACK api should
                // declare port_buffer argument const, so this cast should be
                // safe
                void * data = const_cast<period_info_t*>(info+1);
                jack_midi_event_t event;
                nframes_t nevents = jack_midi_get_event_count(data);
                for (nframes_t j = 0; j < nevents; ++j) {
                        jack_midi_event_get(&event, data, j);
                        if (event.size == 0) continue;
                        event_t e = { info->time - _entry_start,
                                      event.buffer[0],
                                      "" };
                        // hex encode standard midi events
                        if (event.size > 1) {
                                if (event.buffer[0] < midi::note_off)
                                        e.message = reinterpret_cast<char*>(event.buffer+1);
                                else {
                                        char s[event.size*2];
                                        jill::util::to_hex(event.buffer+1,event.size-1,s);
                                        e.message = s;
                                }
                        }
                        dset->second->write(&e, 1);
                }
        }

}

void
arf_writer::flush()
{
        if (_file) _file->flush();
}


