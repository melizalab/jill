# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Shared fixtures for the JILL test driver.

The compiled suites are run as subprocesses rather than linked into pytest, so
that a suite which crashes or hangs takes only itself down.
"""

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


def binary(name):
    """Path to a compiled test binary."""
    return TEST_DIR / name


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
