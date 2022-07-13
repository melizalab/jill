#!/usr/bin/env python
# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Calculate time clock jitter statistics

Usage: arf_jitter.py <arffiles>

Copyright (C) 2013 Dan Meliza <dmeliza@gmail.com>
Created Tue Jul  2 15:00:18 2013
"""

import h5py
import operator
import sys


def timestamp_to_float(arr):
    """convert two-element timestamp (sec, usec) to a floating point (sec since epoch)"""
    from numpy import dot

    return dot(arr, (1.0, 1e-6))


def clock_jitter(fp):
    """calculate jitter between different clocks in jill files"""
    from numpy import diff

    timevals = sorted(
        (
            (
                e.attrs["jack_frame"],
                e.attrs["jack_usec"],
                timestamp_to_float(e.attrs["timestamp"]),
            )
            for e in fp.itervalues()
            if "jack_frame" in e.attrs
        ),
        key=operator.itemgetter(2),
    )

    frames, usecs, secs = (diff(f) for f in zip(*timevals))

    return (1.0 * usecs / frames, frames / secs, usecs / secs)


if __name__ == "__main__":

    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(-1)

    print("# Comparison of jack frame, jack usec, and system clocks (seconds):")
    print(
        "\t".join(
            (
                "filename",
                "usec.frame",
                "uf.sd",
                "frame.sys",
                "fs.sd",
                "sys.usec",
                "su.sd",
            )
        )
    )
    templ = "%s\t" + "\t".join(("%.3f",) * 6)
    for arg in sys.argv[1:]:
        fp = h5py.File(arg, "r")
        diffs = clock_jitter(fp)
        print(
            templ
            % (
                arg,
                diffs[0].mean(),
                diffs[0].std(),
                diffs[1].mean(),
                diffs[1].std(),
                diffs[2].mean(),
                diffs[2].std(),
            )
        )

# Variables:
# End:
