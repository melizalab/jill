#include <cassert>

#include "jill/util/event_randomizer.hh"

using namespace jill::util;

int main(int, char**)
{
	const float desired = 0.5;
	const int n_trials = 20;
	event_randomizer randomizer(desired, 8, 1024);
	for (int i = 0; i < n_trials; ++i) {
		randomizer.present("A");
		randomizer.present("B");
		randomizer.present("C");
	}

	std::cout << "Results after " << n_trials << " trials, desired p=" << desired << std::endl;
	for (const auto & item : randomizer.counts()) {
		std::cout << item.first << ": " << item.second.first << "/" << item.second.second << std::endl;
	}
}
