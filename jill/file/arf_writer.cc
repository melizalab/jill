

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
          _entry_start(0),
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
}

arf_writer::~arf_writer()
{}

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
arf_writer::new_entry(nframes_t sample_count)
{

}

void
arf_writer::write(period_info_t const * info)
{
        // start new entry?
        // in triggered mode?
        if (!_entry && _trigger) {
        }
        // overflow the sample count?
        else if (!_entry || info->time < _entry_start) {
                new_entry(info->time);
        }

}
