# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""A client for the jstimserver control protocol.

See doc/jstimserver-protocol.md. This is the reference implementation of the
client side: the test suite drives jstimserver through it, and it is small
enough to copy into an experiment script.

Two choices are worth explaining.

The request socket is a DEALER rather than a REQ. REQ enforces a strict
send/recv alternation and will not let you send again until it has a reply, so
a server that dies without answering leaves the socket permanently unusable and
the failure surfaces later, somewhere else. DEALER lets a missing reply be
observed where it happens, which matters here because two of the protocol's
known defects are exactly that.

Nothing blocks without a deadline. Every receive goes through a poller, so a
server that has crashed or wedged produces a timeout that a test can assert on
rather than a hang that has to be killed from outside.
"""

import json

import zmq

#: How long to wait for a reply or an event before giving up.
DEFAULT_TIMEOUT = 2.0


class Timeout(Exception):
    """No reply or event arrived within the deadline."""


class JstimserverClient:
    """Talks to one jstimserver instance.

    :param endpoint_dir: the ipc directory the server binds under, i.e.
        ``ipc:///tmp/org.meliza.jill/SERVER_NAME/CLIENT_NAME``. The ``req`` and
        ``pub`` endpoints are formed from it.
    """

    def __init__(self, endpoint_dir, timeout=DEFAULT_TIMEOUT, context=None,
                 on_timeout=None):
        self.timeout = timeout
        #: Called with the message before a Timeout is raised. The test suite
        #: uses it to report that the server process has died, which is the
        #: usual reason for a request or event to go missing and is far more
        #: use than the timeout on its own.
        self.on_timeout = on_timeout
        self._owns_context = context is None
        self._ctx = context or zmq.Context()

        self._req = self._ctx.socket(zmq.DEALER)
        # do not let close() block on undelivered messages
        self._req.setsockopt(zmq.LINGER, 0)
        self._req.connect("%s/req" % endpoint_dir)

        self._sub = self._ctx.socket(zmq.SUB)
        self._sub.setsockopt(zmq.LINGER, 0)
        # the server publishes no topic prefix, so subscribe to everything
        self._sub.setsockopt(zmq.SUBSCRIBE, b"")
        self._sub.connect("%s/pub" % endpoint_dir)

    # -- lifecycle ---------------------------------------------------------

    def _timed_out(self, message):
        """Raise Timeout, giving the owner a chance to explain it first."""
        if self.on_timeout is not None:
            self.on_timeout(message)
        raise Timeout(message)

    def close(self):
        self._req.close()
        self._sub.close()
        if self._owns_context:
            self._ctx.term()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    # -- requests ----------------------------------------------------------

    def request(self, text, timeout=None):
        """Send a raw request and return the reply.

        Takes the request as text rather than as arguments so that a caller can
        send something malformed, which is most of what the protocol's error
        handling needs to be tested with.

        :raises Timeout: if no reply arrives.
        """
        self._req.send_string(text)
        if not self._req.poll(1000 * (self.timeout if timeout is None else timeout)):
            self._timed_out("no reply to %r" % text)
        return self._req.recv_string()

    def drain_replies(self, settle=0.1):
        """Discard replies already queued, and return them.

        Needed because ØMQ lets a socket connect to an endpoint nothing has
        bound yet and queues what is sent meanwhile. A caller that probes a
        starting server by sending requests will have every one of them
        answered at once when it finally binds, after which each request
        returns the *previous* request's reply until the backlog drains. Call
        this once the server is known to be up.
        """
        seen = []
        while self._req.poll(1000 * settle):
            seen.append(self._req.recv_string())
        return seen

    def version(self, **kw):
        return self.request("VERSION", **kw)

    def stimlist(self, **kw):
        """The stimulus set, as a list of dicts.

        Durations are converted to float here: the server serializes them as
        JSON *strings*, because its writer does not track types.
        """
        reply = self.request("STIMLIST", **kw)
        stimuli = json.loads(reply)["stimuli"]
        for stim in stimuli:
            stim["duration"] = float(stim["duration"])
        return stimuli

    def play(self, name, **kw):
        return self.request("PLAY %s" % name, **kw)

    def interrupt(self, **kw):
        return self.request("INTERRUPT", **kw)

    # -- events ------------------------------------------------------------

    def next_event(self, timeout=None):
        """The next published event.

        :raises Timeout: if none arrives.
        """
        if not self._sub.poll(1000 * (self.timeout if timeout is None else timeout)):
            self._timed_out("no event")
        return self._sub.recv_string()

    def events_until(self, verb, timeout=None):
        """Collect events up to and including the first one starting with `verb`.

        Returns the list. Useful because several requests produce a couple of
        events whose order is not always fixed, and a test usually cares that a
        particular one arrived rather than exactly when.

        :raises Timeout: if `verb` never arrives.
        """
        seen = []
        while True:
            try:
                event = self.next_event(timeout)
            except Timeout:
                # what did arrive is the whole diagnosis when this fails
                self._timed_out("no %s; saw %r" % (verb, seen))
            seen.append(event)
            if event.split(" ", 1)[0] == verb:
                return seen

    def drain_events(self, settle=0.2):
        """Discard anything already published, and return it.

        Call between trials so that one test's events are not mistaken for the
        next one's.
        """
        seen = []
        while self._sub.poll(1000 * settle):
            seen.append(self._sub.recv_string())
        return seen


def parse_event(event):
    """Split an event into (verb, name, frame).

    `name` and `frame` are None for the events that carry no arguments.

    Splits the name from the right because a stimulus name is a file basename
    and so may contain spaces; ``event.split()`` mis-parses those.
    """
    verb, _, rest = event.partition(" ")
    if not rest:
        return verb, None, None
    name, _, frame = rest.rpartition(" ")
    return verb, name, int(frame)
