#!/usr/bin/env python3
"""Manages a triggered recording from a single audio channel using jill. Starts
jrecord and jdetect and wires them together, so that the recording opens when
the signal on the monitored channel exceeds a threshold and closes when it falls
quiet again. An arf file is generated with one entry per detected event.

The detector counts threshold crossings in short analysis periods and compares a
running total against a rate threshold, which biases it towards signals with
energy at high frequencies. The defaults are a starting point, not a
configuration: the open and close rates always need tuning against real signal
from the rig. Use --count-port to expose the detector's running estimate for
monitoring while you do that.

"""

import time
import datetime
import argparse
import signal
import subprocess
import logging
from pathlib import Path
import shutil

log = logging.getLogger("jrecord-triggered")  # root logger

__version__ = "2.2.2"


def terminate_on_signal(sig, frame):
    """Turn SIGTERM into the same shutdown path as ctrl-C.

    Without this, a SIGTERM from a supervisor or from `timeout` kills the
    script outright and leaves jrecord running with the arf file open, so the
    recording is neither stopped nor readable until someone finds the orphan.
    """
    raise KeyboardInterrupt


def setup_log(log, debug=False):
    ch = logging.StreamHandler()
    formatter = logging.Formatter("%(asctime)s : %(message)s")
    loglevel = logging.DEBUG if debug else logging.INFO
    log.setLevel(loglevel)
    ch.setLevel(loglevel)
    ch.setFormatter(formatter)
    log.addHandler(ch)


