# jstimserver control protocol

**Status:** draft. **Version:** 1.0. Applies to JILL 2.2.x.

`jstimserver` presents stimuli on demand rather than from a fixed playlist. A
client asks it to play a named stimulus and is told, asynchronously, what
happened. This document defines that exchange.

The key words MUST, MUST NOT, REQUIRED, SHOULD, SHOULD NOT, RECOMMENDED, MAY and
OPTIONAL are to be interpreted as described in
[RFC 2119](http://tools.ietf.org/html/rfc2119).

This protocol is specific to `jstimserver`. The general conventions it builds on
— endpoint naming and socket types for control interfaces — belong to the
[JILL specification](specification.org). Note that `jillctl.abnf` in this
directory describes an unrelated binary protocol that is not implemented by any
module and has no bearing on this one.

## 1. Overview

`jstimserver` exposes two ØMQ endpoints:

| Purpose | Server socket | Client socket | Direction |
|---|---|---|---|
| Requests | `ROUTER` | `REQ` or `DEALER` | client → server, with a reply |
| Events | `PUB` | `SUB` | server → clients, unsolicited |

The split is not decorative. A reply says only that a request was **accepted**;
what actually happened to the audio is reported on the event channel, because
the answer is not known until the realtime thread acts on the request — which
may be several milliseconds later, and may be "no".

> **The single most important rule in this document.** `OK` means *the server
> took your request*, not *the stimulus played*. A `PLAY` issued while another
> stimulus is playing is answered `OK` on the request channel and `BUSY` on the
> event channel. A client that treats `OK` as success will silently believe in
> trials that never happened.

## 2. Transport

### 2.1 Endpoints

Both endpoints are ØMQ interprocess sockets under a directory derived from the
JACK server name and the client name:

```
ipc:///tmp/org.meliza.jill/SERVER_NAME/CLIENT_NAME/req
ipc:///tmp/org.meliza.jill/SERVER_NAME/CLIENT_NAME/pub
```

`SERVER_NAME` is the JACK server (`--server`, default `default`) and
`CLIENT_NAME` is the JACK client name (`--name`, default `jstimserver`). The
server creates the directory if it does not exist, and MUST bind `req` before
it begins accepting requests.

### 2.2 Framing and encoding

Every request, reply and event is a single UTF-8 string. There is no binary
framing, no length prefix and no terminator.

Requests are carried in the **last frame** of the ØMQ message; the server
replies with the frames it received, with only that last frame replaced. This
is the standard `ROUTER` idiom, and it means the routing prefix is preserved
without the server having to understand it. Both `REQ` (which inserts an empty
delimiter frame) and `DEALER` (which does not) therefore work unmodified, and a
client MAY use either.

Events are single-frame messages with no routing prefix. Clients MUST subscribe
to the empty prefix (`SUBSCRIBE ""`) — the server does not use ØMQ topic
prefixes, so a non-empty subscription silently matches on the message text.

## 3. Requests and replies

| Request | Reply on success | Other replies |
|---|---|---|
| `VERSION` | JILL version string, e.g. `2.2.2` | — |
| `STIMLIST` | JSON object, see §5 | — |
| `PLAY <name>` | `OK` | `BADSTIM`, `BUSY` |
| `INTERRUPT` | `OK` | `BUSY` |
| anything else | — | `BADCMD` |

Every request MUST receive exactly one reply. The reply tokens are:

| Reply | Meaning |
|---|---|
| `OK` | The request was accepted and handed to the realtime thread. Watch the event channel for the outcome. |
| `BADCMD` | The request was not recognised. |
| `BADSTIM` | The named stimulus is not in the server's stimulus set. |
| `BUSY` | A previous request has not yet been consumed by the realtime thread. The client MAY retry. |

### 3.1 `VERSION`

Returns the JILL version the server was built from. A client SHOULD issue this
first and refuse to continue against a version it does not understand.

### 3.2 `STIMLIST`

Returns the stimulus set as a JSON object (§5). The set is fixed at startup from
the command line and does not change while the server runs, so a client SHOULD
request it once and cache it.

### 3.3 `PLAY <name>`

Requests playback of the stimulus called `<name>`. The name is separated from
the verb by exactly one space and extends to the end of the frame, so a name
MAY contain spaces. Names come from the stimulus file's basename with its
directory and extension removed, so `/data/stims/song a.wav` is `song a`.

`<name>` MUST NOT be empty. A `PLAY` with no name is malformed and MUST be
answered `BADCMD`.

If `<name>` is not in the stimulus set the server replies `BADSTIM` and does
nothing. Otherwise it replies `OK`, and the outcome follows on the event
channel: `PLAYING` if playback began, or `BUSY` if another stimulus was already
playing.

### 3.4 `INTERRUPT`

Requests that playback stop immediately. The reply is `OK`, and the outcome
follows on the event channel: `INTERRUPTED` if a stimulus was cut off, or
`NOTPLAYING` if nothing was playing.

Interruption is not a fade — output drops to silence at the next period
boundary, and a `stim_off` MIDI message is emitted at that instant.

### 3.5 Ordering

`VERSION` and `STIMLIST` are answered from the server's main thread and are
always available. `PLAY` and `INTERRUPT` reach the realtime thread through a
single-slot request register, so at most one may be outstanding; a second one
arriving before the first is consumed is answered `BUSY`. The window is one
JACK period, typically a few milliseconds.

## 4. Events

Events are published as they occur. Each is a single frame.

| Event | Emitted when |
|---|---|
| `STARTING` | The server has bound the event endpoint and is ready. |
| `PLAYING <name> <frame>` | Playback of `<name>` began. |
| `DONE <name> <frame>` | `<name>` played to its end. |
| `INTERRUPTED <name> <frame>` | `<name>` was cut off by `INTERRUPT` or by a stream break. |
| `XRUN <name> <frame>` | The audio stream broke while `<name>` was playing. Not emitted when nothing is playing; see §4.3. |
| `BUSY` | A `PLAY` arrived while another stimulus was playing. It was discarded. |
| `NOTPLAYING` | An `INTERRUPT` arrived with nothing playing. |
| `STOPPING` | The server is shutting down. |

`BUSY` and `NOTPLAYING` carry no arguments. Both are outcomes of a request that
was already answered `OK`, which is why the request channel alone is not enough
to know what happened.

### 4.1 Frame times

`<frame>` is the JACK frame counter, a decimal unsigned 32-bit integer. It
**wraps**, roughly every 27 hours at 44.1 kHz, so clients MUST treat it as
modular: compare by subtraction, never by magnitude.

`PLAYING` and `INTERRUPTED` report the frame at the start of the period in which
the event occurred. `DONE` reports the frame of the last sample written, which
is inside the period rather than at its start. All of these are the server's
JACK clock, which is meaningful only to other clients of the same JACK server —
see the note on `jrelay` in the JILL specification for why frame counts do not
travel to other acquisition systems.

### 4.2 Parsing

Because a stimulus name MAY contain spaces, a client MUST parse the argument
form by splitting the verb from the left and the frame from the **right**:

```python
verb, rest = msg.split(" ", 1)
name, frame = rest.rsplit(" ", 1)
```

Splitting on all whitespace will mis-parse any name containing a space.

### 4.3 Stream breaks

An xrun, or a change of JACK period size, means the audio stream has a gap in
it. When this happens while a stimulus is playing, the server publishes `XRUN`
naming that stimulus, truncates playback, and publishes `INTERRUPTED`. The
trial is over and is not valid.

When nothing is playing, the server MUST NOT publish anything. An `XRUN` event
carries the name of the stimulus it ruined, and between trials there is no such
stimulus — the event would have to name something that does not exist, and this
protocol reports what happened to stimuli rather than the health of the audio
stream.

The consequence is deliberate but worth stating: **a client cannot use this
channel to learn that the stream is breaking up between trials.** That is real
information about a degrading session, and it is available elsewhere — JACK
reports xruns to the server log, and `jrecord` records them in the ARF file as
part of the recording they actually affect. A client that wants to monitor
stream health should look there rather than here.

### 4.4 Delivery

Events are best-effort, and a client MUST NOT assume it sees all of them:

- **The event channel is `PUB`/`SUB`.** A subscriber that connects after the
  server binds misses everything published in the interim, including `STARTING`.
  A client that needs `STARTING` must be subscribed before the server starts,
  which in practice means it cannot be relied upon; treat a successful `VERSION`
  exchange as the readiness signal instead.
- **The server's internal event ring is finite and drops silently when full.**
  It is constructed for 64 events but the allocation rounds up to a memory
  page, so the real capacity is platform-dependent — 256 events on a 4 KB page,
  more where pages are larger. A subscriber that stalls long enough for the
  server to produce that many events will lose some without being told.

## 5. The stimulus list

`STIMLIST` returns a single JSON object:

```json
{"stimuli":[{"name":"tone","duration":"0.200000003"},{"name":"tone2","duration":"0.300000012"}]}
```

`stimuli` is an array with one entry per loaded stimulus, in the order given on
the command line. Stimuli that failed to load are absent — the server logs the
failure and continues, so an empty array is possible.

Two things about this encoding are worth stating plainly, because both will
mislead a client author who assumes ordinary JSON:

- **Every value is a JSON string, including `duration`.** The serializer is
  Boost's `property_tree` writer, which does not track types. Clients MUST
  convert `duration` to a number themselves.
- **`duration` is a single-precision float rendered at full precision**, which
  is why `0.2` appears as `0.200000003`. It is a duration in seconds and SHOULD
  be treated as approximate; a client needing exact lengths should count frames
  from the event stream.

Clients MUST ignore members they do not recognise, so that fields can be added.

Names in the array are not guaranteed unique. A name is a file basename with
its directory stripped, so two stimuli in different directories can collapse to
one name; the list reports both while only the first is playable (see §7).
Clients SHOULD treat a repeated name as a configuration error and report it.

## 6. MIDI output

Independently of this protocol, the server emits MIDI on its `trig_out` JACK
port: `stim_on` with the stimulus name when playback begins, and `stim_off` with
the name when it ends, is interrupted, or is truncated. These use
`midi::channel::stim` (channel 0). This is how playback is recorded by `jrecord`
in the same JACK graph, and it carries sample-accurate timing that the ØMQ event
channel does not.

A client that needs a durable, precisely timed record of what was presented
SHOULD use the MIDI path and treat the event channel as supervisory.

## 7. Known divergences

The following are defects in the current implementation, not intended
behaviour. They are listed so that client authors know what to expect and so
that a conforming client can assert against the intended behaviour.

| # | Intended | Actual |
|---|---|---|
| 4 | Two stimulus files with the same basename are rejected, or disambiguated. | `STIMLIST` reports both, with their true durations, but only the first is loaded. `PLAY` always gets the first, so a client timing against the second's duration is silently wrong. |

Three earlier entries have been fixed and are recorded here only so that a
client written against an older build knows what it may meet.

- **An xrun with nothing playing crashed the server** (SIGSEGV), because the
  event was published with no stimulus to name and the publisher dereferenced
  it. This needed no client involvement and fired on ordinary xruns, not only
  on the deliberate period-size change used to provoke it.
- **`PLAY` with no name aborted the server** (SIGABRT). It matched the
  dispatch, `substr` threw, and the exception unwound past a joinable thread
  before any handler could run.
- **`BADCMD` and `BUSY` were interchangeable.** The busy test ran before the
  command was recognised, so an unknown request was answered `BUSY` whenever
  the realtime thread had not yet consumed the previous one.

## 8. Grammar

```abnf
; Requests: one UTF-8 frame, no terminator.
request       = version-req / stimlist-req / play-req / interrupt-req
version-req   = %s"VERSION"
stimlist-req  = %s"STIMLIST"
play-req      = %s"PLAY" SP stim-name
interrupt-req = %s"INTERRUPT"

; Replies: one UTF-8 frame.
reply         = version / stimlist / %s"OK" / %s"BADCMD"
              / %s"BADSTIM" / %s"BUSY"
version       = 1*VCHAR                 ; JILL version, e.g. "2.2.2"
stimlist      = json-object             ; see section 5

; Events: one UTF-8 frame, published unsolicited.
event         = %s"STARTING"
              / %s"STOPPING"
              / %s"BUSY"
              / %s"NOTPLAYING"
              / %s"PLAYING"     SP stim-name SP frame
              / %s"DONE"        SP stim-name SP frame
              / %s"INTERRUPTED" SP stim-name SP frame
              / %s"XRUN"        SP stim-name SP frame

; A stimulus name is a file basename stripped of directory and extension, so
; it may contain spaces. In the event forms it is delimited on the right by
; the final SP before the frame counter.
stim-name     = 1*( VCHAR / SP )
frame         = 1*DIGIT                 ; JACK frames, unsigned 32-bit, wraps
```
