/*
 * JILL - C++ framework for JACK
 *
 * includes code from klick, Copyright (C) 2007-2009  Dominic Sacre  <dominic.sacre@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _DEBUG_HH
#define _DEBUG_HH

#ifndef NDEBUG
    #include <assert.h>
    #define ASSERT(f) assert(f)
    #define VERIFY(f) assert(f)
    #define FAIL()    assert(false)
#else
    #define ASSERT(f) ((void)0)
    #define VERIFY(f) ((void)(f))
    #define FAIL()    ((void)0)
#endif

#endif // _DEBUG_HH
