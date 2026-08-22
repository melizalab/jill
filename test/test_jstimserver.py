# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Drives jstimserver through its control protocol.

These tests pin what the server does *now*, so that the batch of protocol
fixes still to come can be seen to change exactly what it intends to and
nothing else. See doc/jstimserver-protocol.md.

Known defects are written as the behaviour the specification calls for and
marked strict xfail, following test_module_lifecycle.py. That way the test
says what should happen rather than enshrining what does, and fixing the
server makes this suite complain until the marker is removed. Three of them
kill the server outright, so each test gets its own instance.

**This file is intermittently red, and that is the server's fault rather than
the suite's.** Defect 1 -- an xrun with nothing playing dereferences a null
stimulus -- does not need the deliberate period-size change that
test_xrun_while_idle_does_not_crash uses to provoke it. Starting and stopping
JACK clients around an idle server is enough to produce a real xrun, and the
server then dies mid-test, roughly one run in eight here. The fixture reports
that as "jstimserver exited: rc=-11 (SIGSEGV)" rather than as a bare timeout,
so it is recognisable when it happens. It will stop once defect 1 is fixed,
which is the main argument for fixing that one ahead of the others.
"""

import json
import math
import os
import pathlib
import shutil
import signal
import struct
import subprocess
import time
import wave

import pytest

pytest.importorskip("zmq", reason="the jstimserver protocol tests need pyzmq")

from conftest import TEST_DIR, sanitizer_env  # noqa: E402
from jstimserver_client import JstimserverClient, Timeout, parse_event  # noqa: E402

MODULE = TEST_DIR.parent / "modules" / "jstimserver"

pytestmark = pytest.mark.needs_jack

SAMPLERATE = 44100

# Long enough that a request issued after playback starts is comfortably
# inside it, short enough not to pad the suite.
LONG_SECONDS = 2.0
SHORT_SECONDS = 0.2


def write_tone(path, seconds, freq=440.0):
    nframes = int(seconds * SAMPLERATE)
    with wave.open(str(path), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SAMPLERATE)
        w.writeframes(b"".join(
            struct.pack("<h", int(16000 * math.sin(2 * math.pi * freq * i / SAMPLERATE)))
            for i in range(nframes)))
    return str(path)


@pytest.fixture(scope="session")
def stimuli(tmp_path_factory):
    """A handful of stimulus files, including one whose name has a space."""
    d = tmp_path_factory.mktemp("jstimserver_stims")
    return {
        "short": write_tone(d / "short.wav", SHORT_SECONDS),
        "long": write_tone(d / "long.wav", LONG_SECONDS, freq=220.0),
        "two words": write_tone(d / "two words.wav", SHORT_SECONDS, freq=880.0),
    }


class Server:
    """A running jstimserver and a client connected to it."""

    def __init__(self, proc, client, logfile):
        self.proc = proc
        self.client = client
        self.logfile = logfile

    def returncode(self):
        return self.proc.poll()

    def wait_for_exit(self, timeout=5.0):
        """Wait for the server to exit; returns the code, or None if it did not."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rc = self.proc.poll()
            if rc is not None:
                return rc
            time.sleep(0.05)
        return None

    def log(self):
        try:
            return self.logfile.read_text()
        except OSError:
            return ""


