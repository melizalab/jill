#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.13"
# dependencies = [
#     "requests>=2.34.2",
# ]
# ///
"""Manages an auditory physiology electrophysiology experiment using jill and
open-ephys. Starts jrecord, jclicker, jrelay, and jstim, and makes sure they're all
wired up properly. Each stimulus will be presented in turn, generating a
synchronization pulse and sending a message with the stimulus name to open-ephys's
NetworkEvent plugin. A biphasic pulse can optionally be sent to a second channel, for
example to trigger an optogenetic light pulse. An arf file is generated with a log of
the stimuli.

"""

import time
import datetime
import argparse
import subprocess
import logging
from pathlib import Path
import shutil
import requests as rq

log = logging.getLogger("jpresent")  # root logger

__version__ = "2.2.2"


def check_oe_processors(processors):
    log.debug(" - checking for broadcast option in Network Events node")
    for processor in processors:
        if processor["name"] == "Network Events":
            for parameter in processor["parameters"]:
                if parameter["name"] == "broadcast_all_messages" and parameter[
                    "value"
                ] in (1, "1"):
                    return  # Ok
            raise RuntimeError("broadcast is not set to ON in Network Events plugin")
    raise RuntimeError("networkEvents plugin not detected in the signal chain")


def setup_log(log, debug=False):
    ch = logging.StreamHandler()
    formatter = logging.Formatter("%(asctime)s : %(message)s")
    loglevel = logging.DEBUG if debug else logging.INFO
    log.setLevel(loglevel)
    ch.setLevel(loglevel)
    ch.setFormatter(formatter)
    log.addHandler(ch)


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
        "--audio-out",
        default="system:playback_1",
        help="name of the JACK port where the stimulus audio should go (default %(default)s)",
    )
    p.add_argument(
        "--sync-out",
        default="system:playback_3",
        help="name of the JACK port where the synchronization signal should go (default %(default)s)",
    )
    p.add_argument(
        "--trig-out",
        default="system:playback_4",
        help="name of the JACK port where the trigger signal should go (default %(default)s)",
    )

    p.add_argument(
        "--trig-duration",
        type=float,
        default=50,
        help="duration of the trigger pulse (in ms; default %(default)d)",
    )

    p.add_argument(
        "--trig-prob",
        type=float,
        default=0.5,
        help="probability of the trigger pulse (0-1; default %(default)f)",
    )

    # TODO make this trigger delay
    # p.add_argument(
    #     "--trigger-after",
    #     type=float,
    #     default=1.0,
    #     help="time after stimulus end to send a trigger off pulse",
    # )
    p.add_argument(
        "--open-ephys",
        help="specify the network address where open-ephys is running",
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
    setup_log(log, args.debug)

    if args.open_ephys:
        oe_url = f"http://{args.open_ephys}:37497/api"
        log.info("checking for a running open-ephys at %s", oe_url)
        oe_session = rq.Session()
        try:
            r = oe_session.get(f"{oe_url}/status")
            r.raise_for_status()
            reply = r.json()
            if reply["mode"] != "IDLE":
                raise RuntimeError(
                    "open-ephys is already acquiring data. Stop it and try again."
                )
            r = oe_session.get(f"{oe_url}/processors")
            r.raise_for_status()
            reply = r.json()
            check_oe_processors(reply["processors"])
        except rq.exceptions.ConnectionError:
            log.error(" - error: unable to connect to open-ephys. Is it running?")
            p.exit(-1)
        except KeyError:
            log.error(" - error: unable to parse reply from open-ephys: %s", reply)
            p.exit(-1)
        except RuntimeError as err:
            log.error(" - error: %s", err)
            p.exit(-1)

    log.info("checking for jill binaries:")
    binary_paths = {}
    for prog in ("jrecord", "jclicker", "jrelay", "jstim"):
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
        "0x10,positive,1",
        "0x00,negative,1",
    )
    jstim_args.extend(("-e", "jclicker-sync:in"))
    log.debug(" ".join(jclicker_sync_args))

    if args.trig_out is not None:
        jclicker_trig_args = (
            binary_paths["jclicker"],
            "-n",
            "jclicker-trig",
            "-o",
            args.trig_out,
            "--prob",
            f"{args.trig_prob}",
            f"0x11,biphasic,{args.trig_duration}",
        )
        jstim_args.extend(
            (
                "-e",
                "jclicker-trig:in",
            )
        )
        log.debug(" ".join(jclicker_trig_args))

    jrelay_args = [binary_paths["jrelay"]]
    if args.open_ephys:
        jrelay_args.extend(("--open-ephys", f"tcp://{args.open_ephys}:5556"))
    log.debug(" ".join(jrelay_args))
    jstim_args.extend(("-e", "jrelay:in"))

    jstim_args.extend(
        ("-o", args.audio_out, "--gap", f"{args.gap}", "--repeats", f"{args.repeats}")
    )
    jstim_args.extend(args.jstim_args)
    log.debug(" ".join(jstim_args))

    if args.dry_run:
        log.info("dry run complete")
        p.exit()

    try:
        log.info("starting jrecord:")
        jrecord_proc = subprocess.Popen(jrecord_args)
        log.info("starting jclicker for sync events:")
        jsync_proc = subprocess.Popen(jclicker_sync_args)
        if args.trig_out is not None:
            log.info("starting jclicker for trigger events:")
            jtrig_proc = subprocess.Popen(jclicker_trig_args)
        else:
            jtrig_proc = None
        log.info("starting jrelay:")
        jrelay_proc = subprocess.Popen(jrelay_args)
        time.sleep(1)
        log.info("starting jstim:")
        jstim_proc = subprocess.Popen(jstim_args)
        jstim_proc.wait()
    except KeyboardInterrupt:
        log.info("experiment interrupted by user")
        jstim_proc.terminate()
    finally:
        time.sleep(1)
        jsync_proc.terminate()
        jrelay_proc.terminate()
        if jtrig_proc is not None:
            jtrig_proc.terminate()
        time.sleep(1)
        jrecord_proc.terminate()
