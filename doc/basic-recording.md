# Basic recording

This tutorial demonstrates how to save data from a sound card input to disk. Conceptually, this is the reverse of the playback operation. Instead of moving data from a file to a playback port, you'll move data from one or more capture ports to a file. There are many JACK modules that will record one channel to a WAVE file or some other simple format, which we won't cover here. Instead, we'll use the `jrecord` module, which is specialized for recording many channels of data to an HDF5 file. HDF5 (<http://www.hdfgroup.org/HDF5>) is a structured file format that can store data hierarchically, with detailed metadata. It's critical to store experimental data in an open, extensible, and well-documented format, so that it remains accessible and interpretable far into the future. `jrecord` uses the [ARF format](https://meliza.org/spec:1/arf/) to organize data in the HDF5 files.

Try recording continuously from one of the sound card's capture ports:

```shell
jrecord -i system:capture_1 <filename>
```

Let this process run for a few seconds, quit it with `Ctrl-C`, and then examine the contents of the file with `h5ls <filename>`. You should see something like this:

```
jill_log                 Dataset {14/Inf}
jrecord_0000             Group
```

You can print a log of all the events that occurred during the recording session, including port connections and disconnections, with `h5ls -d <filename>/jill_log`. The `jrecord_0000` group will contain datasets, one for each channel that was recorded. See the [ARF specification](https://meliza.org/spec:1/arf/) for more information on how data is organized.

You can (and should) record metadata about the experiment when making recordings by specifying attributes as commandline arguments. For example:

```shell
jrecord -i system:capture_1 -a experimenter=cdmeliza -a animal=bu38 -a mic=ME66 <filename>
```

You can list the attributes stored in an entry with the command.

```shell
h5dump -A -g /jrecord_0000 <filename>
```

All the entries created by jrecord have a `timestamp` attribute, which is the number of seconds since January 1, 1970 UTC, and a `jack_frame` attribute, which corresponds to the number of samples since the JACK daemon started.

**IMPORTANT NOTE:** You can use the same file to store multiple recording sessions in the same file sequentially, but not in parallel. Don't let more than one `jrecord` process write to the same file. The HDF5 library has no way to coordinate access by multiple programs and the file will be corrupted. Furthermore, be aware that if the JACK daemon is restarted, the internal frame counter will reset, and the `jack_frame` attribute will be inconsistent in the file. You'll get into even worse problems if you change the sampling rate. For these reasons, it's strongly recommended that during data collection you use only one ARF file per recording session.

## Configuration files

It can quickly become tedious to specify long lists of attributes or input channels on the command line. One solution is to write shell scripts for common tasks. Another, complementary solution is to put commonly used options in configuration files. All **JILL** modules have a common configuration file format, with a simple syntax consisting of a list of option=value pairs. The options a program supports are listed in the commandline help (e.g. `jrecord -h`). Use the long form of any option. A configuration file for the `jrecord` command above would look like:

```ini
in=system:capture_1
attr=experimenter=cdmeliza
attr=animal=bu38
attr=mic=M66
```

To run jrecord with these options: `jrecord -C <configfile> <data-file>`
