# Testing notes

How to build and run the test suites, and what the various build modes check.
The prerequisites differ by platform and are listed in
[Debian](debian-installation.md) and [OS X](osx-installation.md); everything
here applies to both.

## Running the tests

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
JACK on macOS is less predictable than on Linux, so if `jackd` cannot be
started those tests skip rather than fail. `test_zmq_server` runs until
interrupted and is skipped by default; start it by hand if you need it.

You can also run a single binary directly, which is often quicker when working
on one area:

```shell
./test/test_ringbuf
./test/test_ringbuf --test-case="*mirrored*"
```

## Sanitizer and soak runs

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
