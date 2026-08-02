# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Runs the compiled test suites and reports a single pass or fail.

Each doctest suite is one pytest case. They are separate processes, so a suite
that crashes or deadlocks fails on its own rather than taking the run with it,
and each gets its own timeout.

    pytest test                     # everything runnable unattended
    pytest test -m "not needs_jack" # skip anything wanting a JACK server
    pytest test -m manual           # only the ones needing a human
"""

import os

import pytest

from conftest import TEST_DIR, run_binary

# Suites built from doctest. These are self-contained: no server, no network,
# no fixture files. Keep in step with UNIT_SUITES in test/SConscript; the
# consistency check at the bottom of this file enforces that.
UNIT_SUITES = [
    "test_counter",
    "test_randomizer",
    "test_ringbuf",
    "test_stimset",
    "test_crossing",
    "test_midi",
    "test_util",
]

# Older programs that predate the harness. They mostly return 0 whatever
# happens, so running them is a smoke test rather than a real check, but a
# crash or a hang would still be caught.
JACK_PROGRAMS = ["test_xrun"]
ARF_PROGRAMS = ["test_arf_writer"]

# test_zmq_server binds a socket and then blocks in `while (!s_interrupted)`
# until it is signalled. It is a diagnostic log receiver, not a test, and can
# only ever be run by hand.
MANUAL_PROGRAMS = ["test_zmq_server"]


@pytest.mark.parametrize("suite", UNIT_SUITES)
def test_unit_suite(suite, request):
    """A doctest suite passes when it exits zero.

    With --soak=N the suite is run N times. A sanitizer or a race usually
    needs more than one pass to show itself, and the iteration is reported so
    an intermittent failure can be told from a consistent one.
    """
    runs = request.config.getoption("--soak")
    for i in range(runs):
        result = run_binary(suite, timeout=120)
        if result.returncode != 0:
            pytest.fail(
                "%s exited %d on run %d of %d\n\n"
                "--- stdout ---\n%s\n--- stderr ---\n%s"
                % (suite, result.returncode, i + 1, runs, result.stdout, result.stderr)
            )


@pytest.mark.needs_jack
@pytest.mark.parametrize("program", JACK_PROGRAMS)
def test_jack_program(program, jack_server):
    """A legacy program that needs a JACK server to connect to."""
    result = run_binary(program, timeout=60)
    assert result.returncode == 0, (
        "%s exited %d\n\n--- output ---\n%s%s"
        % (program, result.returncode, result.stdout, result.stderr)
    )


@pytest.mark.needs_arf
@pytest.mark.parametrize("program", ARF_PROGRAMS)
def test_arf_program(program, tmp_path):
    """A legacy program that writes an ARF file.

    Run in a temporary directory: it writes test.arf into the working
    directory, and would otherwise litter the source tree.
    """
    if not (TEST_DIR / program).exists():
        pytest.skip("%s was not built (scons --no-arf?)" % program)
    result = run_binary(program, timeout=60, cwd=tmp_path)
    assert result.returncode == 0, (
        "%s exited %d\n\n--- output ---\n%s%s"
        % (program, result.returncode, result.stdout, result.stderr)
    )
    assert (tmp_path / "test.arf").exists(), "no output file was written"


@pytest.mark.manual
@pytest.mark.parametrize("program", MANUAL_PROGRAMS)
def test_manual_program(program):
    """Placeholder recording that these cannot run unattended."""
    pytest.skip(
        "%s runs until interrupted; start it by hand when you need it" % program
    )


def test_declared_suites_match_what_was_built():
    """Catch drift between this file and test/SConscript.

    Every executable in test/ should be accounted for above. Without this,
    adding a suite to the SConscript and forgetting it here would leave it
    silently unrun.
    """
    declared = set(UNIT_SUITES + JACK_PROGRAMS + ARF_PROGRAMS + MANUAL_PROGRAMS)
    built = {
        entry.name
        for entry in TEST_DIR.iterdir()
        if entry.is_file()
        and os.access(entry, os.X_OK)
        and entry.suffix not in (".py", ".cc", ".ini", ".o")
    }
    unaccounted = built - declared
    assert not unaccounted, (
        "built but not declared in test_suites.py: %s" % sorted(unaccounted)
    )
