# Network JACK

It can be useful to send data from JACK to other computers for monitoring or processing. There are a number of different protocols for connecting two JACK systems over a network. By far the most straightforward is [JackTrip](http://code.google.com/p/jacktrip/). You can install jacktrip on Debian with `apt-get install jacktrip`, and there are binaries available for OS X on the JackTrip website.

After starting JACK normally on both machines, start jacktrip on the master computer:

```shell
jacktrip -s
```

On the slave computer, start jacktrip and connect to the master computer's IP address:

```shell
jacktrip -c <master_ip>
```

After the two jacktrip clients connect to each other, they will connect their input and output ports to the first system:capture and system:playback ports. Unfortunately there is no way to disable this behavior, or to set up a one-way connection, so if you want to connect other ports you'll have to do it manually.

Some other limitations in jacktrip:

- only one client can be connected to a server
- server and client deaths are not handled very gracefully

The other options for connecting JACK systems over the network:

- [netjack](http://trac.jackaudio.org/wiki/WalkThrough/User/NetJack)
- [netjack2](http://trac.jackaudio.org/wiki/WalkThrough/User/NetJack2)
