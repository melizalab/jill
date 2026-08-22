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

The protocol tests in `test_jstimserver.py` additionally need `pyzmq`, since
they speak to the server over its ØMQ endpoints. It is `python3-zmq` on Debian
and `pip install pyzmq` elsewhere; without it that file skips and everything
else runs as normal.

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

`test_ringbuf_concurrent` exists specifically to drive the ringbuffers from two
threads. Nothing else does, so before it was written the most important race in
the codebase produced no sanitizer output at all. It reported seventeen races
under `sanitize=thread` until the read and write pointers became `std::atomic`;
it should now be clean, and a report from it is a regression.

## Checking the realtime callbacks

A JACK process callback has to finish in bounded time without blocking, which
rules out allocating, freeing, taking a lock, doing I/O, or making a syscall
that can sleep. `sanitize=realtime` checks that instead of trusting it:

```shell
scons -Q -c
scons -Q debug=1 sanitize=realtime modules test
pytest test/test_module_lifecycle.py
```

This one is clang-only, because it works through clang's `nonblocking`
attribute, which `JILL_RT` in `jill/rt.hh` expands to. The build switches the
compiler for you and refuses to run if `CXX` names something that is not clang,
rather than silently producing a binary that checks nothing. On gcc `JILL_RT`
still expands to `noexcept`, so half the contract is enforced everywhere:
throwing allocates, and an exception unwinding out of a callback would cross
into JACK's C frames.

Marking a callback does not make it safe; it asks to be told when it is not.
Any violation aborts the process with a stack trace at the point of the call,
and the driver fails the test on the report rather than on the exit status --
these modules exit non-zero on their error paths anyway, so the status alone
cannot tell a handled error from an abort inside a callback.

Note that a module only reaches its interesting code when something drives it.
`jclicker` and `jrelay` do their per-event work only when events are arriving,
so `test_event_driven_path_is_clean` runs each of them alongside `jstim` and
lets real stimulus markers flow between them.

If a run appears to hang instead of printing a report, it is stuck in
`llvm-symbolizer`. Set `RTSAN_OPTIONS=symbolize=0` to get addresses instead of
symbols, which `llvm-symbolizer` can resolve afterwards from the build ID.
