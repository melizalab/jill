
# Implementation notes

This section is for notes describing design considerations that involve functions and classes throughout the JILL library.

## Signal detection

The signal detection algorithm used by `jdetect` is fairly straightforward. The problem is to calculate a running average of the signal power and compare it against a threshold.

The power is estimated by counting the number of times the signal crosses a threshold. This estimator is strongly biased toward signals that contain high frequencies, because high-frequency signals will cross a threshold more often than a low-frequency signal of equal power. This bias is useful for detecting bird songs, as these typically have lots of power at high frequencies.  The `jill::dsp::crossing_counter` class implements this part of the algorithm.  It uses `jill::dsp::running_counter` to calculate the running average.

The algorithm needs to detect when the signal starts and when it ends, and for many kinds of signals it's desirable for these detectors to have different sensitivities. Typically the onset detector needs to be more sensitive than the offset detector so that short gaps aren't registered as offsets.  The `jill::dsp::crossing_trigger` class encapsulates two crossing_counters and manages the logic of switching between opened and closed states.

## Stimulus playback

The `jstim` module loads audio waveforms from files and plays them in sequence or in random order. The minimum interval between stimulus onsets and the minimum gap between stimulus offsets and onsets can be specified.

Stimuli are loaded from disk using the libsndfile library and resampled (if needed to match the sampling rate of the JACK engine) using libsamplerate. The `jill::file::stimfile` class encapsulates all this logic.

Resampling can be a time-consuming operation and we don't want the user to have to wait while all the stimuli are loaded and resampled. This is avoided by using a readahead queue. The idea of the queue is that while a consumer thread is reading from a buffer of samples, a background thread is loading the next file and resampling it.  `jill::util::stimqueue` defines the interface for an object that can do this, and `jill::util::readahead_queue` is the implementation.

The consumer calls the `stimqueue::head()` function to access the samples for the stimulus at the head of the queue. If data is not available, the function returns a null pointer and the consumer goes and twiddles its thumbs for a while (in `jstim`, until the next process loop).  When the consumer is done with the stimulus, it calls `stimqueue::release()`, which notifies the background thread to do some work. If the background thread is through loading the next stimulus, it moves it into the head of the queue and starts work on the next stimulus in line. If not, it finishes loading the stimulus that should go at the head.

Synchronization in `jill::util::readahead_queue` is maintained using a mutex and a condition variable (from pthreads).  The background thread locks the mutex and then waits on a condition variable when it's not working (which releases the lock). `head()` and `release()` are called by the RT thread and need to be wait-free, so they use a trylock call when attempting to signal the condition variable.

Some tricky logic is also present in the RT loop itself, for determining when to start and stop the playback. See the comment for `process()` in `jstim.cc`

## Buffered recording

Recording data to disk, like any operation that involves potentially slow operations, requires a wait-free mechanism for the realtime processing thread to send data to a thread that doesn't have such stringent time constraints and can run at lower priority.

The interface for this data movement pattern is defined in `jill::data_thread`. The producer thread calls `push()` to send periods of data to the slower thread. The interface also defines methods for starting and stopping the consumer thread, for signalling the thread to split the data stream into multiple entries, and for signalling the thread that an overrun has occurred elswhere in the JACK system.

JILL provides two implementations for `data_thread`. `jill::dsp::buffered_data_writer` supports continuous writing, and `jill::dsp::triggered_data_writer` (which derives from `buffered_data_writer`) supports writing that's triggered by events in one of the data channels. These implementations use a mutex/condition variable mechanism for the producer thread to signal to the consumer thread that data has been added to the ringbuffer.  Additional notes on the threading and synchronization are in `buffered_data_writer.cc`.

Data are moved between the producer thread (which runs with realtime priority) and the consumer thread using a ringbuffer. Although JACK comes with a ringbuffer class, JILL provides its own template-based implementation to provide additional type safety.  The important behavior of a ringbuffer is that reads or writes near the end of the buffer wrap around to the beginning.  The JILL ringbuffer (`jill::dsp::ringbuffer`) uses mirrored memory, in which a single region of memory is mapped to two contiguous locations in virtual address space. Thus, when reads or writes overrun the end of the lower end of the buffer, they wrap to the upper end. See `jill::dsp::mirrored_memory` for this logic.

Data are serialized into the ringbuffer as blocks consisting of a header followed by an array of data. The header is defined by the `jill::data_block_t` type.  For sampled data, blocks correspond to periods; for event data, blocks correspond to individual events.  All the blocks from a period are stored in sequence in a single ringbuffer (multiple ringbuffers would require synchronization and would not be lock-free).  The `jill::dsp::block_ringbuffer` class derives from `ringbuffer` and contains additional logic for moving data into and out of the ringbuffer in blocks.

In the `buffered_data_writer` classes, the consumer thread sends data it pulls off the ringbuffer to a `jill::data_writer` object. `data_writer` is an interface that specifies methods for writing blocks of data to a file (or other sink), and for splitting the data stream into multiple entries.  One implementation is provided, `jill::file::arf_writer`, which stores data in an HDF5 file in ARF format.

## Logging

Log messages are generated using a fairly simple stream-based approach. The `jill::log_msg` class encapsulates a stringstream object. On construction, the time of the message is saved. On destruction, messages are passed to a singleton logger object, which outputs the timestamp and message to the console.

A major design goal in JILL 2.1 was for log messages from all JILL modules to be stored in a single location. This requires some mechanism for interprocess communication. We use zeromq (http://zeromq.org), a robust and flexible socket library. In addition to being written to the console, messages are also written to a DEALER socket, which provides asynchronous communication.  Multiple DEALER sockets can be connected to a single endpoint, which does not need to have a bound server. Messages sent before the server binds will be queued. The `buffered_data_writer` class attempts to bind to this endpoint using a ROUTER socket.  When the consumer thread in this class isn't processing data from the ringbuffer, it reads messages from the socket and sends them to its `data_writer` object.
