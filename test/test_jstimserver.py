# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Drives jstimserver through its control protocol.

These tests pin what the server does *now*, so that the batch of protocol
fixes still to come can be seen to change exactly what it intends to and
nothing else. See doc/jstimserver-protocol.md.

Known defects are written as the behaviour the specification calls for and
marked strict xfail, following test_module_lifecycle.py. That way the test
says what should happen rather than enshrining what does, and fixing the
server makes this suite complain until the marker is removed. Each test gets
its own server instance, because a defect that kills it must not take the
rest of the file with it.

Worth knowing if this file starts failing intermittently again: an
unanswered request usually means the server has died, so the fixture reports
its exit status rather than a bare timeout. That is how defect 1 was found to
fire without the deliberate period-size change used to provoke it -- ordinary
JACK client churn produced real xruns, and an idle server segfaulted on them
about one run in eight until it was fixed.
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

#: The protocol version the server should report. Bump deliberately, and
#: only alongside doc/jstimserver-protocol.md.
PROTOCOL_VERSION = "1.0"

SAMPLERATE = 44100

# Long enough that a request issued after playback starts is comfortably
# inside it, short enough not to pad the suite.
LONG_SECONDS = 2.0
SHORT_SECONDS = 0.2

# generous enough for startup on a loaded runner, short enough that a hang
# is obvious rather than a wait
BUDGET = 30


def break_the_stream(server_name, period):
    """Put a gap in the audio stream by changing the JACK period size.

    A real xrun cannot be provoked on demand; this reaches the same code path,
    since jstimserver routes the buffer size callback through its xrun counter.
    """
    subprocess.run(["jack_bufsize", str(period)],
                   env={**os.environ, "JACK_DEFAULT_SERVER": server_name},
                   capture_output=True, timeout=10)


def expect_completion(client, name, timeout):
    """Wait for `name` to finish playing, and return the frame it ended on.

    Skips rather than fails if the stream breaks first. A genuine xrun
    truncates playback and publishes XRUN and INTERRUPTED instead of DONE,
    which is what section 4.3 of the protocol requires, so any assertion about
    a stimulus completing is really an assertion that the audio stream stayed
    clean. The dummy backend under a loaded test run does not promise that, and
    the trial is legitimately void when it happens.
    """
    seen = []
    while True:
        event = client.next_event(timeout)
        seen.append(event)
        verb, event_name, frame = parse_event(event)
        if verb == "XRUN":
            pytest.skip("the stream broke during playback (%r); the trial is "
                        "void, see protocol section 4.3" % (seen,))
        if verb == "DONE" and event_name == name:
            return frame


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

def test_version_reports_the_protocol_version(server):
    """VERSION reports the protocol, not the JILL release.

    Pinned to a value, unlike the release version it replaced: this changes
    only when the protocol does, and a client keys its behaviour off it.
    """
    assert server.client.version() == PROTOCOL_VERSION


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
    end = expect_completion(server.client, "short", timeout=SHORT_SECONDS + 2)
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
    expect_completion(server.client, "long", timeout=LONG_SECONDS + 2)


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

def test_unknown_request_is_refused_even_with_a_request_pending(server):
    """The busy check used to run before the command was recognised.

    So the same input got BADCMD or BUSY depending on whether the realtime
    thread had consumed the previous request, and a client could not tell a
    malformed request from a transient one.
    """
    assert server.client.interrupt() == "OK"
    assert server.client.request("NOT_A_COMMAND") == "BADCMD"


def test_duplicate_basenames_are_refused(tmp_path, jack_server):
    """Two files with the same stem collapse to one name, so the server refuses.

    STIMLIST used to report both with their true durations while only the first
    was loaded, which meant a client timing against the second was silently
    wrong. There is no honest list to serve here, so this fails at startup
    rather than during the experiment.

    Runs the binary directly: the start_server fixture waits for a server that
    is never going to bind.
    """
    a = tmp_path / "a"
    b = tmp_path / "b"
    a.mkdir()
    b.mkdir()
    if not MODULE.exists():
        pytest.skip("jstimserver was not built")
    proc = subprocess.run(
        [str(MODULE), "--name", "jstimtest_dup_%d" % os.getpid(),
         write_tone(a / "dup.wav", SHORT_SECONDS),
         write_tone(b / "dup.wav", LONG_SECONDS)],
        capture_output=True, text=True, timeout=BUDGET, env=sanitizer_env())
    assert proc.returncode != 0, (
        "jstimserver started with two stimuli called 'dup'\n%s" % proc.stdout)
    assert proc.returncode > 0, (
        "jstimserver was killed by signal %d rather than reporting the error"
        % -proc.returncode)
    assert "dup" in proc.stdout, (
        "the error does not name the offending stimulus\n%s" % proc.stdout)


