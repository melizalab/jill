# Debian installation

These instructions have been tested on Debian 12 and should work any other
Debian-based Linux system, though some package names may differ. They should also work on Red Hat
based systems, but you'll have to find the equivalent packages in your package
manager.

## JACK and qjackctl

```shell
sudo apt-get install jackd2 libjack-jackd2-dev qjackctl
```

If you wish, you can substitute JACK 2 for JACK 1. Qjackctl is not necessary, but it's really useful for connecting clients.

## YASS

YASS is a simple scrolling oscilloscope. It's not necessary, but it's used in the tutorials to visualize signals in JACK.

```shell
git clone https://github.com/dmeliza/yass.git
cd yass
make
sudo make install
```

At this point you're ready to go through the first tutorial.

## JILL prerequisites

Most of the prerequisites are available through package managers:

```shell
sudo apt-get install scons libboost-system-dev libboost-date-time-dev libboost-program-options-dev \
 libboost-filesystem-dev libsndfile1-dev libsamplerate0-dev libzmq3-dev
```

If you want to use `jrecord`, you need to install hdf5:

```shell
sudo apt-get install hdf5-helpers hdf5-tools libhdf5-dev
```

The ARF headers are tracked as a git submodule, pinned to a known-good release.
Fetch them by cloning with `--recursive`, or, in an existing checkout:

```shell
# run this command in the jill repository
git submodule update --init
```

## Building JILL

```shell
scons -Q modules
```

To omit compiling `jrecord`, run `scons -Q --no-arf modules` instead. You can run the modules from their build directory. (e.g. `modules/jstim`). To install modules in a system location, use the following command:

```shell
scons -Q install
```

The default install location is /usr/local/bin, but this can be changed with the `prefix` argument.

## Running the tests

The unit tests are written against [doctest](https://github.com/doctest/doctest),
and the ARF file format is checked from python with h5py. Neither is needed
to build the modules, only to build and run the tests:

```shell
sudo apt-get install doctest-dev python3-pytest python3-h5py
```

Build the suites and run them all with a single command, from the top of the
source tree:

```shell
scons -Q test
pytest
```

Each suite is a separate binary under `test/`, run as its own process so that
one crashing or hanging cannot take the rest of the run down. `pytest` reports
a single pass or fail for the lot.

Some of the older programs need external resources, and are tagged so they can
be left out:

```shell
pytest -m "not needs_jack"   # skip anything needing a JACK server
pytest -m "not needs_arf"    # skip the ARF/HDF5 format tests
```

Tests needing a JACK server get one automatically: a `jackd -d dummy` is
started for them and shut down afterwards, so no sound hardware is involved.
`test_zmq_server` runs until interrupted and is skipped by default; start it by
hand if you need it.

You can also run a single binary directly, which is often quicker when working
on one area:

```shell
./test/test_ringbuf
./test/test_ringbuf --test-case="*mirrored*"
```

### Sanitizer and soak runs

Races and leaks rarely appear on a single pass of a clean build, so the suites
can be built against the runtime sanitizers and run repeatedly:

```shell
scons -Q -c
scons -Q debug=1 sanitize=address test    # or sanitize=thread
pytest --soak=50
```

Combine `sanitize=` with `debug=1`: a sanitizer build with `NDEBUG` set has all
its assertions compiled out of exactly the code being checked. `--soak=N` runs
each suite N times and reports which iteration failed, so an intermittent
problem can be told from a consistent one.

LeakSanitizer suppressions live in `test/lsan.supp` and are applied
automatically by the driver. HDF5 registers process-global state on first use
and never frees it, which would otherwise be reported on every single run.

`sanitize=thread` currently fails, and is meant to: two suites report genuine
data races that are being worked through as part of the concurrency audit.
`test_stimset` reports `readahead_stimqueue`, where `head()` and `release()`
touch `_head` without the lock the background thread holds when writing it.
`test_ringbuf_concurrent` reports the ringbuffers themselves, which advance
their read and write pointers atomically but declare them as plain `size_t`
and read them without synchronization; the payload copies and block headers
race for the same reason. Neither is a problem with your build.

`test_ringbuf_concurrent` exists specifically to make that second one visible.
Nothing else drives the ringbuffers from two threads, so before it was written
the most important race in the codebase produced no sanitizer output at all.
