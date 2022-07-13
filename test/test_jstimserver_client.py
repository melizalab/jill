# -*- coding: utf-8 -*-
# -*- mode: python -*-

# python 3 compatibility
from __future__ import absolute_import
from __future__ import print_function
from __future__ import division
from __future__ import unicode_literals

import os.path
import pprint
import zmq
import json
import time

if __name__ == "__main__":

    import argparse

    p = argparse.ArgumentParser()
    p.add_argument(
        "-e",
        "--endpoint",
        help="endpoint of jstimserver",
        default="ipc:///tmp/org.meliza.jill/default/jstimserver",
    )

    opts = p.parse_args()

    ctx = zmq.Context()

    subsock = ctx.socket(zmq.SUB)
    subsock.connect(os.path.join(opts.endpoint, "pub"))
    subsock.setsockopt(zmq.SUBSCRIBE, b"")

    reqsock = ctx.socket(zmq.REQ)
    reqsock.connect(os.path.join(opts.endpoint, "req"))

    reqsock.send(b"STIMLIST")
    reply = reqsock.recv()
    stims = json.loads(reply.decode("utf-8"))
    print("received list of stimuli (n=%d)" % len(stims["stimuli"]))
    pprint.pprint(stims)

    req = "INTERRUPT"
    print("req: %s" % req)
    reqsock.send_string(req)
    reply = reqsock.recv_string()
    print("rep: %s" % reply)

    stim_1 = stims["stimuli"][0]["name"]
    req = "PLAY %s" % stim_1
    print("req: %s" % req)
    reqsock.send_string(req)
    reply = reqsock.recv_string()
    print("rep: %s" % reply)

    time.sleep(0.1)
    req = "PLAY %s" % stim_1
    print("req: %s" % req)
    reqsock.send_string(req)
    reply = reqsock.recv_string()
    print("rep: %s" % reply)

    time.sleep(0.1)
    req = "INTERRUPT"
    print("req: %s" % req)
    reqsock.send_string(req)
    reply = reqsock.recv_string()
    print("rep: %s" % reply)

    while 1:
        msg = subsock.recv()
        print("msg: %s" % msg.decode("ascii"))
        if msg.startswith(b"STOPPING"):
            break

    reqsock.close()
    subsock.close()
    ctx.term()
