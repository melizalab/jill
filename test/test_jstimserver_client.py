# -*- coding: utf-8 -*-
# -*- mode: python -*-

# python 3 compatibility
from __future__ import absolute_import
from __future__ import print_function
from __future__ import division
from __future__ import unicode_literals

import os.path
import zmq

if __name__ == "__main__":

    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("-e", "--endpoint",
                   help="endpoint of jstimserver",
                   default="ipc:///tmp/org.meliza.jill/default/")
    p.add_argument("request")

    opts = p.parse_args()

    ctx = zmq.Context()

    subsock = ctx.socket(zmq.SUB)
    subsock.connect(os.path.join(opts.endpoint, "pub"))
    subsock.setsockopt(zmq.SUBSCRIBE, b"")

    reqsock = ctx.socket(zmq.REQ)
    reqsock.connect(os.path.join(opts.endpoint, "jstimserver", "req"))

    reqsock.send(opts.request.encode())
    reply = reqsock.recv()
    print(reply.decode('ascii'))

    while 1:
        msg = subsock.recv()
        print("msg: %s" % msg.decode('ascii'))
        if msg == b"STOPPING":
            break

    reqsock.close()
    subsock.close()
    ctx.term()
