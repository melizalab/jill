# Debian installation

These instructions have been tested on Debian 12 and should work any other
Debian-based Linux system, though some package names may differ. They should also work on Red Hat
based systems, but you'll have to find the equivalent packages in your package
manager.

## JACK and qjackctl

```shell
sudo apt-get install jackd2 libjack-jackd2-dev qjackctl
```

If you wish, you can substitute JACK 2 for JACK 1. Qjackctl is not necessary, but it's really useful for connecting clients.

## YASS

YASS is a simple scrolling oscilloscope. It's not necessary, but it's used in the tutorials to visualize signals in JACK.

```shell
git clone https://github.com/dmeliza/yass.git
cd yass
make
sudo make install
```

At this point you're ready to go through the first tutorial.

## JILL prerequisites

Most of the prerequisites are available through package managers:

```shell
sudo apt-get install scons libboost-system-dev libboost-date-time-dev libboost-program-options-dev \
 libboost-filesystem-dev libsndfile1-dev libsamplerate0-dev libzmq3-dev
```

If you want to use `jrecord`, you need to install hdf5:

```shell
sudo apt-get install hdf5-helpers hdf5-tools libhdf5-dev
```

The ARF headers are tracked as a git submodule, pinned to a known-good release.
Fetch them by cloning with `--recursive`, or, in an existing checkout:

```shell
# run this command in the jill repository
git submodule update --init
```

## Building JILL

```shell
scons -Q modules
```

To omit compiling `jrecord`, run `scons -Q --no-arf modules` instead. You can run the modules from their build directory. (e.g. `modules/jstim`). To install modules in a system location, use the following command:

```shell
scons -Q install
```

The default install location is /usr/local/bin, but this can be changed with the `prefix` argument.
