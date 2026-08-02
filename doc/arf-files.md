# JILL ARF files

`jrecord` stores data in HDF5 files, a hierarchical format that supports a wide
range of architectures and data types.  It organizes the data in the file
according to the ARF specification (see <https://github.com/melizalab/arf>).
Briefly, this layout is as follows:

- The `/jill_log` dataset stores log messages from `jrecord` and from other
  programs that connect to its 0MQ logging socket.
- Each recording epoch is stored in a sequentially named group,
  `/jrecord_0000, /jrecord_0001, ...`. These groups (*entries* in
  the ARF specification) have a number of attributes:
  - `timestamp` is a two-element vector containing the unix timestamp (number
    of seconds and microseconds since Jan 1, 1970 UTC)
  - `jack_frame` is the frame count at the start of the entry
  - `jack_sampling_rate` is the sampling rate of the JACK engine when the
    entry was created.
  - `entry_creator` is a string indicating the program that created the file
    and its version
- Each entry contains datasets that correspond to `jrecord` ports.  Sampled and
  event data are stored differently (see below). All datasets have attributes
  that indicate the units and sampling rate of the data.

## Sampled data

`jrecord` stores sampled data in one-dimensional arrays of 32-bit floats, which
is how these data are represented in JACK. Values are bounded between -1.0 and
1.0.

## Event data

`jrecord` stores data from event ports in HDF5 datasets using a compound data
type. The fields are:

- `start`: the frame count when the event occurred
- `status`: the MIDI status byte of the message
- `message`: a string giving the remainder of the event message. This string may
  be hex-encoded for some event types and UTF-8 encoded for others.

**JILL** modules use the following status bytes:

|   value | explanation  | message                                        |
|--------:|--------------|------------------------------------------------|
|    0-15 | stimulus on  | the name of the stimulus, utf-8 encoded string |
|   16-31 | stimulus off | the name of the stimulus, utf-8 encoded string |
| 128-143 | note on      | pitch and velocity, 2 bytes, hex encoded       |
| 144-159 | note off     | pitch and velocity, 2 bytes, hex encoded       |

At present, `jstim` emits stimulus on and stimulus off events when it starts
and stops playback. `jdetect` emits note on and note off events when it detects
signals starting and stopping. It uses the MIDI default pitch of 60 and
velocity of 64.
