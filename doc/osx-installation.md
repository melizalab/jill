# OS X installation

The installation instructions for OS X are similar to those for [Debian](debian-installation.md), but use MacPorts instead of the Debian package system.

The build works with either MacPorts or [Homebrew](https://brew.sh) — it looks
for both, and asks `brew --prefix` where Homebrew keeps things rather than
assuming, since that differs between Apple Silicon and Intel machines. The
commands below are given for MacPorts because that is what the lab uses; the
Homebrew formula names are generally the same without the `py-` prefix. If
Boost ends up somewhere neither is expected, point `BOOST_ROOT` at its
installation prefix.

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
and the ARF file format is checked from python with h5py. Neither is needed
to build the modules, only to build and run the tests:

```shell
sudo port install doctest py-pytest py-h5py
```

See [testing notes](testing-notes.md) for how to run the suites and what the
sanitizer builds check.
