# JILL modules

Every module is a JACK client. They are combined by connecting ports, so a
setup is usually a handful of these started together by a script — see
`scripts/jpresent-mini.py` and `scripts/jpresent-ephys.py` for examples.

This page explains what each module is used for and some brief notes on their
options. Every module has a `--help` output that gives the exact options, and
the tutorials under [the documentation index](README.md) show them in use.

## Common to all modules

Every module accepts:

| Option                             | Meaning                                              |
|------------------------------------|------------------------------------------------------|
| `-s`, `--server`                   | which JACK server to connect to (default `default`)  |
| `-n`, `--name`                     | the JACK client name, which also names its ports     |
| `-C`, `--config`                   | read options from an ini file; the command line wins |
| `-L`, `--no-remote-log`            | do not forward log messages to `jrecord`             |
| `-v`, `--version` / `-h`, `--help` |                                                      |

Log messages go to the console and to a `jrecord` in the same JACK graph, which
records them in the ARF file alongside the data. If `jrecord` is not running,
messages will queue until one appears. These will be dropped if queue fills, but
you can also shut off remote logging from startup with the `-L` flag. See [the
specification](specification.org) for the socket conventions.

## Recording

### jrecord

Writes sampled and event data to an [ARF file](arf-files.md). The output file is
a positional argument.

```shell
jrecord -i system:capture_1 -i system:capture_2 recording.arf
```

`-i` connects to an existing port; `-I` and `-E` create input ports for sampled
and event data respectively, for other modules to connect to. Each input port is
stored in a separate channel. The `-t` option will put `jrecord` into triggered
recording mode. It creates an event input port (`trig_in`) and listens for onset
and offset events, keeping `--pretrigger` seconds before each onset and
`--posttrigger` after each offset. `-a key=value` attaches attributes to every
entry. See [Basic Recording](basic-recording.md) and [Triggered
Recording](triggered-recording.md) for tutorials.

## Stimulus presentation

### jstim

Plays a list of stimuli in sequence. Stimuli are named on the command line, each
optionally followed by a repeat count, or listed in a file with `--stim-list`.
`-S`/`--shuffle` randomizes the order, with an optional seed so a presentation
can be repeated exactly.

It emits song_on and song_off MIDI events on `sync_out` at every stimulus onset
and offset, which is how the recording knows what was presented when.
`--trial-before` and `--trial-after` cause events to be emitted on a second
channel with a wider window; this is used when there is hardware that needs to
be activated with before the stimulus comes on (e.g., calcium imaging).
`--condition-prob` marks a proportion of each stimulus's repetitions on a third
channel (see [MIDI channels](#midi-channels)). With `--trig` it waits for an
external trigger instead of free-running. See [Stimulus
Presentation](stimulus-presentation.md).

### jstimserver

Plays stimuli on demand. The module loads a list of stimuli; a client connects
over ØMQ and asks for a stimulus by name; the server replies whether the request
was accepted and publishes what actually happened. Main application is for
behavioral experiments, where operant control software needs to control which
stimulus is presented in a given trial. Also emits song_on and song_off MIDI
events for downstream realtime handling.

The ØMQ protocol is specified in [the jstimserver control
protocol](jstimserver-protocol.md).

### jclicker

Emits a pulse on an audio output in response to MIDI events, for driving
hardware that expects a TTL edge. Each `--pulse` gives a condition, a shape, a
duration in milliseconds and an optional delay:

```shell
jclicker --pulse 0x00,positive,1 --pulse 0x10,negative,1
```

The condition is the MIDI status byte to match, so this example fires on
stimulus onsets and offsets from `jstim`. Shapes are `positive`, `negative` and
`biphasic`. `--amplitude` reduces the level below full scale, which some TTL
conversion hardware prefers.

Note that when pulses are delivered through audio hardware, the maximum voltage
will be line level (±1.6V for consumer hardware), and there is usually an AC
coupling capacitor on the output that will return any output to baseline, so the
actual output will be an exponentially decaying pulse rather than a square wave. Brief clicks are fine as a synchronization signal, but if you need longer pulses and/or logic level, you'll have to convert with a Schmitt trigger (we use [this circuit](https://github.com/melizalab/audio-sync-circuit)) or a microcontroller.

`--prob` can be used to only fire the signal on a fraction of events, balanced
per stimulus. Prefer using `jstim`'s `--condition-prob` option whenever
possible, as this is only approximate.

### jrelay

Forwards events from a MIDI port to open-ephys over ØMQ, so that the other
acquisition system records what was presented.

The main purpose of this module is to get open-ephys to record stimulus identity
in its data tree, not to control or record exact timing. The JACK frame count is
on a different clock from the open-ephys hardware. The timing information is
carried on a TTL that `jclicker` fires on a separate hardware line. `jrelay` can
also be used to log event data to the console, and may be expanded in the future
to talk to other systems over the network or interprocess communication channels.

## Detection

### jdetect

Watches an input for signal onsets and offsets and emits note_on and note_off
MIDI events, for triggering `jrecord`. It counts threshold crossings in short
analysis periods and compares a running total against a count threshold, which
biases it towards signals with energy at high frequencies (useful for birdsong).
The MIDI channel is configurable with `--channel`.

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
while destroying its spectral detail. `--envelope` outputs the envelope itself instead, for
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

| Channel | Name        | Delimits                                                 |
|---------|-------------|----------------------------------------------------------|
| 0       | `stim`      | the stimulus itself: exact onset and offset              |
| 1       | `trial`     | a wider window around it, for anything needing lead time |
| 2       | `condition` | trials carrying an out-of-band manipulation              |

Channel 2 is **additive**: channel 0 still fires on every trial, including
marked ones. Anything matching only channel 0 keeps working unchanged when
`--condition-prob` is introduced.

The status bytes themselves are listed in [the ARF file layout](arf-files.md),
which is where they end up.
