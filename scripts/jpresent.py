#!/usr/bin/env python3
"""Manages an auditory physiology experiment using jill. Starts jrecord,
 jclicker, and jstim, and makes sure they're all wired up properly.

"""
import time
import datetime
import argparse
import subprocess
import logging
from pathlib import Path
import shutil

log = logging.getLogger("jpresent")  # root logger

__version__ = "2.2.0"


def setup_log(log, debug=False):
    ch = logging.StreamHandler()
    formatter = logging.Formatter("%(asctime)s : %(message)s")
    loglevel = logging.DEBUG if debug else logging.INFO
    log.setLevel(loglevel)
    ch.setLevel(loglevel)
    ch.setFormatter(formatter)
    log.addHandler(ch)


def get_jill_binaries(jill_path):
    return binary_paths


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument(
        "-v", "--version", action="version", version="%(prog)s " + __version__
    )
    p.add_argument("--debug", help="show verbose log messages", action="store_true")
    p.add_argument(
        "--jill-path",
        type=Path,
        help="directory to search for jill binaries; otherwise uses $PATH",
    )

    p.add_argument(
        "--audio-out",
        default="system:playback_1",
        help="name of the JACK port where the stimulus audio should go (default %(default)s)",
    )
    p.add_argument(
        "--sync-out",
        default="system:playback_2",
        help="name of the JACK port where the synchronization signal should go (default %(default)s)",
    )
    p.add_argument(
        "--trig-out",
        default="system:playback_3",
        help="name of the JACK port where the trigger signal should go (default %(default)s)",
    )

    p.add_argument(
        "--trigger-before",
        type=float,
        default=1.0,
        help="time before stimulus onset to send a trigger on pulse",
    )

    p.add_argument(
        "--trigger-after",
        type=float,
        default=1.0,
        help="time after stimulus end to send a trigger off pulse",
    )

    p.add_argument(
        "--experimenter", required=True, help="name of the experimenter (required)"
    )
    p.add_argument(
        "--animal",
        required=True,
        help="identifier (uuid) of the experimental subject (required)",
    )

    p.add_argument(
        "--repeats",
        "-r",
        type=int,
        default=1,
        help="number of repetitions for each stimulus (default %(default)d))",
    )
    p.add_argument(
        "--gap",
        "-g",
        type=float,
        default=2.1,
        help="minimum time between end of one stimulus and start of the next (default %(default).1f s)",
    )
    p.add_argument(
        "jstim_args", nargs="+", help="additional arguments passed to jstim. Use '--' "
    )

    args = p.parse_args()
    print(args)
    setup_log(log, args.debug)

    if args.trigger_before + args.trigger_after >= args.gap:
        p.error(
            "pre- and post-stimulus trigger times must sum to less than gap between stimuli"
        )

    log.info("checking for jill binaries:")
    binary_paths = {}
    for prog in ("jrecord", "jclicker", "jstim"):
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
    arf_out = now.strftime("jpresent_%Y%m%d-%H%M%S.arf")
    jstim_args = [binary_paths["jstim"]]

    jrecord_args = (
        binary_paths["jrecord"],
        "-n",
        "jpresent",
        "-E",
        "jstim",
        "-a",
        f"experimenter={args.experimenter}",
        "-a",
        f"animal={args.animal}",
        arf_out,
    )
    jstim_args.extend(("-e", "jpresent:jstim"))
    log.debug(" ".join(jrecord_args))

    jclicker_sync_args = (
        binary_paths["jclicker"],
        "-n",
        "jclicker-sync",
        "-o",
        args.sync_out,
        "0x00,positive,1",
        "0x10,negative,1",
    )
    jstim_args.extend(("-e", "jclicker-sync:in"))
    log.debug(" ".join(jclicker_sync_args))

    log.info("starting jclicker for trigger events:")
    jclicker_trig_args = (
        binary_paths["jclicker"],
        "-n",
        "jclicker-trig",
        "-o",
        args.trig_out,
        "0x01,positive,1",
    )
    jstim_args.extend(
        (
            "-e",
            "jclicker-trig:in",
            "--trigger-before",
            f"{args.trigger_before}",
            "--trigger-after",
            f"{args.trigger_after}",
        )
    )
    log.debug(" ".join(jclicker_trig_args))

    jstim_args.extend(
        ("-o", args.audio_out, "--gap", f"{args.gap}", "--repeats", f"{args.repeats}")
    )
    jstim_args.extend(args.jstim_args)
    log.debug(" ".join(jstim_args))

    try:
        log.info("starting jrecord:")
        jrecord_proc = subprocess.Popen(jrecord_args)

        log.info("starting jclicker for sync events:")
        jsync_proc = subprocess.Popen(jclicker_sync_args)
        if args.trigger_before is not None:
            log.info("starting jclicker for trigger events:")
            jtrig_proc = subprocess.Popen(jclicker_trig_args)
        else:
            jtrig_proc = None
        log.info("starting jstim:")
        jstim_proc = subprocess.Popen(jstim_args)
        jstim_proc.wait()
    except KeyboardInterrupt:
        log.info("experiment interrupted by user")
        jstim_proc.terminate()
    finally:
        time.sleep(1)
        jsync_proc.terminate()
        if jtrig_proc is not None:
            jtrig_proc.terminate()
        time.sleep(1)
        jrecord_proc.terminate()
