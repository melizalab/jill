/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for jill::util::event_randomizer.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "jill/util/event_randomizer.hh"

using jill::util::event_randomizer;

namespace {

/* the control parameter the class documentation recommends */
const float control = 8.0;
const float seed = 1024;

}

TEST_CASE("a proportion outside [0, 1] is rejected") {
        CHECK_THROWS_AS(event_randomizer(1.5, control, seed), std::out_of_range);
        CHECK_THROWS_AS(event_randomizer(-0.1, control, seed), std::out_of_range);
        CHECK_NOTHROW(event_randomizer(0.0, control, seed));
        CHECK_NOTHROW(event_randomizer(1.0, control, seed));
}

TEST_CASE("a proportion of zero never emits an event") {
        event_randomizer randomizer(0.0, control, seed);
        for (int i = 0; i < 100; ++i) {
                CHECK_FALSE(randomizer.present("a"));
        }
        CHECK(randomizer.get_events("a") == std::pair<int, int>(0, 100));
        CHECK(randomizer.get_proportion("a") == doctest::Approx(0.0));
}

TEST_CASE("a proportion of one always emits an event") {
        event_randomizer randomizer(1.0, control, seed);
        for (int i = 0; i < 100; ++i) {
                CHECK(randomizer.present("a"));
        }
        CHECK(randomizer.get_events("a") == std::pair<int, int>(100, 100));
        CHECK(randomizer.get_proportion("a") == doctest::Approx(1.0));
}

TEST_CASE("every presentation is counted, whatever the outcome") {
        event_randomizer randomizer(0.5, control, seed);
        int emitted = 0;
        for (int i = 0; i < 50; ++i) {
                if (randomizer.present("a")) emitted += 1;
        }
        auto events = randomizer.get_events("a");
        CHECK(events.second == 50);
        CHECK(events.first == emitted);
        CHECK(randomizer.get_proportion("a") == doctest::Approx(float(emitted) / 50.0f));
}

TEST_CASE("stimuli are tracked independently") {
        event_randomizer randomizer(0.5, control, seed);
        for (int i = 0; i < 20; ++i) {
                randomizer.present("a");
        }
        for (int i = 0; i < 5; ++i) {
                randomizer.present("b");
        }
        CHECK(randomizer.get_events("a").second == 20);
        CHECK(randomizer.get_events("b").second == 5);
        CHECK(randomizer.size() == 2);
}

TEST_CASE("an unseen stimulus has no counts to report") {
        event_randomizer randomizer(0.5, control, seed);
        CHECK_THROWS_AS(randomizer.get_events("never-presented"), std::out_of_range);
}

TEST_CASE("the same seed produces the same sequence") {
        event_randomizer a(0.5, control, seed);
        event_randomizer b(0.5, control, seed);
        for (int i = 0; i < 50; ++i) {
                CHECK(a.present("s") == b.present("s"));
        }
        CHECK(a.get_events("s") == b.get_events("s"));
}

TEST_CASE("different seeds diverge") {
        event_randomizer a(0.5, control, 1);
        event_randomizer b(0.5, control, 99);
        std::vector<bool> left, right;
        for (int i = 0; i < 50; ++i) {
                left.push_back(a.present("s"));
                right.push_back(b.present("s"));
        }
        CHECK(left != right);
}

TEST_CASE("the observed proportion is steered towards the target") {
        // the class documentation claims a control parameter of 8-10 brings
        // the observed proportion close to target within about 20 trials
        for (float target : {0.25f, 0.5f, 0.75f}) {
                event_randomizer randomizer(target, control, seed);
                for (int i = 0; i < 20; ++i) {
                        randomizer.present("a");
                }
                CAPTURE(target);
                CHECK(randomizer.get_proportion("a") == doctest::Approx(target).epsilon(0.15));
        }
}

TEST_CASE("reset clears counts") {
        event_randomizer randomizer(0.5, control, seed);
        randomizer.present("a");
        randomizer.present("b");
        REQUIRE(randomizer.size() == 2);

        SUBCASE("for a single stimulus") {
                randomizer.reset("a");
                CHECK(randomizer.size() == 1);
                CHECK_THROWS_AS(randomizer.get_events("a"), std::out_of_range);
                CHECK(randomizer.get_events("b").second == 1);
        }

        SUBCASE("for every stimulus") {
                randomizer.reset();
                CHECK(randomizer.empty());
                CHECK_THROWS_AS(randomizer.get_events("b"), std::out_of_range);
        }
}

/* The table is fixed at construction because growing it would mean allocating
 * in the realtime thread. These cover what that costs: a capacity to run into,
 * and a name length to exceed. */

TEST_CASE("a capacity of zero is rejected") {
        CHECK_THROWS_AS(event_randomizer(0.5, control, seed, 0), std::out_of_range);
        CHECK_NOTHROW(event_randomizer(0.5, control, seed, 1));
}

