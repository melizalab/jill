# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Shared fixtures for the JILL test driver.

The compiled suites are run as subprocesses rather than linked into pytest, so
that a suite which crashes or hangs takes only itself down.
"""

import os
import shutil
import socket
import subprocess
import time
from pathlib import Path

import pytest

# a standalone client script for exercising jstimserver by hand. It has no
# pytest-style tests and imports anyio at module scope, so collecting it would
# only produce an error.
collect_ignore = ["test_jstimserver_client.py"]

TEST_DIR = Path(__file__).parent


def pytest_addoption(parser):
    parser.addoption(
        "--soak",
        type=int,
        default=1,
        metavar="N",
        help="run each compiled suite N times. Races and leaks rarely show up "
        "on a single pass, so this is how the suites are used under "
        "-fsanitize=thread or =address.",
    )


def binary(name):
    """Path to a compiled test binary."""
    return TEST_DIR / name


def sanitizer_env():
    """Environment with the LeakSanitizer suppressions applied.

    Third-party libraries -- HDF5 in particular -- allocate process-global
    state on first use and never free it, which LeakSanitizer reports on every
    run. Without suppressing those the real findings are lost in the noise.
    Setting these is harmless in a build without sanitizers.
    """
    env = dict(os.environ)
    # RealtimeSanitizer's reports go through llvm-symbolizer, which hangs on
    # some systems and would wedge a suite rather than fail it. Anything the
    # caller already set wins, so `RTSAN_OPTIONS=symbolize=0 pytest` is how to
    # work around that; see doc/testing-notes.md.
    env.setdefault("RTSAN_OPTIONS", "halt_on_error=true")
    suppressions = TEST_DIR / "lsan.supp"
    if suppressions.exists():
        option = "suppressions=%s" % suppressions
        existing = env.get("LSAN_OPTIONS")
        env["LSAN_OPTIONS"] = option if not existing else existing + ":" + option
    return env


# Every sanitizer announces a finding with a line of this shape before it does
# anything else. Matching the banner rather than the exit status matters for the
# module tests, which run programs whose error paths legitimately exit non-zero:
# RealtimeSanitizer exits 43, ThreadSanitizer 66, and both would otherwise read
# as "the module reported an error and shut down properly".
SANITIZER_BANNER = "ERROR: "
SANITIZERS = (
    "AddressSanitizer",
    "ThreadSanitizer",
    "LeakSanitizer",
    "MemorySanitizer",
    "UndefinedBehaviorSanitizer",
    "RealtimeSanitizer",
)


def assert_no_sanitizer_error(out, name):
    """Fail if a sanitizer reported anything in a program's output.

    A build without sanitizers never emits these lines, so this is a no-op
    there and does not need to know how the tree was built.
    """
    if out is None:
        return
    for line in out.splitlines():
        if SANITIZER_BANNER in line and any(s in line for s in SANITIZERS):
            pytest.fail(
                "%s tripped a sanitizer:\n  %s\n--- output ---\n%s"
                % (name, line.strip(), out[-3000:]))


def run_binary(name, timeout=60, cwd=None, args=()):
    """Run a compiled suite, returning the CompletedProcess.

    Fails the calling test if the binary is missing, rather than skipping:
    a driver that quietly runs nothing is worse than one that complains.
    """
    path = binary(name)
    if not path.exists():
        pytest.fail(
            "%s has not been built. Run 'scons test' first." % path.name
        )
    return subprocess.run(
        [str(path), *args],
        cwd=str(cwd) if cwd else None,
        capture_output=True,
        text=True,
        timeout=timeout,
        env=sanitizer_env(),
    )


@pytest.fixture(scope="session")
def jack_server():
    """A JACK server on the dummy backend, for suites that need one.

    The dummy driver needs no sound hardware, which is what makes these
    runnable in CI. A missing or unstartable jackd is a failure, not a skip:
    JACK is a hard dependency of JILL, so anyone running the suites has it.
    Platforms where a server is not wanted deselect these with -m instead.
    """
    if shutil.which("jackd") is None:
        pytest.fail("jackd is not installed")

    proc = subprocess.Popen(
        ["jackd", "-r", "-d", "dummy", "-r", "44100", "-p", "1024"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    # give the server a moment to bind before anything tries to connect
    deadline = time.monotonic() + 10.0
    ready = False
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            break
        if _jack_responding():
            ready = True
            break
        time.sleep(0.1)

    if not ready:
        proc.terminate()
        try:
            output = proc.communicate(timeout=5)[0]
        except subprocess.TimeoutExpired:
            proc.kill()
            output = ""
        pytest.fail("could not start jackd -d dummy:\n%s" % output)

    yield proc

    proc.terminate()
    try:
        proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()


def _jack_responding():
    """Whether a JACK server is up, using jack_lsp if it is available."""
    lsp = shutil.which("jack_lsp")
    if lsp is None:
        # no way to ask; assume the short sleep above was enough
        return True
    try:
        return subprocess.run(
            [lsp], capture_output=True, timeout=5
        ).returncode == 0
    except subprocess.TimeoutExpired:
        return False


@pytest.fixture
def free_ipc_endpoint(tmp_path):
    """A filesystem path usable as a zmq ipc endpoint."""
    return "ipc://%s" % (tmp_path / "msg")


def _unused_tcp_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]
