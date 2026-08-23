/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#ifndef _DUMMY_EVENT_RECEIVER_HH
#define _DUMMY_EVENT_RECEIVER_HH

#include <sstream>

#include "../data_writer.hh"
#include "../logging.hh"
#include "../midi.hh"

namespace jill { namespace net {

/**
 * A data_writer that prints event messages to the log instead of forwarding
 * them anywhere. This is what makes jrelay usable as an event monitor.
 */
class dummy_event_receiver : public data_writer {
public:
        void write(data_block_t const * block, nframes_t, nframes_t) override {
                if (block->sz_data == 0) return;
                if (block->dtype == SAMPLED) return;
                midi::event_view ev(*block);
                // The frame is worth printing even though the log line
                // carries a timestamp: the timestamp is when the writer
                // thread drained the block, which is a poll interval later
                // than the event, while the frame is the one jrecord stores
                // as jack_frame and so is what a recording can be lined up
                // against.
                log_msg m;
                m << block->id() << " @ " << block->time << ": "
                  << ev.status() << " " << ev.message();
        }

};


}} // jill::net


#endif
