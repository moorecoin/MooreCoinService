#!/usr/bin/env python

from __future__ import absolute_import, division, print_function, unicode_literals

import sys
import traceback

from ripple.ledger import server
from ripple.ledger.commands import cache, info, print
from ripple.ledger.args import args
from ripple.util import log
from ripple.util.commandlist import commandlist

_commands = commandlist(cache, info, print)

if __name__ == '__main__':
    try:
        server = server.server()
        args = list(args.command)
        _commands.run_safe(args.pop(0), server, *args)
    except exception as e:
        if args.verbose:
            print(traceback.format_exc(), sys.stderr)
        log.error(e)
