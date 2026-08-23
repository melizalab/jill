# Testing notes

How to build and run the test suites, and what the various build modes check.
The prerequisites differ by platform and are listed in
[Debian](debian-installation.md) and [macOS](osx-installation.md), but
everything here applies to both.

## Linting

`clang-tidy` is configured in `.clang-tidy`, with a check set aimed at
lifetimes, concurrency and the things that go wrong rarely.

An optional pre-commit hook runs it over the lines a commit touches:

```shell
git config core.hooksPath .githooks
```

Only the diff is linted, because there are a lot of old findings in the tree
that shouldn't block future work. Do `git commit --no-verify` to skip the lint,
and the hook skips itself when `clang-tidy` is not installed.

The hook covers `.cc` files only. Headers are absent from the compilation
database, so a commit touching only headers is not checked, only if a `.cc` that
includes it is linted.

To run it by hand over one file:

```shell
scons -Q compile_commands.json
clang-tidy -p . jill/dsp/buffered_data_writer.cc
```

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

Some tests require external resources, and are tagged so they can be left out:

```shell
pytest -m "not needs_jack"   # skip anything needing a JACK server
pytest -m "not needs_arf"    # skip the ARF/HDF5 format tests
```

The protocol tests in `test_jstimserver.py` additionally need `pyzmq`, since
they speak to the server over its ØMQ endpoints. It is `python3-zmq` on Debian
and `pip install pyzmq` elsewhere.

Tests that need a JACK server will fire up `jackd -d dummy` and shut it
afterwards, so no sound hardware is involved.

One caveat: If a run fails to start a server and jackd says
`jack_get_descriptor`, its registry in `/dev/shm` is full. jackd2 has room for
only a handful of servers there, and an entry outlives a server that was killed
rather than asked to stop. Interrupting runs or using `kill -9` will eventually
fill it. If jack is not running, you can safely do `rm /dev/shm/jack*` to clears
the backlog. JACK on macOS is less predictable than on Linux, so if `jackd`
cannot be started those tests skip rather than fail. `test_zmq_server` runs
until interrupted and is skipped by default; start it by hand if you need it.

You can also run single test binaries directly:

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

Always combine `sanitize=` with `debug=1`: a sanitizer to ensure that `NDEBUG`
doesn't compile out assertions. `--soak=N` runs each suite N times and reports
which iteration failed, so an intermittent problem can be told from a consistent
one.

LeakSanitizer suppressions live in `test/lsan.supp` and are applied
automatically by the driver. HDF5 registers process-global state on first use
and never frees it, which would otherwise be reported on every single run.

## Realtime sanitizer

JACK process callbacks have to finish in bounded time without blocking, which
rules out allocating, freeing, taking a lock, doing I/O, or making a syscall
that can sleep. Use `sanitize=realtime` to check this behavior:

```shell
scons -Q -c
scons -Q debug=1 sanitize=realtime modules test
pytest test/test_module_lifecycle.py
```

Only supported on clang, because it works through clang's `nonblocking`
attribute, which `JILL_RT` in `jill/rt.hh` expands to. The build switches the
compiler for you and refuses to run if `CXX` isn't pointing to clang,
rather than silently producing a binary that checks nothing. On gcc `JILL_RT`
still expands to `noexcept` to enforce what we can.

Any violation during the sanitizer run will abort the process with a stack trace
at the point of the call, and the driver fails the test on the report rather
than on the exit status. Note that some modules only reach critical code when
something drives it. `jclicker` and `jrelay` do their per-event work only when
events are arriving, so `test_event_driven_path_is_clean` runs each of them
alongside `jstim` and lets real stimulus markers flow between them.

If a run appears to hang instead of printing a report, it may be stuck in
`llvm-symbolizer`. Set `RTSAN_OPTIONS=symbolize=0` to get addresses instead of
symbols, which `llvm-symbolizer` can resolve afterwards from the build ID.
