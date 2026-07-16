/*
 * JILL - C++ framework for JACK
 *
 * Allocates memory which has a contiguous virtual mirror. This is extremely
 * useful for ringbuffers because read and write functions can access their
 * space as a single unbroken array.  Based on virtual ringbuffer.
 *
 * Copyright (C) 2010-2012 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */
#include <stdexcept>
#include <map>
#include "event_randomizer.hh"

using namespace jill::util;

event_randomizer::event_randomizer(float prob, float control, float rng_seed)
        : _desired_proportion(prob), _control(control), _rng(rng_seed), _distribution(0.0, 1.0) {
        if (prob > 1.0 || prob < 0.0) {
                throw std::out_of_range("desired proportion must be between 0.0 and 1.0");
        }

}

bool
event_randomizer::present(std::string const & name)
{
	float new_prob(_desired_proportion);
        std::pair<int, int> & events = _counts[name];
	if (events.second > 0) {
		float observed_proportion = float(events.first) / float(events.second);
		float err = observed_proportion - _desired_proportion;
		new_prob = std::min(std::max(_desired_proportion - _control * err, 0.0f), 1.0f);
	}
        float draw = _distribution(_rng);
        events.second += 1;
        if (draw < new_prob) {
                events.first += 1;
                return true;
        }
        else {
                return false;
        }
}

std::pair<int, int>
event_randomizer::get_events(std::string const & name) const
{
        return _counts.at(name);
}

float
event_randomizer::get_proportion(std::string const & name) const
{
        auto events = get_events(name);
        return float(events.first) / float(events.second);
}

void
event_randomizer::reset(std::string const & name)
{
        _counts.erase(name);
}

void
event_randomizer::reset()
{
        _counts.clear();
}
