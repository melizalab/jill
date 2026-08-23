# Orchestration scripts

In the tutorials so far, we've started modules one at a time and connected them by hand. In real experiments there may be four or five processes that all have to be started in the right order, wired to the right ports, and shut down in the right order at the end. Doing that by hand at the start of every session is tedious, and worse, it's easy to get subtly wrong in a way you won't notice until you look at the data.

The `scripts` directory contains Python programs that handle orchestration for the most common use cases.

## Triggered acoustic recording

`jrecord-triggered.py` sets up the triggered recording from [Triggered Recording](triggered-recording.md), with a `jdetect` watching a channel, and a `jrecord` that opens an entry whenever the detector fires.

```shell
jrecord-triggered.py --animal bu70 --experimenter dmeliza -i system:capture_1
```

This will write `bu70_20260823-141530.arf` in the current directory, one entry per detected vocalization, and run until you press ctrl-C.

The detector options have reasonable defaults but are all exposed so you can tune them for a given setup.

```shell
jrecord-triggered.py --animal bu70 --experimenter dmeliza \
    --open-rate 30 --open-period 400 --close-rate 5 --close-period 2000 \
    --count-port
```

`--count-port` asks `jdetect` for its running estimate on a port, so you can watch what the detector thinks is happening while you adjust the thresholds. See [Triggered Recording](triggered-recording.md) for what these mean.

## Auditory physiology and calcium imaging

`jpresent-mini.py` and `jpresent-ephys.py` both run stimulus presentation experiments.

`jpresent-mini.py` is for miniscope calcium imaging. It starts `jrecord` to log what was presented, `jstim` to present it, and two `jclicker` instances that convert `jstim`'s MIDI events into clicks on separate hardware lines, one marking the stimulus itself and the other marking a wider window around each trial so acquisition can be started before the stimulus starts. External hardware is needed to convert these line-level clicks into TTL pulses. Trials are normally separated by big gaps to avoid bleaching the calcium indicators.

```shell
jpresent-mini.py --animal bu70 --experimenter dmeliza -r 10 -g 5 -- stimuli/*.wav
```

The `--` option makes everything after pass through to `jstim`, which is how you give it a stimulus list without the script having to know about every `jstim` option.

`jpresent-ephys.py` is for extracellular recording with open-ephys. It does the same job, and adds `jrelay` to send stimulus names to open-ephys over the network. There is an optional second `jclicker` output used to control external hardware like an optogenetic stimulation LED

```shell
jpresent-ephys.py --animal bu70 --experiment site1 \
    --open-ephys 192.168.1.20 --trig-out system:playback_4 --trig-prob 0.5 \
    -r 10 -g 5 -- stimuli/*.wav
```

With `--open-ephys` it also talks to the open-ephys GUI directly over its HTTP API: it checks the GUI is idle before starting, configures the recording directory, finds the port its Network Events plugin is listening on, and puts it into acquisition and then record mode at the right moments. This means open-ephys has to be running, with a Record Node and a Network Events plugin in the signal chain, before you start the script.

## Common features

All three scripts take `--jill-path`, which is where to look for the binaries if they aren't on your `PATH`, and all three log the version of every binary they find. This is useful if you're testing JILL binaries in the source tree before installing them under `/usr/local/bin`.

`jpresent-ephys.py` and `jrecord-triggered.py` also take `-y`/`--dry-run`, which prints the command lines the script would run and then exits.

```shell
jrecord-triggered.py --animal bu70 --experimenter dmeliza --dry-run --debug
```

## Writing your own scripts

Some comments about design and pitfalls to avoid.

**Start order:** A module's `-i` and `-o` options connect to ports that must already exist, so whichever module owns a connection has to start after the module it's connecting to. Most of the modules have options to create ports without connecting anything to them, which allows you some flexibility in how you build up the connection tree. In `jrecord-triggered.py`, `jrecord` starts first with a bare `-t`, which registers its `trig_in` port without connecting anything to it, and then `jdetect` connects itself with `-o jrecord:trig_in`.

There are also logical dependencies between modules. `jrecord` should be started first so that it's there to log messages the other modules emit as they start up. Messages are queued rather than dropped, so a short gap is harmless, but starting `jrecord` first means you don't have to think about it. If you're presenting stimuli, `jstim` should start last, only after everything else is ready.

**Module names:** Every script passes `-n` to name each client, which is what makes `jclicker-sync:in` and `jclicker-trig:in` distinguishable. The JACK daemon will assign unique names to multiple instances of the same module, but it's better to control this.

**Shut down** should happen in reverse. Pause between shutting down modules if you know they need to do some cleanup. The scripts stop the stimulus source first, wait, then the intermediate modules, wait again, and stop `jrecord` last, so that the last trial's data reaches the file before it's closed. `jrecord-triggered.py` waits for `posttrigger + 1` seconds for exactly this reason.

**Handle SIGTERM, not just ctrl-C** in long-running experiments like `jrecord-triggered`. A script that only catches `KeyboardInterrupt` will be killed outright by a supervisor or by `timeout`, leaving `jrecord` running with the ARF file open and locked. `jrecord-triggered.py` installs a `SIGTERM` handler that raises `KeyboardInterrupt`, so both paths run the same cleanup.

## Recording what you did

Use required options to ensure that key experiment metadata gets stored somewhere. All three scripts require `--animal`, and they require one more identifying field that you can't omit, so a file can't be produced without it. Watch the name: `jpresent-mini.py` and `jrecord-triggered.py` take `--experimenter`, the person running the session, while `jpresent-ephys.py` takes `--experiment`, the name of the experiment. They are different fields with confusingly similar names, and they end up as different attributes in the file.

`jpresent-ephys.py` and `jrecord-triggered.py` also take `-k key=value`, repeatable, for anything else you want to keep:

```shell
jrecord-triggered.py --animal bu70 --experimenter dmeliza \
    -k hemisphere=left -k depth_um=1200 -k notes="second session"
```

These end up as HDF5 attributes on every entry, which means the metadata travels with the data rather than living in a lab notebook that may or may not still be next to the file in three years. See [JILL ARF files](arf-files.md) for how to read them back out.
