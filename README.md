
## JILL Real-Time Auditory Neuroscience Framework

**JILL** is a system for auditory behavioral and neuroscience experiments. It consists of several independent modules that handle stimulus presentation, vocalization detection, and data recording. Modules can be connected to send and receive sampled (e.g. audio or neural) data and event time (e.g. action potentials, stimulus onsets and offsets) data in a low-latency, real-time framework. This modular design provides great flexibility in running closed- and open-loop experiments.

**JILL** modules use the JACK audio framework (<http://jackaudio.org>) to route data between modules and to and from data acquisition (DAQ) hardware. JACK runs on Linux, OS X, and Windows, and supports a wide range of sound cards out of the box. **JILL** and JACK do not require any specialized hardware. If you have a sound card in your computer, you can record audio or neural data.

There is an ongoing effort to develop third-party drivers for general-purpose DAQ cards, including the Intan RHD2000 eval board (see <https://github.com/dmeliza/jack_rhd2000>). **JILL** modules can interoperate with any existing JACK modules (e.g. for filtering, synthesis, and visualization). JACK is well-tested and has a large user community.

JILL is under active development and is used in a wide range of experiments. Want to learn more? Check out the [wiki](https://github.com/dmeliza/jill/wiki).