class ParseKeyVal(argparse.Action):
    """Custom action that parses arguments formed as key=value pair"""

    def parse_value(self, value):
        import ast

        try:
            return ast.literal_eval(value)
        except (ValueError, SyntaxError):
            return value

    def __call__(self, parser, namespace, arg, option_string=None):
        kv = getattr(namespace, self.dest)
        if kv is None:
            kv = dict()
        if not arg.count("=") == 1:
            raise ValueError(f"-k {arg} argument badly formed; needs key=value")
        else:
            key, val = arg.split("=")
            kv[key] = self.parse_value(val)
        setattr(namespace, self.dest, kv)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "-v", "--version", action="version", version="%(prog)s " + __version__
    )
    p.add_argument("--debug", help="show verbose log messages", action="store_true")
    p.add_argument(
        "-y",
        "--dry-run",
        action="store_true",
        help="print the commands but don't execute them",
    )
    p.add_argument(
        "--jill-path",
        type=Path,
        help="directory to search for jill binaries; otherwise uses $PATH",
    )

    p.add_argument(
        "--input",
        "-i",
        default="system:capture_1",
        help="name of the JACK port to monitor and record (default %(default)s)",
    )
    p.add_argument(
        "--output-dir",
        "-d",
        type=Path,
        default=Path.cwd(),
        help="directory to write the arf file to (default: the current directory)",
    )

    p.add_argument(
        "--animal",
        required=True,
        help="identifier (uuid) of the experimental subject (required)",
    )
    p.add_argument(
        "--experimenter", required=True, help="name of the experimenter (required)"
    )
    p.add_argument(
        "-k",
        help="specify additional metadata for the recording (use multiple -k for multiple fields).",
        action=ParseKeyVal,
        default=dict(),
        metavar="KEY=VALUE",
        dest="metadata",
    )

    rec = p.add_argument_group("recording options")
    rec.add_argument(
        "--pretrigger",
        type=float,
        default=1.0,
        help="how much signal before the onset to keep (s; default %(default).1f)",
    )
    rec.add_argument(
        "--posttrigger",
        type=float,
        default=0.5,
        help="how much signal after the offset to keep (s; default %(default).1f)",
    )
    rec.add_argument(
        "--buffer",
        type=float,
        default=2.0,
        help="ringbuffer size (s; must exceed pretrigger; default %(default).1f)",
    )
    rec.add_argument(
        "--compression",
        type=int,
        default=0,
        help="compression level for the arf file (0-9; default %(default)d)",
    )

    det = p.add_argument_group(
        "detector options (these need tuning against real signal)"
    )
    det.add_argument(
        "--period-size",
        type=float,
        default=20,
        help="analysis period (ms; default %(default)g)",
    )
    det.add_argument(
        "--open-thresh",
        type=float,
        default=0.01,
        help="sample threshold for opening the gate (0-1.0; default %(default)g)",
    )
    det.add_argument(
        "--open-rate",
        type=float,
        default=20,
        help="crossing rate that opens the gate (s^-1; default %(default)g)",
    )
    det.add_argument(
        "--open-period",
        type=float,
        default=500,
        help="how long a window the opening rate is measured over (ms; default %(default)g)",
    )
    det.add_argument(
        "--close-thresh",
        type=float,
        default=0.01,
        help="sample threshold for closing the gate (0-1.0; default %(default)g)",
    )
    det.add_argument(
        "--close-rate",
        type=float,
        default=2,
        help="crossing rate that closes the gate (s^-1; default %(default)g)",
    )
    det.add_argument(
        "--close-period",
        type=float,
        default=5000,
        help="how long a window the closing rate is measured over (ms; default %(default)g)",
    )
    det.add_argument(
        "--count-port",
        action="store_true",
        help="expose the detector's running estimate on a port, for tuning the rates",
    )

    args = p.parse_args()
    setup_log(log, args.debug)

    # Both windows are integrated in whole analysis periods, so a window shorter
    # than one period leaves the detector with nothing to integrate. jdetect
    # refuses to start in that case; catching it here names the option.
    for window in ("open_period", "close_period"):
        if getattr(args, window) < args.period_size:
            p.error(
                f"--{window.replace('_', '-')} must be at least one --period-size "
                f"({args.period_size:g} ms)"
            )
    if args.pretrigger >= args.buffer:
        p.error("--buffer must be longer than --pretrigger")

    log.info("checking for jill binaries:")
    binary_paths = {}
    for prog in ("jrecord", "jdetect"):
        path = shutil.which(prog, path=args.jill_path)
        if path is None:
            p.error(f"unable to find {prog}!")
        version_info = subprocess.check_output([path, "--version"])
        try:
            version = version_info.split()[1].decode("ascii")
        except ValueError:
            p.error(f"unable to determine version for {prog}")
        log.info(" - %s %s: %s", prog, version, path)
        binary_paths[prog] = path

    now = datetime.datetime.now()
    arf_out = args.output_dir / now.strftime(f"{args.animal}_%Y%m%d-%H%M%S.arf")

    additional_metadata = []
    for key, value in args.metadata.items():
        additional_metadata.extend(("-a", f"{key}={value}"))

    # jrecord is given a bare -t, which puts it in triggered mode and registers
    # trig_in without connecting anything to it. jdetect then connects itself
    # with -o. Doing it in this direction lets jrecord start first, so it is
    # already recording when the first trigger can arrive, and so its log
    # socket is bound before jdetect has anything to say.
    jrecord_args = [
        binary_paths["jrecord"],
        "-n",
        "jrecord",
        "-t",
        "-i",
        args.input,
        "--pretrigger",
        f"{args.pretrigger}",
        "--posttrigger",
        f"{args.posttrigger}",
        "--buffer",
        f"{args.buffer}",
        "--compression",
        f"{args.compression}",
        "-a",
        f"animal={args.animal}",
        "-a",
        f"experimenter={args.experimenter}",
        *additional_metadata,
        str(arf_out),
    ]
    log.debug(" ".join(jrecord_args))

    jdetect_args = [
        binary_paths["jdetect"],
        "-n",
        "jdetect",
        "-i",
        args.input,
        "-o",
        "jrecord:trig_in",
        "--period-size",
        f"{args.period_size:g}",
        "--open-thresh",
        f"{args.open_thresh:g}",
        "--open-rate",
        f"{args.open_rate:g}",
        "--open-period",
        f"{args.open_period:g}",
        "--close-thresh",
        f"{args.close_thresh:g}",
        "--close-rate",
        f"{args.close_rate:g}",
        "--close-period",
        f"{args.close_period:g}",
    ]
    if args.count_port:
        jdetect_args.append("--count-port")
    log.debug(" ".join(jdetect_args))

    if args.dry_run:
        log.info("dry run complete")
        p.exit()

    signal.signal(signal.SIGTERM, terminate_on_signal)

    jdetect_proc = None
    try:
        log.info("recording to %s", arf_out)
        log.info("starting jrecord:")
        jrecord_proc = subprocess.Popen(jrecord_args)
        # Give jrecord time to register trig_in; jdetect's -o fails if the port
        # is not there yet.
        time.sleep(1)
        if jrecord_proc.poll() is not None:
            p.error("jrecord exited during startup; see its output above")
        log.info("starting jdetect:")
        jdetect_proc = subprocess.Popen(jdetect_args)
        log.info("recording until interrupted (ctrl-C to stop)")
        jdetect_proc.wait()
    except KeyboardInterrupt:
        log.info("recording interrupted by user")
    finally:
        if jdetect_proc is not None:
            jdetect_proc.terminate()
        # Let the last trigger's posttrigger window drain into the file before
        # jrecord is asked to close it.
        time.sleep(args.posttrigger + 1)
        jrecord_proc.terminate()
        jrecord_proc.wait()
        log.info("wrote %s", arf_out)
