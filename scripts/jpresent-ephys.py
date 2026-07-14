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

import os
import time
import datetime
import argparse
import subprocess
import logging
from pathlib import Path
import shutil
import requests as rq
import json

log = logging.getLogger("jpresent")  # root logger

__version__ = "2.2.2"

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
    
# The open-ephys-python-tools library is pretty broken, so we're just rolling our own calls

class OpenEphysControl:

    def __init__(self, hostname: str):
        self.url = f"http://{hostname}:37497/api"
        self.session = rq.Session()

    def send(self, endpoint: str, data: dict | None = None):
        url = f"{self.url}/{endpoint}"
        if data is None:
            r = self.session.get(url)
        else:
            r = self.session.put(url, json=data)
        r.raise_for_status()
        return r.json()
        
    def get_status(self):
        """Get the current status of the open-ephys gui"""
        reply = self.send("status")
        return reply["mode"]

    def start_acquisition(self):
        """Put the gui into ACQUIRE mode"""
        self.send("status", {"mode": "ACQUIRE"})

    def start_recording(self):
        """Put the gui into RECORD mode"""
        self.send("status", {"mode": "RECORD"})

    def stop(self):
        """Put the gui into IDLE mode"""
        self.send("status", {"mode": "IDLE"})
        
    def get_network_event_processor_port(self):
        """Check for Network Event node, configure it for broadcast, and return the port"""
        reply = self.send("processors")
        for proc in reply["processors"]:
            if proc["name"] == "Network Events":
                break
        else:
            raise RunTimeError("Network Events plugin not found in the signal chain")
        processor_id = proc["id"]
        log.info(" - setting Network Events broadcast to ON")
        reply = self.send(f"processors/{processor_id}/parameters/broadcast_all_messages", {"value": True})
        if int(reply["value"]) != 1:
            raise RuntimeError("unable to set broadcast to ON in Network Events plugin")
        for param in proc["parameters"]:
            if param["name"] == "port":
                return int(param["value"])
        else:
            raise RuntimeError("unable to determine port for Network Events plugin")

    def configure_recording(self, path: str, prepend: str, append: str):
        """Configure the recording directory and name"""
        reply = self.send("recording")
        try:
            record_node = reply["record_nodes"][0]
            record_node_id = record_node["node_id"]
        except KeyError:
            raise RuntimeError("no recording node in the signal chain")
        log.info(" - setting recording directory to %s", path)
        self.send("recording", {"parent_directory": path, "prepend_text": f"{prepend}_", "append_text": f"_{append}"})
        self.send(f"recording/{record_node_id}", {"parent_directory": path})

    def message(self, msg: str):
        """Send a message to all nodes (used for saving metadata)"""
        self.send("message", {"text": msg})

    
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
        "--warmup",
        help="pause between starting acquisition and recording (default: %(default)s s)",
        type=float,
        default=5.0,
    )
    p.add_argument(
        "--open-ephys-directory",
        "-d",
        metavar="DIR",
        default=os.environ["HOME"],
        help="open-ephys recording directory (default: %(default)s)",
    )

    p.add_argument(
        "--experiment", required=True, help="name of the experiment"
    )
    p.add_argument(
        "--animal",
        required=True,
        help="identifier (uuid) of the experimental subject",
    )
    p.add_argument(
        "-k",
        help="specify additional metadata for the recording (use multiple -k for multiple fields).",
        action=ParseKeyVal,
        default=dict(),
        metavar="KEY=VALUE",
        dest="metadata",
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

    if args.open_ephys is not None:
        try:
            log.info("checking for a running open-ephys on %s", args.open_ephys)
            oe_controller = OpenEphysControl(args.open_ephys)
            if oe_controller.get_status() != "IDLE":
                raise RuntimeError(
                    "open-ephys is already acquiring data. Stop it and try again."
                )
            network_event_port = oe_controller.get_network_event_processor_port()
            oe_controller.configure_recording(args.open_ephys_directory, args.animal, args.experiment)
        except rq.exceptions.ConnectionError:
            log.error(" - error: unable to connect to open-ephys. Is it running?")
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

    additional_metadata = (f"-a {key}={value}" for key, value in args.metadata.items())
    jrecord_args = (
        binary_paths["jrecord"],
        "-n",
        "jpresent",
        "-E",
        "jstim",
        "-a",
        f"experiment={args.experiment}",
        "-a",
        f"animal={args.animal}",
        *additional_metadata,
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
    if args.open_ephys is not None:
        jrelay_args.extend(("--open-ephys", f"tcp://{args.open_ephys}:{network_event_port}"))
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
        if args.open_ephys is not None:
            log.info("starting open-ephys acquisition:")
            oe_controller.start_acquisition()
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
        if args.open_ephys is not None:
            log.info("starting open-ephys recording:")
            oe_controller.start_recording()
        time.sleep(args.gap)
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
