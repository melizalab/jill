# OS X installation

The installation instructions for OS X are similar to those for [Debian](debian-installation.md), but use MacPorts instead of the Debian package system.

## MacPorts

If you don't already have MacPorts, get it [here](http://www.macports.org).

## JACK and qjackctl

You can install JACK with MacPorts:

```shell
sudo port install jack
```

But a simpler and more flexible solution is provided by [Jack OS X](http://www.jackosx.com), which to route audio to and from native OS X applications, and includes qjackctl.

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

Most of the prerequisites are available through MacPorts:

```shell
sudo port install scons boost libsamplerate libsndfile zmq
```

Note that this installs ZMQ 4.x. Older version of ZMQ are no longer compatible. If you plan to use `jrecord`, you will also need the hdf5 dependencies:

```shell
sudo port install hdf5 -cxx +threadsafe
```

The ARF headers themselves are tracked as a git submodule, pinned to a
known-good release, so they are fetched along with the source below.

## JILL modules

```shell
git clone --recursive https://github.com/melizalab/jill.git
cd jill
scons -Q modules
```

To omit compiling `jrecord`, run `scons -Q --no-arf modules` instead. You can run the modules from their build directory. (e.g. `modules/jstim`). To
install modules in a system location, use the following command:

```shell
scons -Q install
```

The default install location is /usr/local/bin, but this can be changed with the
`prefix` argument.

## Running the tests

The unit tests are written against [doctest](https://github.com/doctest/doctest),
which is only needed to build and run the tests, not the modules themselves:

```shell
sudo port install doctest
```

Build the test programs and run one:

```shell
scons -Q test
./test/test_ringbuf
```

Each suite is a separate binary under `test/`, and reports a non-zero exit
status if any of its cases fail. Some of the older programs there are not yet
converted and need external resources — a running JACK server, or a bound
socket — so they cannot all be run unattended.
