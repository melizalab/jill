# JILL modules

Every module is a JACK client. They are combined by connecting ports, so a
setup is usually a handful of these started together by a script — see
`scripts/jpresent-mini.py` and `scripts/jpresent-ephys.py` for worked examples.

This page is a reference for what each one is *for*. Run any of them with
`--help` for the exact options; the tutorials under
[the documentation index](README.md) show them in use.

## Common to all modules

Every module accepts:

| Option | Meaning |
|---|---|
| `-s`, `--server` | which JACK server to connect to (default `default`) |
| `-n`, `--name` | the JACK client name, which also names its ports |
| `-C`, `--config` | read options from an ini file; the command line wins |
| `-L`, `--no-remote-log` | do not forward log messages to `jrecord` |
| `-v`, `--version` / `-h`, `--help` | |

Log messages go to the console *and*, unless `-L` is given, to a `jrecord` in
the same JACK graph, which records them in the ARF file alongside the data.
`-L` is not needed when no `jrecord` is running: messages queue until one
appears, and are dropped if the queue fills. See
[the specification](specification.org) for the socket conventions.

## Recording

### jrecord

Writes sampled and event data to an [ARF file](arf-files.md). The output file
is a positional argument, not an option:

```shell
jrecord -i system:capture_1 -i system:capture_2 recording.arf
```

`-i` connects an existing port; `-I` and `-E` create input ports for sampled
and event data respectively, for other modules to connect *to*. With `-t` it
records only between trigger events on `trig_in`, keeping `--pretrigger`
seconds before each onset and `--posttrigger` after each offset. `-a key=value`
attaches attributes to every entry. See
[Basic Recording](basic-recording.md) and
[Triggered Recording](triggered-recording.md).

## Stimulus presentation

### jstim

Plays a list of stimuli in sequence, resampling to the server's rate. Stimuli
are named on the command line, each optionally followed by a repeat count, or
listed in a file with `--stim-list`. `-S`/`--shuffle` randomises the order and
takes an optional seed, so a presentation can be repeated exactly; the seed is
logged either way.

It emits MIDI on `sync_out` at every stimulus onset and offset, which is how
the recording knows what was presented when. `--trial-before` and
`--trial-after` widen that window on a second channel for anything needing lead
time, and `--condition-prob` marks a proportion of each stimulus's repetitions
on a third — see [MIDI channels](#midi-channels) below. With `--trig` it waits
for an external trigger instead of free-running. See
[Stimulus Presentation](stimulus-presentation.md).

### jstimserver

Plays stimuli on demand rather than from a fixed list. A client connects over
ØMQ and asks for a stimulus by name; the server replies whether the request was
accepted and publishes what actually happened. It is for closed-loop
experiments, where what to present next depends on what the animal just did.

The exchange is specified in full in
[the jstimserver control protocol](jstimserver-protocol.md), which is worth
reading before writing a client — a reply reports that a request was
*accepted*, never that it succeeded.

### jclicker

Emits a pulse on an audio output in response to MIDI events, for driving
hardware that expects a TTL edge. Each `--pulse` gives a condition, a shape, a
duration in milliseconds and an optional delay:

```shell
jclicker --pulse 0x00,positive,5 --pulse 0x10,negative,5
```

The condition is the MIDI status byte to match, so the example fires on
stimulus onsets and offsets from `jstim`. Shapes are `positive`, `negative` and
`biphasic`; a biphasic pulse is charge balanced at any duration, which matters
when the far end is an electrode. `--amplitude` reduces the level below full
scale, which some TTL conversion hardware prefers. `--prob` fires on only a
fraction of events, balanced per stimulus.

### jrelay

Forwards stimulus names from a MIDI port to open-ephys over ØMQ, so that the
other acquisition system records what was presented.

**It carries identity, not timing.** A JACK frame count means nothing to a
machine running its own acquisition clock, so the messages say only *what*, and
the *when* comes from a TTL that `jclicker` fires on a separate hardware line.
`jrelay` is therefore only useful alongside that TTL — `jpresent-ephys.py` sets
up both. Using it on its own would need a clock both ends understand, which is
a design question rather than a configuration one.

## Detection

### jdetect

Watches an input for signal onsets and offsets and emits MIDI events, for
triggering `jrecord`. It counts threshold crossings in short analysis periods
and compares a running total against a count threshold, which biases it towards
signals with energy at high frequencies — useful for birdsong.

`--period-size` sets the analysis period; `--open-period` and `--close-period`
set how long a window the running total covers when opening and closing the
gate. Both windows must be at least one period long, and the module refuses to
start otherwise. See
[Triggered Recording](triggered-recording.md) and
[the implementation notes](implementation-notes.md).

## Signal generation and processing

### jnoise

Generates band-limited noise on `out`, at one amplitude during the day and
another at night. `--start` and `--stop` give the times of day the level
changes, and amplitudes are in dB relative to full scale. Intended as masking
noise for a sound box on a light cycle.

### jamnoise

Converts an input signal to amplitude-modulated noise: it extracts the envelope
of the input, using a Hilbert transform and a low-pass filter, and applies it to
a noise carrier. The result preserves the amplitude structure of the original
while destroying its spectral detail, which is what makes it useful as an
acoustic control. `--envelope` outputs the envelope itself instead, for
checking that the extraction is behaving.

### jdelay

Delays a signal by `--delay` milliseconds between `in` and `out`. Useful for
compensating a known latency somewhere else in the graph, and for measuring
one.

### jmonitor

A small utility rather than a processing module: it turns JACK's monitoring on
for a named client and leaves it on until interrupted. It takes a client name
as its only argument and none of the common options above.

## MIDI channels

Modules that emit stimulus events use the low nibble of the MIDI status byte as
a channel, and the high nibble to say what happened. The channels are named for
the interval each one delimits:

| Channel | Name | Delimits |
|---|---|---|
| 0 | `stim` | the stimulus itself: exact onset and offset |
| 1 | `trial` | a wider window around it, for anything needing lead time |
| 2 | `condition` | trials carrying an out-of-band manipulation |

The naming is deliberate and worth preserving. Channel 1 was once called "acq",
after the acquisition it was there to start early enough to cover a trial; when
a second experiment used the same window to gate an LED, the name had quietly
stopped describing anything. What the channel *is*, in both cases, is the
trial's outer bracket.

Channel 2 is **additive**: channel 0 still fires on every trial, including
marked ones. Anything matching only channel 0 keeps working unchanged when
`--condition-prob` is introduced.

The status bytes themselves are listed in [the ARF file layout](arf-files.md),
which is where they end up.
