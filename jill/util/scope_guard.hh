/*
 * JILL - C++ framework for JACK
 *
 * Copyright (C) 2010-2026 C Daniel Meliza <dan || meliza.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _SCOPE_GUARD_HH
#define _SCOPE_GUARD_HH

#include <utility>

namespace jill { namespace util {

/**
 * Runs a callable when it goes out of scope, however the scope is left.
 *
 * The modules keep the objects their JACK callbacks need at file scope, so
 * those objects outlive main() and are destroyed during static destruction --
 * after the JACK client is gone, after the objects they hold references to are
 * gone, and after libraries such as HDF5 have finalized themselves. Tearing
 * down at the end of a try block covers only the happy path; an exception
 * skips it. Declaring one of these immediately after the client means teardown
 * runs on every path out, and runs before the client, because members of a
 * scope are destroyed in reverse order of declaration.
 *
 *     jack_client client(name);
 *     util::scope_guard teardown{[&]{ client.deactivate(); thing.reset(); }};
 *
 * The callable must not throw. These run during stack unwinding, where an
 * escaping exception would call std::terminate, so anything thrown is
 * swallowed here instead: losing a diagnostic during shutdown is a great deal
 * better than aborting in the middle of reporting the original error.
 */
template <typename Callable>
class scope_guard {

public:
        explicit scope_guard(Callable f) : _f(std::move(f)) {}

        ~scope_guard()
        {
                try {
                        _f();
                }
                catch (...) {
                        // see the note above: never propagate from here
                }
        }

        scope_guard(scope_guard const &) = delete;
        scope_guard & operator=(scope_guard const &) = delete;

private:
        Callable _f;
};

}} // namespace jill::util

#endif
