# Basic signal processing

This section introduces some basic concepts in digital signal processing.

1. Physical quantities like voltage, sound pressure, and time are continuous (or analog), but most modern computers are digital, operating on discrete quantities.

2. Analog values can be converted to digital values by *sampling*. Sampling occurs at discrete time points, and sampled values are limited in range and precision. For example, a 16-bit integer can represent only 65536 distinct values.

3. Analog, time-varying signals are usually sampled at fixed intervals. The reciprocal of the interval duration is the *sampling rate*. The maximum frequency that can be carried by a digitized signal is half the sampling rate (also called the Nyquist frequency).

4. Sampling rates are regulated by clocks. No two clocks are identical.

5. Digital values can be converted to analog signals. This process is also governed by sampling rate and precision.

6. A digital system is *real-time* if it processes data at the same rate as it occurs in physical reality. If it fails to keep up with this rate, data will be lost (from inputs) or distorted (on outputs).

7. Most desktop computers and operating systems implement real-time behavior through *interrupts* and *buffering*. On the input side, data is sampled and stored in a buffer. When the buffer is full, an interrupt notifies the computer to stop what it's doing and handle the data. Buffers introduce latency equal to the number of samples in the buffer times the sampling rate.

8. The data in the buffer need to be processed before the next interrupt occurs, or data will be lost. An *overrun* occurs when the process taking data out of the buffer fails to keep up with the process putting data in. An *underrun* occurs when the input process fails to keep up with the output process. Most desktop computers are doing many things at once, and may not be able to handle interrupts in a timely fashion. Larger buffers protect against this problem, but at the cost of more latency.

9. JACK operates on a similar principle, though at a much higher level of abstraction. JACK clients can receive and send data through *ports*. Data are processed in blocks of samples called *periods*. When a period has elapsed, each client receives a block of data for each of its input ports, and has the opportunity to write data to its output ports. All of the clients have to finish processing their data before the period ends.

10. JACK input ports can be connected to output ports, and the data written to an output will be passed to the inputs of all the connected clients. JACK provides input and/or output ports that correspond to the hardware inputs and outputs.
