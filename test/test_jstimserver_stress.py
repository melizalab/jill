# -*- coding: utf-8 -*-
# -*- mode: python -*-

# python 3 compatibility
from __future__ import absolute_import
from __future__ import print_function
from __future__ import division
from __future__ import unicode_literals

import os.path
import itertools
import pprint
import zmq
import json
import time

if __name__ == "__main__":

    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("-e", "--endpoint",
                   help="endpoint of jstimserver",
                   default="ipc:///tmp/org.meliza.jill/default/jstimserver")

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

    for i, stim in enumerate(itertools.cycle(stims["stimuli"])):
        name = stim["name"]
        req = "PLAY %s" % name
        print("req %d: %s" % (i, req))
        reqsock.send_string(req)
        reply = reqsock.recv_string()
        print("rep %d: %s" % (i, reply))
        while 1:
            msg = subsock.recv()
            print("pub: %s" % msg.decode('ascii'))
            if msg.startswith(b"DONE"):
                break

    reqsock.close()
    subsock.close()
    ctx.term()
