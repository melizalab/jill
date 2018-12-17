# -*- coding: utf-8 -*-
# -*- mode: python -*-

# python 3 compatibility
from __future__ import absolute_import
from __future__ import print_function
from __future__ import division
from __future__ import unicode_literals

import zmq

if __name__ == "__main__":

    import argparse
    p = argparse.ArgumentParser()
    p.add_argument("-e", "--endpoint",
                   help="endpoint of jstimserver",
                   default="ipc:///tmp/org.meliza.jill/default/jstimserver/req")
    p.add_argument("request")

    opts = p.parse_args()

    ctx = zmq.Context()
    sock = ctx.socket(zmq.DEALER)
    sock.connect(opts.endpoint)

    sock.send(opts.request.encode())
    reply = sock.recv()
    print(reply.decode('ascii'));