@pytest.fixture
def start_server(tmp_path, jack_server):
    """Factory for jstimserver instances, one endpoint namespace per server.

    Each gets a distinct JACK client name so that concurrent runs, and any
    jstimserver the developer happens to have running, cannot collide on the
    ipc paths.
    """
    started = []
    counter = [0]

    def _start(stims, server_name="default", timeout=15.0):
        if not MODULE.exists():
            pytest.skip("jstimserver was not built")
        counter[0] += 1
        name = "jstimtest_%d_%d" % (os.getpid(), counter[0])
        logfile = tmp_path / ("%s.log" % name)
        proc = subprocess.Popen(
            [str(MODULE), "--name", name, "--server", server_name, *stims],
            stdout=logfile.open("w"), stderr=subprocess.STDOUT,
            env=sanitizer_env())
        socket_dir = pathlib.Path("/tmp/org.meliza.jill") / server_name / name
        endpoint = "ipc://%s" % socket_dir

        def alive_or_fail(what, cause=None):
            """Fail with the server's fate rather than with a bare timeout.

            A request that goes unanswered almost always means the server is
            gone, and until defect 1 is fixed it can go at any moment: an xrun
            while nothing is playing kills it, and starting and stopping JACK
            clients around it is enough to provoke one. Saying so here is the
            difference between a puzzling flake and a named defect.
            """
            rc = proc.poll()
            if rc is not None:
                signalled = " (%s)" % signal.Signals(-rc).name if rc < 0 else ""
                pytest.fail("jstimserver exited %s: rc=%d%s%s\n%s"
                            % (what, rc, signalled,
                               "\n  after: %s" % cause if cause else "",
                               logfile.read_text()))

        # Wait for both endpoints to exist *before* connecting, rather than
        # probing with requests. ØMQ happily connects to an unbound endpoint
        # and queues whatever is sent, so probing would leave one queued
        # request per attempt, all of them answered at once on bind -- after
        # which every reply belongs to an earlier request.
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            alive_or_fail("during startup")
            if (socket_dir / "req").exists() and (socket_dir / "pub").exists():
                break
            time.sleep(0.05)
        else:
            pytest.fail("jstimserver did not bind its endpoints within %gs\n%s"
                        % (timeout, logfile.read_text()))

        client = JstimserverClient(
            endpoint,
            on_timeout=lambda msg: alive_or_fail("while a client was waiting",
                                                 cause=msg))
        server = Server(proc, client, logfile)
        started.append(server)
        alive_or_fail("after binding")
        try:
            client.version(timeout=5.0)
        except Timeout as e:
            alive_or_fail("before answering VERSION", cause=e)
            raise

        # Confirm the subscription has actually reached the publisher, rather
        # than sleeping and hoping: zmq propagates subscriptions asynchronously
        # and anything published before ours arrives is simply lost. INTERRUPT
        # is the cheapest request with a visible effect and no side effect when
        # nothing is playing.
        for _ in range(40):
            alive_or_fail("while waiting for its event socket")
            try:
                reply = client.interrupt(timeout=5.0)
            except Timeout as e:
                # a missing reply is never something to retry through: doing so
                # would leave the unanswered request to be delivered later and
                # put every subsequent reply one behind
                alive_or_fail("before answering INTERRUPT", cause=e)
                raise
            assert reply == "OK"
            try:
                client.events_until("NOTPLAYING", timeout=0.25)
                break
            except Timeout:
                continue
        else:
            pytest.fail("no events arrived from jstimserver\n%s" % logfile.read_text())

        client.drain_events(settle=0.1)
        client.drain_replies(settle=0.1)
        return server

    yield _start

    for server in started:
        server.client.close()
        if server.proc.poll() is None:
            server.proc.send_signal(signal.SIGINT)
            try:
                server.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                server.proc.kill()
                server.proc.wait()


@pytest.fixture
def server(start_server, stimuli):
    """A server holding all three stimuli."""
    return start_server(list(stimuli.values()))


# --------------------------------------------------------------------------
# The parts that work
# --------------------------------------------------------------------------

def test_version_is_a_version(server):
    version = server.client.version()
    assert version, "VERSION returned nothing"
    # not pinned to a value: this changes every release
    assert version[0].isdigit(), "VERSION returned %r" % version


def test_stimlist_reports_every_stimulus(server, stimuli):
    stims = server.client.stimlist()
    assert sorted(s["name"] for s in stims) == sorted(stimuli)
    for stim in stims:
        expected = LONG_SECONDS if stim["name"] == "long" else SHORT_SECONDS
        assert stim["duration"] == pytest.approx(expected, abs=0.01)


def test_stimlist_serializes_duration_as_a_string(server):
    """Pins the encoding, not just the value.

    A client that assumes ordinary JSON gets a str where it wants a float, so
    the protocol document calls this out and the reference client converts.
    """
    raw = json.loads(server.client.request("STIMLIST"))
    assert all(isinstance(s["duration"], str) for s in raw["stimuli"])


def test_play_runs_to_completion(server):
    assert server.client.play("short") == "OK"
    verb, name, start = parse_event(server.client.next_event())
    assert (verb, name) == ("PLAYING", "short")
    verb, name, end = parse_event(
        server.client.events_until("DONE", timeout=SHORT_SECONDS + 2)[-1])
    assert (verb, name) == ("DONE", "short")
    # frame counts advance by roughly the stimulus length
    assert end - start == pytest.approx(SHORT_SECONDS * SAMPLERATE, rel=0.1)


def test_play_unknown_stimulus_is_refused(server):
    assert server.client.play("no_such_stimulus") == "BADSTIM"


def test_interrupt_while_idle(server):
    assert server.client.interrupt() == "OK"
    assert server.client.next_event() == "NOTPLAYING"


def test_interrupt_during_playback(server):
    assert server.client.play("long") == "OK"
    assert parse_event(server.client.next_event())[:2] == ("PLAYING", "long")
    assert server.client.interrupt() == "OK"
    verb, name, _ = parse_event(server.client.next_event())
    assert (verb, name) == ("INTERRUPTED", "long")


