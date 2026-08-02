# Performance and stability

This section deals with configuring JACK and JILL for low-latency, reliable operation. Modern multithreaded operating systems are typically doing a lot in the background, and there are often periods when the OS is tied up. If your application depends on receiving or producing a steady stream of samples, these periods of heavy activity can lead to glitches and dropouts.

One option for dealing with these problems is to use large memory buffers, which can hold samples during periods of heavy load. Large buffers mean long latencies. If you're only recording or presenting data, or if all the stimuli are defined ahead of time, or you can generate stimuli seconds to minutes before they're needed, you should use long periods (2048 samples or more) to ensure that data is not lost.

In closed-loop applications, the output of the system depends tightly on the input, and latencies typically need to be short. How short depends on the nature of the application. JACK can provide latencies on the order of 1-2 ms if properly configured. It's important to recognize, though, that Linux and OS X are not designed for realtime performance. Only a dedicated hard realtime system can provide guarantees on latency. Below are some measures for improving performance.

## Increase buffer size or decrease sampling rate

Larger periods give JACK clients more time to process the data and make the whole system less vulnerable to xruns. Period sizes need to be a power of two. For example, to run JACK with a period size of 2048 samples:

```shell
jackd -p 2048
```

Increasing the number of periods for playback latency may also help. In `qjackctl` you can adjust these parameters in the setup window and it will report the expected latency.

## Adjust other JACK parameters

Other things to try include:

1. Make the JACK daemon more forgiving of xruns. `jackd -Z ...`

2. Turn off playback ports. `jackd -d alsa -C ...`

3. Decrease the number of channels to what you need. To enable only 4 capture and playback ports: `jackd -d alsa -i 4 -o 4 ...`

Also try running JACK 2 instead of JACK 1; it's more fault tolerant and handles port connections without glitching.

## Keep the system clean

Install a system with a minimal number of applications, and disable any recurring operations.

## Install a low-latency kernel

On Linux, you can install a kernel that will allow itself to be pre-empted by realtime user processes, greatly reducing the probability of xruns. You shouldn't expect hard realtime performance, but you should be able to get latencies down in the 1-2 ms range. For Debian, Pengutronix maintains kernels that have been patched for low-latency: <http://www.pengutronix.com/software/linux-rt/debian_en.html>.

For Red Hat based system, CCRMA at Stanford maintains a repository with patched kernels (<http://ccrma.stanford.edu/planetccrma/software/>). These are not being updated as frequently as the Pengutronix kernels.

You can also try to recompile the kernel yourself with the realtime preemption patches enabled.

## Other settings

This script will test various settings in your installation and make recommendations for improvement:

<https://codeberg.org/rtcqs/rtcqs>