def test_distinct_basenames_in_different_directories_are_fine(tmp_path, jack_server):
    """The check is on the name, not the path: same directory rule, different stems.

    Guards the fix against being written as "reject any repeated path element"
    or similar, which would break an ordinary playlist assembled from several
    directories.
    """
    a = tmp_path / "a"
    b = tmp_path / "b"
    a.mkdir()
    b.mkdir()
    if not MODULE.exists():
        pytest.skip("jstimserver was not built")
    name = "jstimtest_ok_%d" % os.getpid()
    proc = subprocess.Popen(
        [str(MODULE), "--name", name,
         write_tone(a / "one.wav", SHORT_SECONDS),
         write_tone(b / "two.wav", SHORT_SECONDS)],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        env=sanitizer_env())
    try:
        socket_dir = pathlib.Path("/tmp/org.meliza.jill/default") / name
        deadline = time.monotonic() + BUDGET
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                pytest.fail("jstimserver refused a valid playlist (rc=%d)\n%s"
                            % (proc.returncode, proc.communicate()[0]))
            if (socket_dir / "req").exists() and (socket_dir / "pub").exists():
                break
            time.sleep(0.05)
        else:
            pytest.fail("jstimserver did not start")
        with JstimserverClient("ipc://%s" % socket_dir) as client:
            assert sorted(s["name"] for s in client.stimlist()) == ["one", "two"]
    finally:
        proc.send_signal(signal.SIGINT)
        try:
            proc.communicate(timeout=10)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()


@pytest.mark.parametrize("request_text", ["PLAY", "PLAY "],
                         ids=["no-separator", "separator-but-no-name"])
def test_play_with_no_name_is_refused(server, request_text):
    """`PLAY` alone used to match the dispatch, and substr() then threw.

    The exception unwound past a joinable monitor_thread, so the process
    aborted before the handler ran and the client never got a reply. The
    trailing-space form reaches the dispatch legitimately and has to be
    rejected on its own account, since a stimulus name may not be empty.
    """
    assert server.client.request(request_text, timeout=3.0) == "BADCMD"
    assert server.returncode() is None, "the server should still be running"


def test_xrun_while_idle_does_not_crash(private_jack, start_server, stimuli):
    """The most serious of the four defects, now fixed.

    process() pushed the xrun event with whatever _stim held, which is null
    between trials, and the publisher formatted it as stim->name(). Any period
    size change while idle was enough, and so was any real xrun.

    Runs against its own JACK server because changing the period size would
    disturb every other test sharing one.
    """
    server = start_server([stimuli["short"]], server_name=private_jack)
    break_the_stream(private_jack, 512)
    rc = server.wait_for_exit(timeout=3.0)
    assert rc is None, (
        "the server exited with rc=%s after a period size change\n%s"
        % (rc, server.log()))
    # and it should still be answering
    assert server.client.version(timeout=2.0)
    # nothing to report: an XRUN event names the stimulus it ruined, and there
    # was none. Section 4.3 of the protocol says publish nothing.
    assert server.client.drain_events(settle=0.5) == []


def test_xrun_during_playback_is_reported(private_jack, start_server, stimuli):
    """The other half of the rule: a break that ruins a trial must be reported.

    Guards the fix for defect 1 against being written as "never publish XRUN",
    which would silently turn a corrupted trial into one that looks fine.
    """
    server = start_server([stimuli["long"]], server_name=private_jack)
    assert server.client.play("long") == "OK"
    assert parse_event(server.client.next_event())[:2] == ("PLAYING", "long")

    break_the_stream(private_jack, 256)
    seen = server.client.events_until("INTERRUPTED", timeout=5.0)
    verbs = [e.split(" ", 1)[0] for e in seen]
    assert "XRUN" in verbs, "the break was not reported: %r" % (seen,)
    # both name the stimulus they refer to
    for event in seen:
        verb, name, _ = parse_event(event)
        if verb in ("XRUN", "INTERRUPTED"):
            assert name == "long"
    assert server.returncode() is None, "the server died\n%s" % server.log()


@pytest.fixture
def private_jack():
    """A JACK server of our own, for tests that disturb the period size.

    The name is fixed rather than derived from the pid. jackd2 keeps a registry
    in /dev/shm with room for only a handful of servers, and an entry survives a
    server that was killed rather than asked to stop -- so a per-run name burns
    a slot on every interrupted run until startup fails with an opaque
    "jack_get_descriptor : dll" and nothing suggests why. A fixed name reuses
    one slot forever. This does mean two copies of the suite cannot run at once,
    which is already true of the shared jack_server fixture.
    """
    if shutil.which("jackd") is None:
        pytest.fail("jackd is not installed")
    name = "jilltest"
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
        pytest.fail("private jackd did not come up. If it reported "
                    "'jack_get_descriptor', the /dev/shm registry is full of "
                    "entries from servers that were killed rather than stopped; "
                    "remove /dev/shm/jack* with no jackd running.")
    yield name
    proc.terminate()
    try:
        proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
