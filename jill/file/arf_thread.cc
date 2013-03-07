/* @file arf_sndfile.cc
 *
 * Copyright (C) 2011 C Daniel Meliza <dan@meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "arf_thread.hh"
#include <boost/format.hpp>
#include <sys/time.h>
#include <arf.hpp>
#include "../jack_client.hh"
#include "../midi.hh"
#include "../util/string.hh"
#include "../dsp/period_ringbuffer.hh"

using namespace std;
using namespace jill;
using namespace jill::file;

using jill::util::make_string;

pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  data_ready = PTHREAD_COND_INITIALIZER;

arf_thread::arf_thread(string const & filename,
                       vector<port_info_t> const * ports,
                       map<string,string> const * attrs,
                       jack_client * client,
                       dsp::period_ringbuffer * ringbuf,
                       int compression)
        : _client(client), _ringbuf(ringbuf),
          _ports(ports), _attrs(attrs),
          _xruns(0), _stop(0), _compression(compression)
{
        open_arf(filename);
}

arf_thread::~arf_thread()
{
        // prevent access to file during destruction
        pthread_mutex_lock (&disk_thread_lock);
        _dsets.clear();
        _entry.reset();
        _file.reset();
	pthread_mutex_unlock (&disk_thread_lock);
}

void
arf_thread::new_entry(nframes_t frame, timeval const * timestamp)
{

        int idx = _file->nchildren();
        boost::format fmt("entry_%|06|");
        fmt % idx;

        _entry.reset(new arf::entry(*_file, fmt.str(), timestamp));
        _entry->write_attribute()("jack_frame",frame)("jack_usec",_client->time(frame));
        if (_attrs) {
                for_each(_attrs->begin(), _attrs->end(), _entry->write_attribute());
        }

}

void
arf_thread::new_datasets()
{
        _dsets.clear();
        if (_ports==0) return;
        for (vector<port_info_t>::const_iterator it = _ports->begin(); it != _ports->end(); ++it) {
                arf::packet_table_ptr pt;
                if (it->storage >= arf::INTERVAL) {
                        pt = _entry->create_packet_table<arf::interval>(it->name,"samples",it->storage,
                                                                        false, 1024, _compression);
                }
                else if (it->storage >= arf::EVENT) {
                        pt = _entry->create_packet_table<nframes_t>(it->name,"samples",it->storage,
                                                                   false, 1024, _compression);
                }
                else {
                        pt = _entry->create_packet_table<sample_t>(it->name,"",it->storage,
                                                                   false, 1024, _compression);
                }
                // times are stored in units of samples for maximum precision,
                // which requires sample rates to be known.
                pt->write_attribute("sampling_rate", _client->sampling_rate());
                _dsets.push_back(pt);
        }
}

void
arf_thread::open_arf(string const & filename)
{
        _file.reset(new arf::file(filename, "a"));
}

void
arf_thread::log(string const & msg)
{
        // log may be called by a jack callback
        pthread_mutex_lock (&disk_thread_lock);
        if (_file)
                _file->log(msg);
	pthread_mutex_unlock (&disk_thread_lock);
}

void
arf_thread::start()
{
        int ret = pthread_create(&_thread_id, NULL, arf_thread::write_continuous, this);
        if (ret != 0)
                throw std::runtime_error("Failed to start disk thread");
}

void
arf_thread::stop()
{
        _stop = 1;
}


void
arf_thread::join()
{
        pthread_join(_thread_id, NULL);
}

/*
 * Write data to arf file in continous mode.
 *
 * @pre client is started and ports registered
 * @pre outfile is initialized
 */
void *
arf_thread::write_continuous(void * arg)
{
        arf_thread * self = static_cast<arf_thread *>(arg);
        assert(self->_ports);
        dsp::period_ringbuffer::period_info_t const * period;
        long my_xruns = 0;                       // internal counter
        timeval tp;
        nframes_t entry_start;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	pthread_mutex_lock (&disk_thread_lock);

        while (1) {
                period = self->_ringbuf->request();
                if (period) {
                        // cout << "got period" << endl;
                        assert(period->nchannels == self->_ports->size());
                        // create entry when first data chunk arrives. Also
                        // break entries if the counter overflows to ensure that
                        // sample-based time values within the entry are
                        // consistent. This may happen early for the first
                        // entry, but the math is too hard otherwise...
                        if (!self->_entry || period->time < entry_start) {
                                gettimeofday(&tp,0);
                                entry_start = period->time;
                                self->new_entry(period->time, &tp);
                                self->new_datasets();
                        }

                        // write channels
                        size_t i = 0;
                        vector<port_info_t>::const_iterator p = self->_ports->begin();
                        for (; i < period->nchannels; ++i, ++p) {
                                void * data = self->_ringbuf->peek(i);
                                // look up channel name and type
                                if (p->storage < arf::EVENT)  {
                                        self->_dsets[i]->write(data, period->nbytes / sizeof(sample_t));
                                }
                                else {
                                        jack_midi_event_t event;
                                        nframes_t adj_time;
                                        nframes_t nevents = jack_midi_get_event_count(data);
                                        for (nframes_t j = 0; j < nevents; ++j) {
                                                jack_midi_event_get(&event, data, j);
                                                adj_time = event.time + period->time - entry_start;
                                                if (p->storage < arf::INTERVAL) {
                                                        self->_dsets[i]->write(&adj_time,1);
                                                }
                                        }
                                }
                        }
                        // free data
                        self->_ringbuf->pop_all(0);
                        // check for overrun
                        if (my_xruns < self->_xruns) {

                        }
                }
                else if (self->_stop) {
                        goto done;
                }
                else {
                        // wait on data_ready
                        // cout << "waiting" << endl;
                        pthread_cond_wait (&data_ready, &disk_thread_lock);
                }
        }

done:
        self->_file->flush();
	pthread_mutex_unlock (&disk_thread_lock);
        return 0;
}

/*
 * notes on storing interval data. The tricky thing is that we have to track
 * open events across multiple periods, and only write the data to the file when
 * the close event arrives. This format is really suboptimal from the
 * perspective of this function, though it may be simpler to read out. I know
 * this storage format isn't being used much (if at all), so I could break the
 * spec if needed.  something like below. Another thing to think about is
 * defining a format that will also work for behavioral events.  Pecks would be
 * fine as times, but if I'm tracking occupancy I may want to store more than
 * just times.
 *
 * Also bear in mind that jill will not be the only program writing this type of data
 */
struct interval {
        char name[64];          // label for the event
        boost::uint32_t time;   // time of the event
        boost::uint32_t state;  // on/off/other
};