def test_play_while_playing_is_accepted_then_refused(server):
    """The protocol's central trap, pinned.

    OK means the request was taken, not that it succeeded. The second PLAY is
    answered OK on the request channel and BUSY on the event channel, and a
    client that stopped reading at OK would count a trial that never ran.
    """
    assert server.client.play("long") == "OK"
    assert parse_event(server.client.next_event())[:2] == ("PLAYING", "long")

    assert server.client.play("short") == "OK", "the second PLAY should be accepted"
    assert server.client.next_event() == "BUSY", "and then refused on the event channel"

    # the first stimulus is unaffected and still finishes
    verb, name, _ = parse_event(
        server.client.events_until("DONE", timeout=LONG_SECONDS + 2)[-1])
    assert (verb, name) == ("DONE", "long")


def test_unknown_request_is_refused(server):
    assert server.client.request("NOT_A_COMMAND") == "BADCMD"


def test_stimulus_names_may_contain_spaces(server):
    """Pins the rule that events must be parsed from the right.

    Names are file basenames, so they can contain spaces; splitting an event on
    whitespace would put the frame count in the wrong field.
    """
    assert server.client.play("two words") == "OK"
    verb, name, frame = parse_event(server.client.next_event())
    assert (verb, name) == ("PLAYING", "two words")
    assert isinstance(frame, int)


# --------------------------------------------------------------------------
# Known defects. Each asserts what the specification calls for.
# --------------------------------------------------------------------------

@pytest.mark.xfail(strict=True, reason="defect 3: BUSY is returned instead of BADCMD "
                                       "when a request is still pending")
def test_unknown_request_is_refused_even_with_a_request_pending(server):
    """The busy check runs before the server tries to recognise the command.

    So the same input gets BADCMD or BUSY depending on whether the realtime
    thread has consumed the previous request, and a client cannot tell a
    malformed request from a transient one.
    """
    assert server.client.interrupt() == "OK"
    assert server.client.request("NOT_A_COMMAND") == "BADCMD"


@pytest.mark.xfail(strict=True, reason="defect 4: STIMLIST advertises both files, "
                                       "only the first is playable")
def test_duplicate_basenames_are_not_advertised_twice(start_server, tmp_path):
    """Two files with the same stem collapse to one name.

    The list reports both with their true durations while only the first is
    loaded, so a client timing against the second is silently wrong.
    """
    a = tmp_path / "a"
    b = tmp_path / "b"
    a.mkdir()
    b.mkdir()
    server = start_server([write_tone(a / "dup.wav", SHORT_SECONDS),
                           write_tone(b / "dup.wav", LONG_SECONDS)])
    names = [s["name"] for s in server.client.stimlist()]
    assert len(names) == len(set(names)), "STIMLIST reported %r" % (names,)


@pytest.mark.xfail(strict=True, reason="defect 2: PLAY with no name aborts the server")
def test_play_with_no_name_is_refused(server):
    """`PLAY` alone matches the dispatch, and substr() then throws.

    The exception unwinds past a joinable monitor_thread, so the process
    aborts before the handler runs and the client never gets a reply.
    """
    assert server.client.request("PLAY", timeout=3.0) == "BADCMD"
    assert server.returncode() is None, "the server should still be running"


@pytest.mark.xfail(strict=True, reason="defect 1: an xrun with nothing playing "
                                       "dereferences a null stimulus")
def test_xrun_while_idle_does_not_crash(private_jack, start_server, stimuli):
    """The most serious of the four: no client involvement needed.

    process() pushes the xrun event with whatever _stim holds, which is null
    between trials, and the publisher formats it as stim->name(). Any period
    size change while idle is enough, and so is any real xrun.

    Runs against its own JACK server because changing the period size would
    disturb every other test sharing one.
    """
    server = start_server([stimuli["short"]], server_name=private_jack)
    subprocess.run(["jack_bufsize", "512"],
                   env={**os.environ, "JACK_DEFAULT_SERVER": private_jack},
                   capture_output=True, timeout=10)
    rc = server.wait_for_exit(timeout=3.0)
    assert rc is None, (
        "the server exited with rc=%s after a period size change\n%s"
        % (rc, server.log()))
    # and it should still be answering
    assert server.client.version(timeout=2.0)


@pytest.fixture
def private_jack():
    """A JACK server of our own, for tests that disturb the period size."""
    if shutil.which("jackd") is None:
        pytest.fail("jackd is not installed")
    name = "jilltest_%d" % os.getpid()
    proc = subprocess.Popen(
        ["jackd", "-n", name, "-r", "-d", "dummy", "-r", str(SAMPLERATE), "-p", "1024"],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    env = {**os.environ, "JACK_DEFAULT_SERVER": name}
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        if proc.poll() is not None:
            pytest.fail("private jackd exited: %s" % proc.communicate()[0])
        if subprocess.run(["jack_lsp"], env=env,
                          capture_output=True).returncode == 0:
            break
        time.sleep(0.1)
    else:
        proc.kill()
        pytest.fail("private jackd did not come up")
    yield name
    proc.terminate()
    try:
        proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
