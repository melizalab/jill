/*
 * JILL - C++ framework for JACK
 *
 * additions Copyright (C) 2013 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _EVENT_RANDOMIZER_HH
#define _EVENT_RANDOMIZER_HH

#include <random>

namespace jill { namespace util {

/**
 * Tracks how many times an event has been emitted for a collection of stimuli
 * (or any other string label). Useful when you want to only generate
 * optical stimulus pulses on a subset of trials, but need to balance
 * proportions across stimuli. The implementation is not quite random without
 * replacement; instead, the current proportion is compared against a fixed
 * probability, and a random draw is taken to push the current proportion
 * towards the desired target.
 *
 * The randomizer is initialized with a desired proportion (prob) and a
 * proportional control parameter that determines how much of a correction to
 * make on the next trial. Setting this parameter between around 8-10 should
 * ensure that the final observed proportion for each stimulus is relatively
 * close after 20 presentations. Smaller values add more randomness at the
 * expense of larger variance around the target.
 */
class event_randomizer {

public:
        event_randomizer(float prob, float control, float rng_seed);

        /** look up whether to emit an event for a given stimulus */
        bool present(std::string const & name);

        /** get the current number of events and total presentations for a given stimulus */
        std::pair<int, int> get_events(std::string const & name) const;

        /** get the current proportion of events per total presentations */
        float get_proportion(std::string const & name) const;

	/** access the event count map */
	const std::map<std::string, std::pair<int, int> > & counts() const {
		return _counts;
	}

	/** reset the counts for one stimulus */
	void reset(std::string const & name);
	
        /** reset all the counts */
        void reset();

private:
        float _desired_proportion;
	float _control;
        std::map<std::string, std::pair<int, int> > _counts;
	std::mt19937 _rng;
	std::uniform_real_distribution<float> _distribution;

};

}}

#endif
