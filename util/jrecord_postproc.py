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
import ctypes
import ctypes.util

__version__ = '2.1.0-SNAPSHOT'

# this is borrowed from pylibsamplerate, by Timothe Faudot, licensed under GPL.
# I would add it as an external dependency, but it's not on PyPi.
_sr_lib = ctypes.CDLL(ctypes.util.find_library('samplerate'))


class CONVERTERS():
    SRC_SINC_BEST_QUALITY		= 0
    SRC_SINC_MEDIUM_QUALITY		= 1
    SRC_SINC_FASTEST			= 2
    SRC_ZERO_ORDER_HOLD			= 3
    SRC_LINEAR				= 4


class _SRC_DATA(ctypes.Structure):
    """
    data_in       : A pointer to the input data samples.
    input_frames  : The number of frames of data pointed to by data_in.
    data_out      : A pointer to the output data samples.
    output_frames : Maximum number of frames pointer to by data_out.
    src_ratio     : Equal to output_sample_rate / input_sample_rate.
    """
    _fields_ = [("data_in", ctypes.POINTER(ctypes.c_float)),
                ("data_out", ctypes.POINTER(ctypes.c_float)),
                ("input_frames", ctypes.c_long),
                ("output_frames", ctypes.c_long),
                ("input_frames_used", ctypes.c_long),
                ("output_frames_gen", ctypes.c_long),
                ("end_of_input", ctypes.c_int),
                ("src_ratio", ctypes.c_double),
               ]


#dll methods declarations
def _initLibMethods():
    _sr_lib.src_get_name.restype = ctypes.c_char_p
    _sr_lib.src_get_name.argtypes = [ctypes.c_int]
    _sr_lib.src_get_description.restype = ctypes.c_char_p
    _sr_lib.src_get_description.argtypes = [ctypes.c_int]
    _sr_lib.src_get_version.restype = ctypes.c_char_p
    _sr_lib.src_get_version.argtypes = None

    #simple API:
    #int src_simple (SRC_DATA *data, int converter_type, int channels) ;
    _sr_lib.src_simple.restype = ctypes.c_int
    _sr_lib.src_simple.argtypes = [ctypes.POINTER(_SRC_DATA), ctypes.c_int, ctypes.c_int]
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
    src_data = _SRC_DATA(data_in=data_f.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
                         data_out=out.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
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

    entry - the h5py group with recording and trigger data
    recording_dset - the name of the dataset with the recording
    trigger_dset - the name of the dataset with the trigger (and stimulus information)
    match_dur - the number of seconds on either end of the stimulus to use

    """
    from distutils.version import LooseVersion
    # from numpy import correlate

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
    #onset_corr = xcorr[nsamples / 2:].argmax()
    onset_corr = xcorr[:nsamples / 2].argmax()

    if offset is not None:
        xcorr = correlate(recording[offset - nsamples:offset + nsamples], stimulus[-nsamples:], 'same')
        #offset_corr = xcorr[nsamples / 2:].argmax()
        offset_corr = xcorr[:nsamples / 2].argmax()
    else:
        offset_corr = None

    return entry.name, stimname, ds, onset_corr, offset_corr


def log_msg(dset, msg):
    """Write a log message to dset"""
    import time
    t = time.time()
    sec = long(t)
    usec = long((t - sec) * 1000000)
    dset.resize(dset.shape[0] + 1, axis=0)
    dset[-1] = (sec, usec, "[jrecord_postproc] " + msg)
    print "#", msg


def ynprompt(question):
    while True:
        res = raw_input(question + " [y/N] ")
        if res in ('', 'n', 'N'):
            return False
        elif res in ('y', 'Y'):
            return True


def correlate(ref, tgt, mode):
    from numpy import conj
    from numpy.fft import fft, ifft
    # could cache some of these ffts
    X = fft(ref)
    X *= conj(fft(tgt, X.size))
    return ifft(X).real


def main():

    import argparse
    p = argparse.ArgumentParser(prog="jrecord_postproc",
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
    p.add_argument("-m", "--max-jitter", help="""maximum allowable jitter in
    estimated delay (default=%(default)d samples). If delay
    estimate deviates from the mean by more than this value, an
    error will be generated and the entry will not be
    modified""", type=int, default=2)
    p.add_argument("-w", "--stim-window", help="""the amount of time (default
    %(default).1f s) from the stimulus to use for calculating
    delays""", default=1.0, type=float)
    p.add_argument("-y", "--update", help="update trigger times without asking", action='store_true')

    options = p.parse_args()
    stimstats = []
    stimcache = stimulus_cache(options.stimdir)

    with h5py.File(options.arffile, 'a') as afp:
        # open log dataset

        # pass 1: calculate onset and offset delays
        print "entry\tstim\tsampling_rate\td.offset\td.onset"
        # TODO process entries in parallel
        for entry in afp.itervalues():
            if not isinstance(entry, h5py.Group):
                continue
            if 'jill_error' in entry.attrs:
                log_msg(log, "skipping %s: flagged with error '%s'" % (entry.name, entry.attrs['jill_error']))
                continue
            stats = estimate_times(entry, options.recording, options.trigger, stimcache)
            print "{}\t{}\t{}\t{}\t{}".format(*stats)
            stimstats.append(stats)

        # pass 2: calculate statistics of onset delay and flag entries that
        # aren't good
        stimstats = nx.rec.fromrecords(stimstats, names=('entry', 'stim', 'ds', 'onset', 'offset'))
        # average onset and offset delay
        mean_delay = sum(1000. / r['ds'] * (r['onset'] + r['offset']) / 2 for r in stimstats) / stimstats.size
        print "\nAverage delay = %.3f ms" % mean_delay

        if not options.update:
            if not ynprompt('Update %s datasets to reflect delay?' % (options.trigger)):
                return

        log = afp['jill_log']
        log_msg(log, "version: " + __version__)
        log_msg(log, "average delay = %.3f ms" % mean_delay)

        # pass 3: adjust trigger dataset times (or create new datasets?). Delete
        # recordings.
        for entry, stimname, ds, onset, offset in stimstats:
            # round down to sample boundary
            adj = (onset + offset) / 2
            log_msg(log, "%s: delay = %d samples (%.3f ms)" % (entry, adj, 1000. / ds * adj))
            if (offset - onset) > options.max_jitter:
                log_msg(log, "%s: onset and offset delay differ by more than %d samples" %
                        (entry, options.max_jitter))
                # flag entry
                afp[entry].attrs['jill_error'] = 'possible stimulus distortion'
            elif abs(adj - int(mean_delay * ds / 1000)) > options.max_jitter:
                log_msg(log, "%s: delay differs from mean by more than %d samples" %
                        (entry, options.max_jitter))
                # flag entry
                afp[entry].attrs['jill_error'] = 'possible stimulus distortion'
            else:
                trig = afp[entry][options.trigger]
                for i,r in enumerate(trig):
                    if r['message'] == stimname:
                        log_msg(log, "%s[%d]: adjusted time %d -> %d" %
                                (trig.name, i, r['start'], r['start'] + adj))
                        r['start'] += adj
                        trig[i] = r

if __name__ == "__main__":
    main()

# Variables:
# End:
