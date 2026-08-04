# JILL documentation

**JILL** is a system for auditory behavioral and neuroscience experiments. It
consists of several independent modules that can present stimuli, detect
vocalizations, and record data. Modules can be connected to send and receive
sampled data (e.g., audio or neural waveforms) and event-time data (e.g. action
potentials, stimulus onsets and offsets) in a low-latency, real-time framework.
This modular design provides great flexibility in running closed- and open-loop
experiments.

## Installation

**JILL** should work on any system that can run JACK. Some compiling from source
is required, so you will need a reasonably modern C/C++ compiler.

* [Debian](debian-installation.md)
* [OS X](osx-installation.md)

## Tutorials

* [Setting up JACK](setting-up-jack.md)
* [Stimulus Presentation](stimulus-presentation.md)
* [Basic Recording](basic-recording.md)
* [Triggered Recording](triggered-recording.md)
* [Recording Stimulus-Evoked Activity](recording-stimulus-evoked-activity.md)

## Topics

* [Multiple Hardware Devices](multiple-hardware-devices.md)
* [Network JACK](network-jack.md)
* [Other JACK clients](other-jack-clients.md)
* [Basic Signal Processing](basic-signal-processing.md)
* [Performance and Stability](performance-and-stability.md)
* [JILL ARF files](arf-files.md)

## Resources

* [JILL specification](specification.org)
* [Testing notes](testing-notes.md)
* [JILL implementation notes](implementation-notes.md)
* [JACK website](http://www.jackaudio.org)
* [Notes on optimizing Linux systems for realtime audio processing](http://wiki.linuxaudio.org/wiki/system_configuration)
* [Notes on latency in JACK](http://apps.linuxaudio.org/wiki/jack_latency_tests)
* [JACK driver for the Intan RHD2000 eval board](https://github.com/dmeliza/jack_rhd2000) (under development)