TEST_CASE("stimuli are tracked up to the capacity") {
        event_randomizer randomizer(1.0, control, seed, 3);
        REQUIRE(randomizer.capacity() == 3);
        for (int i = 0; i < 3; ++i) {
                randomizer.present(std::string(1, 'a' + i).c_str());
        }
        CHECK(randomizer.size() == 3);
        CHECK_FALSE(randomizer.overflowed());
}

TEST_CASE("a stimulus past the capacity is presented but not tracked") {
        // a proportion of one, so the fallback draw is still deterministic
        event_randomizer randomizer(1.0, control, seed, 2);
        randomizer.present("a");
        randomizer.present("b");
        REQUIRE(randomizer.size() == 2);
        REQUIRE_FALSE(randomizer.overflowed());

        // the third one still gets a decision, just an unbalanced one
        CHECK(randomizer.present("c"));
        CHECK(randomizer.overflowed());
        CHECK(randomizer.size() == 2);
        CHECK_THROWS_AS(randomizer.get_events("c"), std::out_of_range);

        // and the stimuli that did fit are unaffected
        CHECK(randomizer.get_events("a").second == 1);
        CHECK(randomizer.get_events("b").second == 1);
}

TEST_CASE("overflow does not disturb the stimuli already in the table") {
        event_randomizer randomizer(0.5, control, seed, 1);
        for (int i = 0; i < 20; ++i) {
                randomizer.present("kept");
                randomizer.present("dropped");
        }
        CHECK(randomizer.overflowed());
        CHECK(randomizer.get_events("kept").second == 20);
}

TEST_CASE("a name at the length limit is stored whole") {
        event_randomizer randomizer(1.0, control, seed);
        const std::string name(event_randomizer::max_name, 'x');
        randomizer.present(name.c_str());
        CHECK_FALSE(randomizer.truncated());
        CHECK(randomizer.get_events(name).second == 1);
}

TEST_CASE("a longer name is truncated and says so") {
        event_randomizer randomizer(1.0, control, seed);
        const std::string name(event_randomizer::max_name + 10, 'x');
        randomizer.present(name.c_str());
        CHECK(randomizer.truncated());
        // still findable by its full name, because the length is kept
        CHECK(randomizer.get_events(name).second == 1);
}

TEST_CASE("names that truncate alike are told apart by length") {
        event_randomizer randomizer(1.0, control, seed);
        const std::string prefix(event_randomizer::max_name, 'x');
        const std::string one = prefix + "a";
        const std::string two = prefix + "bb";
        randomizer.present(one.c_str());
        randomizer.present(two.c_str());
        CHECK(randomizer.size() == 2);
        CHECK(randomizer.get_events(one).second == 1);
        CHECK(randomizer.get_events(two).second == 1);
}

TEST_CASE("a name is matched on its whole length, not a prefix") {
        event_randomizer randomizer(1.0, control, seed);
        randomizer.present("stim");
        randomizer.present("stimulus");
        CHECK(randomizer.size() == 2);
        CHECK(randomizer.get_events("stim").second == 1);
        CHECK(randomizer.get_events("stimulus").second == 1);
}

TEST_CASE("a name need not be null-terminated") {
        event_randomizer randomizer(1.0, control, seed);
        // the realtime caller passes a pointer into a MIDI buffer, where the
        // bytes after the label belong to whatever follows
        const char buffer[] = "abcdef";
        randomizer.present(buffer, 3);
        CHECK(randomizer.size() == 1);
        CHECK(randomizer.get_events("abc").second == 1);
        CHECK_THROWS_AS(randomizer.get_events("abcdef"), std::out_of_range);
}

TEST_CASE("reset removes one stimulus without disturbing the others") {
        event_randomizer randomizer(1.0, control, seed, 4);
        randomizer.present("a");
        randomizer.present("b");
        randomizer.present("c");
        // removing from the middle is the case the swap-with-last has to get right
        randomizer.reset("b");
        CHECK(randomizer.size() == 2);
        CHECK_THROWS_AS(randomizer.get_events("b"), std::out_of_range);
        CHECK(randomizer.get_events("a").second == 1);
        CHECK(randomizer.get_events("c").second == 1);
}

TEST_CASE("resetting an unknown stimulus does nothing") {
        event_randomizer randomizer(1.0, control, seed);
        randomizer.present("a");
        CHECK_NOTHROW(randomizer.reset("nonesuch"));
        CHECK(randomizer.size() == 1);
}

TEST_CASE("space freed by reset can be reused") {
        event_randomizer randomizer(1.0, control, seed, 1);
        randomizer.present("a");
        randomizer.reset("a");
        randomizer.present("b");
        CHECK_FALSE(randomizer.overflowed());
        CHECK(randomizer.get_events("b").second == 1);
}

TEST_CASE("iteration reports every stimulus exactly once") {
        event_randomizer randomizer(1.0, control, seed, 8);
        randomizer.present("a");
        randomizer.present("a");
        randomizer.present("b");

        int seen = 0, total = 0;
        for (auto const & e : randomizer) {
                seen += 1;
                total += e.presentations;
                CHECK(e.len == 1);
        }
        CHECK(seen == 2);
        CHECK(total == 3);
        CHECK(seen == static_cast<int>(randomizer.size()));
}
