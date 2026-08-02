# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Checks the ARF files jrecord writes against the layout in doc/arf-files.md.

test/write_arf_fixture.cc produces a file with known contents; everything here
reads it back with h5py and asserts its structure. Verifying with h5py rather
than the C++ API is deliberate: it is the library the lab's analysis code uses,
so these tests also confirm the recordings are readable by the tools that
consume them.
"""

import numpy as np
import pytest

h5py = pytest.importorskip("h5py", reason="h5py is needed to inspect ARF files")

from conftest import TEST_DIR, run_binary

# must match the constants in write_arf_fixture.cc
SAMPLING_RATE = 20000
PERIOD = 1024
PERIODS = 10
SOURCE = "fixture"
CHANNELS = ["pcm_000", "pcm_001"]
EVENT_CHANNEL = "trig_in"
STIMULUS_NAME = "stim_a"
STIM_ON = 0x00

pytestmark = pytest.mark.needs_arf


def expected_ramp(channel):
    """The waveform write_arf_fixture writes for one period of a channel."""
    sign = 1.0 if channel % 2 == 0 else -1.0
    return sign * (2.0 * np.arange(PERIOD, dtype=np.float32) / PERIOD - 1.0)


@pytest.fixture(scope="module")
def arf_file(tmp_path_factory):
    """Generate a fixture ARF file and open it read-only."""
    if not (TEST_DIR / "write_arf_fixture").exists():
        pytest.skip("write_arf_fixture was not built (scons --no-arf?)")
    path = tmp_path_factory.mktemp("arf") / "fixture.arf"
    result = run_binary("write_arf_fixture", timeout=120, args=(str(path),))
    assert result.returncode == 0, (
        "write_arf_fixture exited %d\n--- output ---\n%s%s"
        % (result.returncode, result.stdout, result.stderr)
    )
    assert path.exists(), "no output file was written"
    with h5py.File(path, "r") as f:
        yield f


@pytest.fixture(scope="module")
def entries(arf_file):
    """The entry groups, in name order."""
    return [arf_file[n] for n in sorted(arf_file) if isinstance(arf_file[n], h5py.Group)]


def test_file_is_arf(arf_file):
    assert "arf_version" in arf_file.attrs
    assert arf_file.attrs["file_creator"].decode().startswith("org.meliza.jill")


def test_log_dataset(arf_file):
    """doc/arf-files.md: /jill_log holds log messages."""
    assert "jill_log" in arf_file
    log = arf_file["jill_log"]
    assert set(log.dtype.names) == {"sec", "usec", "message"}
    assert len(log) >= 1
    messages = [row["message"].decode() for row in log]
    assert any("a log message" in m for m in messages)
    assert any("[fixture]" in m for m in messages), "the source should be recorded"


def test_entries_are_numbered_sequentially(arf_file, entries):
    """Entries are <source>_0000, _0001, ... in creation order."""
    names = [n for n in sorted(arf_file) if isinstance(arf_file[n], h5py.Group)]
    assert names == ["%s_%04d" % (SOURCE, i) for i in range(len(names))]
    # four, because the frame counter wrap splits the last one
    assert len(entries) == 4


@pytest.mark.parametrize("attr", ["timestamp", "jack_frame", "jack_sampling_rate",
                                  "entry_creator"])
def test_documented_entry_attributes_present(entries, attr):
    for entry in entries:
        assert attr in entry.attrs, "%s missing from %s" % (attr, entry.name)


def test_entry_attribute_values(entries):
    entry = entries[0]
    assert len(entry.attrs["timestamp"]) == 2, "unix seconds and microseconds"
    assert entry.attrs["timestamp"][0] > 1_600_000_000, "a plausible unix time"
    assert entry.attrs["jack_frame"] == 1000
    assert entry.attrs["jack_sampling_rate"] == SAMPLING_RATE
    assert entry.attrs["entry_creator"].decode().startswith("org.meliza.jill")


def test_user_attributes_are_recorded(entries):
    """Attributes passed to the writer land on every entry."""
    for entry in entries:
        assert entry.attrs["experimenter"].decode() == "dmeliza"
        assert entry.attrs["experiment"].decode() == "arf format fixture"


def test_sampled_datasets(entries):
    """One dataset per channel, 32-bit float, one sample per frame."""
    entry = entries[0]
    for channel in CHANNELS:
        assert channel in entry
        dset = entry[channel]
        assert dset.dtype == np.float32
        assert dset.shape == (PERIODS * PERIOD,)
        assert dset.attrs["sampling_rate"] == SAMPLING_RATE
        assert "units" in dset.attrs


def test_sampled_values_survive_the_round_trip(entries):
    """The samples read back are the ones that were written."""
    entry = entries[0]
    for i, channel in enumerate(CHANNELS):
        data = entry[channel][:]
        np.testing.assert_allclose(data[:PERIOD], expected_ramp(i), rtol=0, atol=1e-6)
        # the same period is written repeatedly
        np.testing.assert_allclose(data[PERIOD:2 * PERIOD], expected_ramp(i),
                                   rtol=0, atol=1e-6)


def test_samples_stay_in_range(entries):
    """doc/arf-files.md: sampled values are bounded between -1.0 and 1.0."""
    for entry in entries:
        for channel in CHANNELS:
            if channel not in entry:
                continue
            data = entry[channel][:]
            assert data.min() >= -1.0
            assert data.max() <= 1.0


def test_channels_are_distinguishable(entries):
    """The two channels carry different data, not a copy of one another."""
    entry = entries[0]
    a = entry[CHANNELS[0]][:PERIOD]
    b = entry[CHANNELS[1]][:PERIOD]
    np.testing.assert_allclose(b, -a, rtol=0, atol=1e-6)


def test_channel_uuids_are_stable_across_entries(entries):
    """A channel keeps its uuid, so it can be followed through a session."""
    for channel in CHANNELS:
        uuids = {e[channel].attrs["uuid"].tobytes() for e in entries if channel in e}
        assert len(uuids) == 1, "%s changed uuid between entries" % channel


def test_event_dataset(entries):
    """doc/arf-files.md: event data uses a start/status/message compound type."""
    entry = entries[0]
    assert EVENT_CHANNEL in entry
    events = entry[EVENT_CHANNEL]
    assert set(events.dtype.names) == {"start", "status", "message"}
    assert len(events) == 1

    row = events[0]
    # start is relative to the beginning of the entry, not the jack frame count
    assert row["start"] == 2 * PERIOD
    assert row["status"] == STIM_ON
    # a stimulus name is passed through as utf-8, not hex encoded
    assert row["message"].decode() == STIMULUS_NAME


def test_xrun_is_recorded(entries):
    """An xrun marks the entry it happened in, and only that one."""
    flagged = [e for e in entries if "jill_error" in e.attrs]
    assert len(flagged) == 1
    assert flagged[0].name.endswith("_0001")
    assert "xrun" in flagged[0].attrs["jill_error"].decode()


def test_frame_counter_wrap_splits_the_entry(entries):
    """The 32-bit frame counter wraps about every 24 hours at 48 kHz.

    The writer is expected to notice the count going backwards and start a new
    entry rather than write nonsense offsets, which is what lets a recording
    run for days. The fixture writes six periods across the wrap.
    """
    before, after = entries[2], entries[3]
    assert before.attrs["jack_frame"] > 2 ** 31, "should start just below the wrap"
    assert after.attrs["jack_frame"] < 2 ** 31, "should resume after wrapping"

    # no samples are lost in the split
    total = len(before[CHANNELS[0]]) + len(after[CHANNELS[0]])
    assert total == 6 * PERIOD
