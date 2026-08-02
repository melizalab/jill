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
#ifndef _DAYTIME_HH
#define _DAYTIME_HH

#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace jill { namespace util {

/**
 * Whether a time of day falls inside a recurring daily window.
 *
 * The window runs from @p start to @p stop, is half-open -- @p start itself is
 * outside it and @p stop is inside -- and wraps around midnight when @p stop is
 * earlier in the day than @p start. A window of 22:00 to 06:00 therefore covers
 * the night, and one of 06:00 to 22:00 covers the day.
 *
 * When @p start and @p stop are equal the window is treated as covering the
 * whole day, which is the wrap-around branch applied to an empty interval.
 *
 * @param start  the time of day the window opens
 * @param stop   the time of day the window closes
 * @param now    the time of day to test
 * @return       true if @p now lies inside the window
 */
inline bool
is_daytime(boost::posix_time::time_duration const & start,
           boost::posix_time::time_duration const & stop,
           boost::posix_time::time_duration const & now)
{
        if (stop > start) {
                return (start < now) && (now <= stop);
        }
        else {
                return !((stop < now) && (now <= start));
        }
}

}} // namespace jill::util

#endif
