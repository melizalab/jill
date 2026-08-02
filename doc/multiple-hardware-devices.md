# Multiple hardware devices

It's possible to use multiple hardware devices for input and output. For example, the Intan RHD2000 eval board only supports analog input, so presenting stimuli requires a separate sound card or other device. The fundamental issue with using multiple devices is that no two clocks are identical.  Even two devices running at the same nominal sampling rate will differ slightly, and over long periods of time will gradually diverge.

There are two solutions to this problem:

## The hardware solution

One option is to let the devices run independently (i.e., in separate JACK systems) while exchanging synchronization information.  For example, the signal could be a TTL pulse emitted by the stimulus presentation device at the start and stop of the signal which would be recorded by the input device.  Alternatively, the input device could record the actual signal being produced by the output device.

The major positive of this approach is that it's relatively foolproof, and can work even if the devices are on different computers.  The negative is that setting up the synchronization signal can be a bit complex. Few sound cards can generate TTL signals, so it's usually easier to pass the output of the sound card into one of the inputs of the data acquisition device, and then use software to detect when the signal started.  To use this setup for stimulus-triggered recording with an Intan RHD2000 eval board and a Linux sound card, split the output of the sound card so that one set of wires goes to one of the ADC ports on the eval board, and the other set of wires goes to the amplifier and speaker.  We'll call this the 'copy line'.

```shell
jackd -n input -d rhd2000 &   # this will be the input system
jackd -n output -d alsa -P &  # this will be the output system
jdetect -s input -i rhd2000:EV_0 & # detects stimulus onset on copy line
jrecord -s input -t jdetect:trig_out -i rhd2000:EV_0 -i rhd2000:A1_0 data.arf & # records copy line and neural data
jstim -s output -o system:playback_1 mystimulus.wav
```

`jstim` will output the stimulus to the sound card, and the copy line will pass the signal to `jdetect`, which will detect the onset of the stimulus and trigger `jrecord` to start recording.  You'll need to adjust the pretrigger buffer to ensure that the start of the signal actually gets recorded, and you'll have to post-process the recording from the copy line using cross-correlation to find out when the stimulus actually started (see below).

## The software solution

Another option is to have the devices run the same JACK process, with one controlling the clock (the 'master') and the other 'slave' linked to the master clock through a bridge program. Data sent to (or received from) the slave device is resampled to compensate for differences in sampling rate.  If both devices are in the same computer, this is the most flexible option. However, one must be confident that the resampling does not introduce any distortions into the output signal, so it's a good idea to configure a copy line as described in the previous section.

For the bridge program, you can use `alsa_out`, which comes with JACK, or `zita-ajbridge` (http://kokkinizita.linuxaudio.org/linuxaudio/downloads/). `zita` is much better than `alsa_out` in our hands: the resampling rate estimate converges much faster, the resampler is more efficient, and the algorithm is stable for long periods of time.  To setup software-synchronized triggered recording:

```shell
jackd -d rhd2000   & # this is the master clock
zita-j2a -v > zitaout.log  & # save output to check for errors; set rate close to master rate
jrecord -t -i rhd2000:EV_0 -i rhd2000:A1_0 data.arf  # save copy line and neural data
jstim -e jrecord:trig_in -o zita-j2a:playback_1 mystimulus.wav  # trigger jrecord directly
```

Check the `zitaout.log` file and make sure that the value in the first column stays close to zero, preferably with a magnitude less than 1 sample. A quick plotting command is:

```shell
awk '$1 ~ /[-0-9]/{print $NR/4,$1,$2} zitaout.log | gnuplot -p -e 'plot "-" using 1:2 with lines'
```

Spikes in this time series may indicate that one of the two clocks is not as consistent as the other (a common scenario when one of the devices is on a USB bus).  These spikes will cause the resampler in zita to adjust, possibly distorting the signal.  Try setting the sampling rate or fragment size for the sound card to different values if there are a lot of spikes. Some simple experiments comparing the zero-crossing rate of a sine wave on the copy line indicate that the distortions are very small, on the order of 0.05%.

Note that in this configuration there will still be a delay between the time when `jstim` sends a trigger event to `jrecord` and the actual time of the stimulus onset, and thus it's a good idea to measure this delay precisely (using `jack_iodelay`), and an even better idea to calculate offline the cross-correlation between the signal on the copy line and the presented signal (see [Recording Stimulus-Evoked Activity](recording-stimulus-evoked-activity.md))
