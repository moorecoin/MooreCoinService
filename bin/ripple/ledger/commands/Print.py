from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.ledger.args import args
from ripple.ledger import searchledgers

import json

safe = true

help = """print

print the ledgers to stdout.  the default command."""

def run_print(server):
    args.display(print, server, searchledgers.search(server))
