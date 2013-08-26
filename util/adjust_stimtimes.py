# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Script to adjust stimulus playback times

Copyright (C) 2013 Dan Meliza <dmeliza@gmail.com>
Created Mon Aug 26 09:12:05 2013
"""
import os
import numpy as nx
import ewave
import h5py

# this is borrowed from pylibsamplerate, by Timothe Faudot, licensed under GPL.
# I would add it as an external dependency, but it's not on PyPi.
import ctypes as ct
_sr_lib = ct.CDLL(ct.util.find_library('samplerate'))


class CONVERTERS():
    SRC_SINC_BEST_QUALITY		= 0
    SRC_SINC_MEDIUM_QUALITY		= 1
    SRC_SINC_FASTEST			= 2
    SRC_ZERO_ORDER_HOLD			= 3
    SRC_LINEAR				= 4


class _SRC_DATA(ct.Structure):
    """
    data_in       : A pointer to the input data samples.
    input_frames  : The number of frames of data pointed to by data_in.
    data_out      : A pointer to the output data samples.
    output_frames : Maximum number of frames pointer to by data_out.
    src_ratio     : Equal to output_sample_rate / input_sample_rate.
    """
    _fields_ = [("data_in", ct.POINTER(ct.c_float)),
                ("data_out", ct.POINTER(ct.c_float)),
                ("input_frames", ct.c_long),
                ("output_frames", ct.c_long),
                ("input_frames_used", ct.c_long),
                ("output_frames_gen", ct.c_long),
                ("end_of_input", ct.c_int),
                ("src_ratio", ct.c_double),
               ]


#dll methods declarations
def _initLibMethods():
    _sr_lib.src_get_name.restype = ct.c_char_p
    _sr_lib.src_get_name.argtypes = [ct.c_int]
    _sr_lib.src_get_description.restype = ct.c_char_p
    _sr_lib.src_get_description.argtypes = [ct.c_int]
    _sr_lib.src_get_version.restype = ct.c_char_p
    _sr_lib.src_get_version.argtypes = None

    #simple API:
    #int src_simple (SRC_DATA *data, int converter_type, int channels) ;
    _sr_lib.src_simple.restype = ct.c_int
    _sr_lib.src_simple.argtypes = [ct.POINTER(_SRC_DATA), ct.c_int, ct.c_int]
_initLibMethods()


def src_simple(data, ratio, out=None, converter=CONVERTERS.SRC_SINC_MEDIUM_QUALITY):
    """
    Performs 'simple' conversion of input. (correspond to the 'simple'
                                            API of SRC)
    Parameters:
    data        : a numpy array to convert, if the data is not of dtype
                  numpy.float32, then a local copy will be created
                  as SRC handles only float input at the moment
    ratio       : Equal to output_sample_rate / input_sample_rate
    out         : a numpy array in which to store the resampled data,
                  if None, a new array is created
    converter   : a CONVERTERS value

    returns:
    out         : the converted data with the dtype numpy.float32
    """
    import numpy as np
    nbChannels = data.size/data.shape[0]
    if out == None:
        out = np.empty((np.ceil(max(data.shape) * ratio).astype(int), nbChannels),
            dtype=np.float32)
    data_f = data
    if data.dtype!=np.float32:
        data_f = data.astype(np.float32)
    src_data = _SRC_DATA(data_in=data_f.ctypes.data_as(ct.POINTER(ct.c_float)),
                         data_out=out.ctypes.data_as(ct.POINTER(ct.c_float)),
                         input_frames=len(data),
                         output_frames=len(out),
                         src_ratio=ratio)
    _sr_lib.src_simple(src_data, converter, nbChannels)
    return out.squeeze()


class stimulus_cache(dict):

    def __init__(self, dir):
        self.dir = dir

    def __missing__(self, key):
        fname, fs = key
        fp = ewave.open(os.path.join(self.dir, fname + ".wav"))
        data = fp.read()
        if fp.sampling_rate != fs:
            data = src_simple(data, 1.0 * fs / fp.sampling_rate)
        ret = self[key] = data
        return ret


def jrecord_stimulus_times(dset):
    """Get stimulus id, onset, and offset from a trig_in dataset."""
    stim = onset = offset = None
    for row in dset:
        # only the upper 4 bits are used to indicate status; lower 4 are for channel
        status = row['status'] & 0xf0
        if status == 0x00:
            # stim_on, non-standard midi event
            stim = str(row['message'])
            onset = row['start']
        elif status == 0x10 and onset is not None:
            # stim_off, non-standard midi event. only process if we've got the
            # first stimulus onset
            assert str(row['message']) == stim
            offset = row['start']
        elif status == 0x80:
            # note_on
            onset = row['start']
        elif status == 0x90 and onset is not None:
            # note_off
            assert stim is None
            offset = row['start']
        if onset is not None and offset is not None:
            break
    return stim, onset, offset


def estimate_times(entry, recording_dset, trigger_dset, stimcache, match_dur=1.0):
    """Estimate onset and offset times of a stimulus in a recording.


    """
    from distutils.version import LooseVersion
    from numpy import correlate

    rec = entry[recording_dset]
    ds = rec.attrs['sampling_rate']
    nsamples = int(ds * match_dur)

    trig = entry[trigger_dset]
    assert trig.attrs['sampling_rate'] == ds
    stimname, onset, offset = jrecord_stimulus_times(trig)
    if onset is None:
        # couldn't find an onset:
        return None, None, None
    if stimname is None:
        raise NotImplementedError("%s doesn't have stimulus identity information" % trigger_dset)

    recording = rec[:]
    stimulus = stimcache[stimname, ds]

    # given the stimulus onset we can assume that the first xcorr peak after
    # the onset is the correct one.
    xcorr = correlate(recording[onset:onset + nsamples], stimulus[:nsamples], 'same')
    onset_corr = xcorr[nsamples / 2:].argmax()

    if offset is not None:
        # pre-2.1 jill seems to have consistent errors in the time of the
        # trigger offset, so for these files we use the length of the stimulus
        # as a hint
        try:
            prog, ver = entry.attrs['entry_creator'].split()
            assert LooseVersion(ver) >= '2.1'
        except (AssertionError, IndexError, KeyError):
            offset = onset + stimulus.size

        xcorr = correlate(recording[offset - nsamples:offset + nsamples], stimulus[-nsamples:], 'same')
        offset_corr = xcorr[nsamples / 2:].argmax()
    else:
        offset_corr = None

    return entry.name, stimname, onset_corr, offset_corr


if __name__ == "__main__":

    import argparse
    p = argparse.ArgumentParser(prog="adjust_stimtimes",
                                description=""""Correct stimulus onset times for playback delay. To use this program, you
                                must record the stimulus as it's being played,
                                either through a microphone or a cable. This
                                recording is cross-correlated with the original
                                stimulus file, and the lag is used to adjust the
                                stimulus onset times. """,)

    p.add_argument("arffile", help="file to check and modify. Must be an ARF file created by jrecord")
    p.add_argument("stimdir", help="directory containing the stimulus files")
    p.add_argument("-r", "--recording", help="name of the dataset containing recordings of the presented stimulus",
                   required=True)
    p.add_argument("-t", "--trigger", help="name of the dataset containing the trigger times and stimulus names.",
                   default='trig_in')
    p.add_argument("-m", "--max-jitter", help="""maximum allowable jitter in estimated delay (default=%(default)s
    samples). If delay estimate exceeds this value, an error will be generated and the entry
    will not be modified""",
                   type=int, default=2)
    p.add_argument("-w", "--stim-window",
                   help="the amount of time (default %(default).1f s) from the stimulus to use for calculating delays",
                   default=1.0,
                   type=float)
    p.add_argument("-y", "--update", help="update trigger times", action='store_true')

    options = p.parse_args()
    events = []
    stimcache = stimulus_cache(options.stimdir)

    with h5py.File(options.arffile, 'a') as afp:
        # open log dataset

        # pass 1: calculate onset and offset delays
        print "# calculating onset and offset delays"
        print "entry\tstim\td.offset\td.onset"
        # TODO process entries in parallel
        for entry in afp.itervalues():
            if not isinstance(entry, h5py.Group):
                continue
            stats = estimate_times(entry, options.recording, options.trigger, stimcache)
            print "{}\t{}\t{}\t{}".format(*stats)
            events.append(stats)

        # pass 2: calculate statistics of onset delay and flag entries that
        # aren't good
        events = nx.rec.fromrecords(events, names=('entry', 'stim', 'onset', 'offset'))


        # pass 3: adjust trigger dataset times (or create new datasets?). Delete
        # recordings.

# Variables:
# End:
