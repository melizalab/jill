# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Takes each module down its startup, error and shutdown paths.

The modules keep the objects their JACK callbacks need at file scope, which
means those objects outlive main() and are destroyed during static destruction
-- after the JACK client is gone, and after libraries like HDF5 have finalized
themselves. Every module tears down explicitly at the end of its try block, so
the happy path is fine; the exception paths skip it.

These tests exercise that. A module that hits an error after its globals are
initialized should still exit under its own control: not killed by a signal,
not hung. Anything marked xfail here is a known defect awaiting task 4 step 2,
and the marker is strict so that fixing it makes this suite complain until the
marker is removed.
"""

import os
import signal
import subprocess
import time
import wave
import struct
import math

import pytest

from conftest import TEST_DIR

MODULE_DIR = TEST_DIR.parent / "modules"

pytestmark = pytest.mark.needs_jack

# A budget generous enough for startup on a loaded CI runner, short enough that
# a hang is obvious.
BUDGET = 25


def module(name):
    path = MODULE_DIR / name
    if not path.exists():
        pytest.skip("%s was not built" % name)
    return str(path)


@pytest.fixture(scope="module")
def tone(tmp_path_factory):
    """A short wav file for the modules that need a stimulus."""
    path = tmp_path_factory.mktemp("lifecycle") / "tone.wav"
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(44100)
        w.writeframes(b"".join(
            struct.pack("<h", int(16000 * math.sin(2 * math.pi * 440 * i / 44100)))
            for i in range(8820)))
    return str(path)


def run_until_exit(args, budget=BUDGET):
    """Run a module, returning (returncode, seconds, output).

    returncode is negative when the process was killed by a signal, and None
    when it had to be killed for exceeding the budget.
    """
    start = time.monotonic()
    proc = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True)
    try:
        out = proc.communicate(timeout=budget)[0]
        return proc.returncode, time.monotonic() - start, out
    except subprocess.TimeoutExpired:
        proc.kill()
        out = proc.communicate()[0]
        return None, time.monotonic() - start, out


def assert_exited_under_its_own_control(rc, elapsed, out, name):
    if rc is None:
        pytest.fail(
            "%s did not exit within %ds; it hung rather than reporting the error"
            "\n--- output ---\n%s" % (name, BUDGET, out[-2000:]))
    if rc < 0:
        pytest.fail(
            "%s was killed by signal %d (%s) instead of exiting"
            "\n--- output ---\n%s"
            % (name, -rc, signal.Signals(-rc).name, out[-2000:]))
    # a non-zero status is expected here: these are error paths
    assert rc != 0, "%s reported success on what should be an error path" % name


# Each entry hits an error *after* the module has built the objects it keeps at
# file scope, which is the case that skips the explicit teardown.
ERROR_PATHS = [
    pytest.param("jrecord", ["-i", "no_such_port", "{tmp}/out.arf"], id="jrecord"),
    pytest.param("jdetect", ["-i", "no_such_port"], id="jdetect"),
    pytest.param("jrelay", ["-i", "no_such_port"], id="jrelay"),
    pytest.param("jclicker", ["-i", "no_such_port"], id="jclicker"),
    pytest.param(
        "jstim", ["-l", "-o", "no_such_port", "{tone}"], id="jstim",
        marks=pytest.mark.xfail(
            strict=True,
            reason="aborts on the error path: connect_ports throws, "
                   "stim_queue->join() is skipped, and the global's default "
                   "destructor runs ~std::thread on a joinable thread. "
                   "task 4 step 2"),
    ),
]


@pytest.mark.parametrize("name,args", ERROR_PATHS)
def test_error_path_exits_cleanly(name, args, jack_server, tone, tmp_path):
    """A module that fails after startup should still exit under its own control."""
    filled = [a.format(tmp=str(tmp_path), tone=tone) for a in args]
    rc, elapsed, out = run_until_exit([module(name), *filled])
    assert_exited_under_its_own_control(rc, elapsed, out, name)


# Modules that will run steadily under the dummy backend, for the shutdown path
SHUTDOWN_PATHS = [
    ("jnoise", []),
    ("jdetect", []),
    ("jrecord", ["-i", "system:capture_1", "{tmp}/out.arf"]),
    ("jstim", ["-l", "{tone}"]),
]


@pytest.mark.parametrize("name,args", SHUTDOWN_PATHS,
                         ids=[n for n, _ in SHUTDOWN_PATHS])
def test_sigint_shuts_down_cleanly(name, args, jack_server, tone, tmp_path):
    """SIGINT is the normal way these are stopped, so it has to work.

    Several signal handlers do more than set a flag -- some log, some take a
    mutex -- which is not async-signal-safe and can deadlock. This is the test
    that would catch it.
    """
    filled = [a.format(tmp=str(tmp_path), tone=tone) for a in args]
    proc = subprocess.Popen([module(name), *filled], stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True)
    time.sleep(3)                       # let it reach steady state
    if proc.poll() is not None:
        out = proc.communicate()[0]
        pytest.fail("%s exited before it could be signalled (rc=%s)\n%s"
                    % (name, proc.returncode, out[-2000:]))

    proc.send_signal(signal.SIGINT)
    try:
        out = proc.communicate(timeout=BUDGET)[0]
    except subprocess.TimeoutExpired:
        proc.kill()
        out = proc.communicate()[0]
        pytest.fail("%s did not exit within %ds of SIGINT\n--- output ---\n%s"
                    % (name, BUDGET, out[-2000:]))

    rc = proc.returncode
    # exiting under its own control is what matters; dying from the default
    # SIGINT disposition would mean the handler never ran
    if rc < 0:
        pytest.fail("%s died from signal %d (%s) rather than shutting down\n"
                    "--- output ---\n%s"
                    % (name, -rc, signal.Signals(-rc).name, out[-2000:]))
