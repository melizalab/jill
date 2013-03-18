

#include <arf.hpp>
#include <boost/algorithm/hex.hpp>
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
          _attrs(entry_attrs),
          _client(jack_client),
          _trigger(trigger_port)
{
        if (_client) _agent_name = _client->name();

        _file.reset(new arf::file(filename, "a"));

        // open/create log
        if (_file->contains(JILL_LOGDATASET_NAME)) {
                _log.reset(new arf::h5pt::packet_table(_file->hid(), JILL_LOGDATASET_NAME));
                // compare datatype
                arf::h5t::wrapper<jill::file::message_t> t;
                arf::h5t::datatype expected(t);
                if (expected != *(_log->datatype())) {
                        throw arf::Exception(JILL_LOGDATASET_NAME " has wrong datatype");
                }
        }
        else {
                _log = _file->create_packet_table<jill::file::message_t>(JILL_LOGDATASET_NAME);
        }
        _do_log(make_string() << "[" << _agent_name << "] opened file: " << filename);

        _get_last_entry_index();
}

arf_writer::~arf_writer()
{
        // need to make sure writer thread is stopped before cleanup of this
        // class's members begins
        stop();                 // no more new data; exit writer thread
        join();                 // wait for writer thread to exit
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
                if (rc == 1 && val > _entry_idx) _entry_idx = val;
        }
}

void
arf_writer::new_entry(nframes_t sample_count)
{
        char name[32];
        char const * templ = "jrecord_%04d";

        _entry_idx += 1;
        sprintf(name, templ, _entry_idx);

        if (_entry) {
                _do_log(make_string() << "[" << _agent_name << "] closed entry: " << _entry->name());
        }

        timeval tp;
        gettimeofday(&tp, 0);
        _entry.reset(new arf::entry(*_file, name, &tp));
        _do_log(make_string() << "[" << _agent_name << "] created entry: " << _entry->name());

        arf::h5a::node::attr_writer n = _entry->write_attribute();
        n("jack_frame",sample_count);
        for_each(_attrs.begin(), _attrs.end(), n);
        if (_client) {
                n("jack_usec",_client->time(sample_count));
        }

}

void
arf_writer::write(period_info_t const * info)
{
        // start new entry?
        // in triggered mode?
        if (!_entry && _trigger) {
                // check trigger port
                //new_entry(info->time);
        }
        // in continuous mode, will we overflow the sample count?
        else if (!_entry || info->time < _entry_start) {
                new_entry(info->time);
        }
        // get jack port information
        if (info->arg) {
                jack_port_t *port = static_cast<jack_port_t*>(info->arg);
                // use name information in port to look up dataset
                // create dataset as needed
        }
        else {
                // otherwise how do we know what channel?
        }
}

void
arf_writer::flush()
{
        if (_file) _file->flush();
}
