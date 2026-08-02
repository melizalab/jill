# Recording stimulus-evoked activity

In this tutorial, we'll see how to do a classic "open-loop" neurophysiology experiment, where we present auditory stimuli and record neural responses. We won't use any new modules, but we'll wire them up in a different way. This exercise should show you how different kinds of experiments can be set up using the same basic components.

By now it should be clear what modules we need to run this experiment. We'll use `jstim` to output sounds from a playback port, and `jrecord` to record neural data from one or more capture ports. One way of setting things up would be to let the two processes run independently, with `jrecord` recording continuously and `jstim` playing stimuli at random. Of course, to know what stimuli were presented, you'd want to record the stimulus output on one of the capture ports, perhaps by putting a small microphone near the animal. (It's a good idea to do this in any circumstance, to have a record of any distortions in the signal from the amplifiers, and any delays due to buffering in the sound card.)

But there's a better way. We can use the event output from `jstim` to trigger `jrecord`. What's more, the events that `jstim` emits include the filename of the stimulus that was presented, which will be stored in the ARF file by `jrecord`.

First, start `jrecord` in triggered acquisition mode. If you're recording from many channels you can list them all on the command line:

```shell
jrecord -t -f bu70_1.arf -i system:capture_1 -i system:capture_2 -i system:capture_3 ...
```

or use a configuration file to specify the input ports:

```shell
jrecord -C multichannel.ini -a animal=bu70 -a site=1 -f bu70_1.arf
```

Note the use of the `-a` flags to set attributes on the recorded entries. `multichannel.ini` might look something like:

```ini
trig
pretrigger=1.0
posttrigger=1.0
in=system:capture_1
in=system:capture_2
in=system:capture_3
in=system:capture_4
in=system:capture_5
in=system:capture_6
in=system:capture_7
in=system:capture_8
in=system:capture_9
in=system:capture_10
in=system:capture_11
in=system:capture_12
```

Next, instruct `jstim` to present a set of stimuli 10 times each in random order, with 5 seconds between stimuli:

```shell
jstim -S -r 10 -g 5 -o system:playback_1 -e jrecord:trig_in stimuli/*.wav
```

Note that we've connected the sound output of jstim to a playback port on the sound card, and the event output to the event input of `jrecord`. You should see `jrecord` create an entry for each stimulus as it's presented, and store data between 1 second before each stimulus starts to 1 second after the stimulus ends.

What if you don't want gaps in your recordings? One option is to run `jrecord` in continuous mode:

```shell
jrecord -f bu70_1.arf -E stimuli -i system:capture_1 ...
jstim -S -r 10 -g 5 -o system:playback_1 -e jrecord:stimuli
```

Note that we used the `-E` flag to instruct `jrecord` to create an event port, which we then connected to `jstim`. This port is not used to trigger, but any incoming events will be stored in temporal register to the ARF file.

Alternatively, you can configure `jrecord` in triggered mode so that `pretrigger + posttrigger` is greater than the gap between stimuli. When this is the case, the first sample in each epoch will be immediately after the last sample in the previous epoch. This gives you a continuous recording, but conveniently divided into epochs associated with specific stimuli. You can stitch the data back together later if you need to process it as a continuous stream.

## Post-processing correction for playback delay

**IMPORTANT NOTE:** There is a delay between when `jstim` sends a signal to the sound card and when the sound is output from the speaker. Almost all of this delay is due to buffering. **DO NOT** use the times of the events stored by `jrecord` in analysis, until you've determined what
the delay is. It's strongly recommended that you run the signal from the sound card output to one of your recording system's inputs so that you know exactly what was being presented (or better yet, use a microphone to record what the animal actually heard).

The `scripts/jrecord_postproc.py` script can adjust trigger times to compensate for playback latency. It calculates the actual time of stimulus onset by comparing a recording of the presented stimulus with the original stimulus file. Currently, it only works with the software-synchronization configuration described above, because it uses the records of stimulus onset and offset emitted by `jstim` and stored in `trig_in` dataset to determine what stimulus was presented.  However, in principle the same algorithm can be used to identify the stimulus as well as the time it started so future versions may support the hardware-synchronization configuration.

`jrecord_postproc` works by cross-correlating the copy line recording with the stimulus known to be presented in an entry. It also uses the onset and offset times recorded by `jrecord` as hints for where to look for the real onsets and offsets. The script first calculates the differences between recorded onsets/offsets and the true onsets/offsets, then presents statistics on the errors to the user.  The delay should be nearly constant (within 2-3 samples, depending on the quality of the copy line signal) across all entries and between onsets/offsets.

Example:

```shell
jrecord_postproc.py -r copy_line_channel -t trig_in recording.arf /path/to/stimuli
```
