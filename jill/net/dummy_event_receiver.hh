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

#include "../data_writer.hh"

namespace jill { namespace net {

/** A dummy data_writer that outputs event messages to the console */
class dummy_event_receiver : public data_writer {
public:
        void write(data_block_t const * block, nframes_t, nframes_t) override {
                if (block->sz_data == 0) return;
                if (block->dtype == SAMPLED) return;
                midi::event_view ev(*block);
                std::string encoded = ev.message();
                INFO << ev.status() << ": " << encoded;
        }

};


}} // jill::net


#endif
