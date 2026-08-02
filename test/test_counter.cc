/*
 * JILL - C++ framework for JACK
 *
 * Unit tests for jill::dsp::running_counter.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "jill/dsp/counter.hh"

using jill::dsp::running_counter;

TEST_CASE("running_counter starts empty") {
        running_counter<int> counter(4);
        CHECK_FALSE(counter.full());
        CHECK(counter.running_count() == 0);
}

TEST_CASE("running_counter accumulates until the window is full") {
        const std::size_t size = 10;
        running_counter<int> counter(size);
        int expected = 0;

        for (std::size_t i = 0; i < size; ++i) {
                CHECK_FALSE(counter.full());
                expected += static_cast<int>(i);
                counter.push(static_cast<int>(i));
                CHECK(counter.running_count() == expected);
        }
        CHECK(counter.full());
}

TEST_CASE("a full running_counter drops the oldest value") {
        const std::size_t size = 10;
        running_counter<int> counter(size);
        int expected = 0;
        for (std::size_t i = 0; i < size; ++i) {
                expected += static_cast<int>(i);
                counter.push(static_cast<int>(i));
        }

        // pushing the same sequence again evicts it in the same order, so the
        // running total has to come back to where it was
        for (std::size_t i = 0; i < size; ++i) {
                CHECK(counter.full());
                counter.push(static_cast<int>(i));
                CHECK(counter.running_count() == expected);
        }
}

TEST_CASE("a full running_counter tracks a changing signal") {
        running_counter<int> counter(3);
        counter.push(1);
        counter.push(2);
        counter.push(3);
        REQUIRE(counter.running_count() == 6);

        counter.push(4);        // evicts 1
        CHECK(counter.running_count() == 9);
        counter.push(5);        // evicts 2
        CHECK(counter.running_count() == 12);

        // once the window holds a constant value the total is size * value
        for (int i = 0; i < 3; ++i) counter.push(10);
        CHECK(counter.running_count() == 30);
}

TEST_CASE("reset empties the running_counter") {
        running_counter<int> counter(3);
        counter.push(5);
        counter.push(7);
        REQUIRE(counter.running_count() == 12);

        counter.reset();
        CHECK_FALSE(counter.full());
        CHECK(counter.running_count() == 0);

        // and it is usable afterwards
        counter.push(2);
        CHECK(counter.running_count() == 2);
}

TEST_CASE("a window of one holds only the last value") {
        running_counter<int> counter(1);
        counter.push(4);
        CHECK(counter.full());
        CHECK(counter.running_count() == 4);
        counter.push(9);
        CHECK(counter.running_count() == 9);
}

TEST_CASE("running_counter handles negative values") {
        running_counter<int> counter(3);
        counter.push(5);
        counter.push(-3);
        CHECK(counter.running_count() == 2);
        counter.push(-10);
        CHECK(counter.running_count() == -8);
}

TEST_CASE("running_counter works with a floating point type") {
        running_counter<float> counter(2);
        counter.push(0.5f);
        counter.push(0.25f);
        CHECK(counter.running_count() == doctest::Approx(0.75f));
        counter.push(0.25f);    // evicts 0.5
        CHECK(counter.running_count() == doctest::Approx(0.5f));
}
