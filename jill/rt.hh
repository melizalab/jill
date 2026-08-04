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
#ifndef _RT_HH
#define _RT_HH

/**
 * JILL_RT marks a function that runs on the JACK realtime thread.
 *
 * Everything reached from a process callback has to complete in bounded time
 * without blocking, which rules out allocating, freeing, taking a lock, doing
 * I/O, and making a syscall that can sleep. Until now that rule lived in
 * comments and in whoever last read the code. This makes it something the
 * toolchain checks.
 *
 * Under clang the macro expands to [[clang::nonblocking]], which does two
 * things. A build with `scons sanitize=realtime` links RealtimeSanitizer,
 * which intercepts the libc and pthread entry points and aborts with a stack
 * trace the moment one of them is called inside an annotated function. And
 * -Wfunction-effects, if enabled, warns at compile time when an annotated
 * function calls one that carries no such promise.
 *
 * The macro also carries noexcept, on every compiler. That is not padding to
 * silence clang's -Wperf-constraint-implies-noexcept: throwing allocates, and
 * an exception unwinding out of a process callback would cross into JACK's C
 * frames, which is undefined behaviour. Declaring it noexcept turns that into
 * a clean std::terminate with the exception's message instead. So a gcc build
 * still enforces half the contract even though it cannot check the rest.
 *
 * Beyond noexcept the annotation is a diagnostic, not a code generation
 * directive: annotating a function does not make it realtime safe, it only
 * asks to be told when it is not.
 *
 * Note the placement. This is a type attribute and belongs after the parameter
 * list, not before the return type:
 *
 *     int process(jack_client *c, nframes_t n, nframes_t t) JILL_RT
 *     {
 *             ...
 *     }
 *
 * A function that must call something unsafe on a path that provably cannot
 * run in the realtime thread can wrap it in __rtsan::ScopedDisabler, but
 * prefer restructuring: the annotation is only worth having if it is honest.
 */
#if defined(__clang__) && defined(__has_cpp_attribute)
#  if __has_cpp_attribute(clang::nonblocking)
#    define JILL_RT noexcept [[clang::nonblocking]]
#  endif
#endif

#ifndef JILL_RT
#  define JILL_RT noexcept
#endif

#endif
