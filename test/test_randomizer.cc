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
        CHECK(randomizer.counts().size() == 2);
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
        REQUIRE(randomizer.counts().size() == 2);

        SUBCASE("for a single stimulus") {
                randomizer.reset("a");
                CHECK(randomizer.counts().size() == 1);
                CHECK_THROWS_AS(randomizer.get_events("a"), std::out_of_range);
                CHECK(randomizer.get_events("b").second == 1);
        }

        SUBCASE("for every stimulus") {
                randomizer.reset();
                CHECK(randomizer.counts().empty());
                CHECK_THROWS_AS(randomizer.get_events("b"), std::out_of_range);
        }
}
