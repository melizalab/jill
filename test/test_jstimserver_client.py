# -*- coding: utf-8 -*-
# -*- mode: python -*-
"""Test the behavior of jstimserver

This script requires some dependencies:
 - pip install anyio>=3.6.2 pyzmq>=25.0.2

"""


import anyio
import json
import os.path
import logging

import zmq
from zmq.asyncio import Context


error_count = 0
timeout_count = 0


def equals(expected):
    return expected.__eq__


def startswith(expected):
    return lambda s: s.startswith(expected)


async def expect(socket, is_expected, timeout):
    with anyio.move_on_after(timeout) as scope:
        reply = await socket.recv_string()
        if is_expected(reply):
            logging.info("   ✔︎ socket received expected %s", reply)
        else:
            logging.error("   ✗ error: socket got unexpected %s", reply)
            error_count += 1
    if scope.cancel_called:
        logging.error("   ✗ error: socket timed out waiting for message")
        timeout_count += 1


async def expect_done_or_interrupt(socket, timeout):
    """If the client interrupts near the end of the stimulus, it may get
    INTERRUPTED or a NOTPLAYING and DONE depending on the exact timing."""
    with anyio.move_on_after(timeout) as scope:
        reply = await socket.recv_string()
        if reply.startswith("INTERRUPTED"):
            logging.info("   ✔︎ socket received %s", reply)
        elif reply.startswith("DONE"):
            logging.info("   ✔︎ socket received %s", reply)
            reply = await socket.recv_string()
            if reply == "NOTPLAYING":
                logging.info("   ✔︎ socket received NOTPLAYING")
            else:
                logging.error("   ✗ error: socket got unexpected %s", reply)
                error_count += 1
        else:
            logging.error("   ✗ error: socket got unexpected %s", reply)
            error_count += 1
    if scope.cancel_called:
        logging.error("   ✗ error: socket timed out waiting for message")
        timeout_count += 1


async def main(argv=None):
    import argparse

    p = argparse.ArgumentParser()
    p.add_argument(
        "-e",
        "--endpoint",
        help="endpoint of jstimserver",
        default="ipc:///tmp/org.meliza.jill/default/jstimserver",
    )

    opts = p.parse_args(argv)
    logging.basicConfig(format="%(message)s", level=logging.INFO)

    with Context.instance() as ctx:
        with ctx.socket(zmq.SUB) as subsock, ctx.socket(zmq.REQ) as reqsock:
            logging.info("connecting to jstimserver at %s...", opts.endpoint)
            subsock.connect(os.path.join(opts.endpoint, "pub"))
            subsock.setsockopt(zmq.SUBSCRIBE, b"")
            reqsock.connect(os.path.join(opts.endpoint, "req"))

            await reqsock.send(b"VERSION")
            reply = await reqsock.recv_string()
            logging.info(" - connected to jstimserver version %s", reply)

            logging.info("retrieving stimulus list...")
            await reqsock.send(b"STIMLIST")
            reply = await reqsock.recv_string()
            stims = json.loads(reply)
            logging.info(" - received list of stimuli (n=%d)", len(stims["stimuli"]))
            for stim in stims["stimuli"]:
                stim["duration"] = float(stim["duration"])
                logging.info("   - %s: %.2f s", stim["name"], stim["duration"])

            logging.info("testing interruption with no stimulus playing...")
            await reqsock.send_string("INTERRUPT")
            await expect(reqsock, equals("OK"), 0.1)
            await expect(subsock, equals("NOTPLAYING"), 0.1)

            logging.info("testing interrupting playback...")
            for stim in stims["stimuli"]:
                logging.info(" - requesting playback of %s...", stim["name"])
                await reqsock.send_string("PLAY %s" % stim["name"])
                await expect(reqsock, equals("OK"), 0.1)
                await expect(subsock, startswith("PLAYING %s " % stim["name"]), 0.1)
                logging.info(" - requesting interrupt of %s...", stim["name"])
                await reqsock.send_string("INTERRUPT")
                await expect(reqsock, equals("OK"), 0.1)
                await expect(subsock, startswith("INTERRUPTED %s" % stim["name"]), 0.1)

            # this is the edge case; try to time the interrupt near the end of
            # the stimulus. The goal is to make sure jstimserver doesn't go into
            # some invalid state.
            logging.info("testing interrupting near end of stimulus...")
            for stim in stims["stimuli"]:
                logging.info(" - requesting playback of %s...", stim["name"])
                async with anyio.create_task_group() as tg:
                    await reqsock.send_string("PLAY %s " % stim["name"])
                    tg.start_soon(expect, reqsock, equals("OK"), 0.1)
                    tg.start_soon(
                        expect, subsock, startswith("PLAYING %s" % stim["name"]), 0.1
                    )
                    await anyio.sleep(stim["duration"])
                    logging.info(" - requesting interrupt of %s...", stim["name"])
                    await reqsock.send_string("INTERRUPT")
                    tg.start_soon(expect, reqsock, equals("OK"), 0.1)
                    tg.start_soon(
                        expect_done_or_interrupt,
                        subsock,
                        0.1,
                    )
                await anyio.sleep(0.1)

            logging.info("testing playback with no stimulus playing...")
            for stim in stims["stimuli"]:
                logging.info(" - requesting playback of %s...", stim["name"])
                await reqsock.send_string("PLAY %s" % stim["name"])
                await expect(reqsock, equals("OK"), 0.1)
                await expect(subsock, startswith("PLAYING %s" % stim["name"]), 0.1)
                await expect(
                    subsock,
                    startswith("DONE %s" % stim["name"]),
                    stim["duration"] + 0.2,
                )

            logging.info("testing playback with a stimulus playing...")
            for stim in stims["stimuli"]:
                async with anyio.create_task_group() as tg:
                    logging.info(" - requesting playback of %s...", stim["name"])
                    await reqsock.send_string("PLAY %s" % stim["name"])
                    tg.start_soon(expect, reqsock, equals("OK"), 0.1)
                    tg.start_soon(
                        expect, subsock, startswith("PLAYING %s" % stim["name"]), 0.1
                    )
                    await anyio.sleep(0.2)
                    logging.info(" - requesting second playback of %s...", stim["name"])
                    await reqsock.send_string("PLAY %s" % stim["name"])
                    tg.start_soon(expect, reqsock, equals("OK"), 0.1)
                    tg.start_soon(expect, subsock, equals("BUSY"), 0.1)
                    tg.start_soon(
                        expect,
                        subsock,
                        startswith("DONE %s" % stim["name"]),
                        stim["duration"] + 0.2,
                    )

            logging.info("Summary:")
            logging.info(" - Total number of unexpected responses: %d", error_count)
            logging.info(" - Total number of timeouts: %d", timeout_count)


if __name__ == "__main__":
    anyio.run(main)
