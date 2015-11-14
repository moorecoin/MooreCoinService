from __future__ import absolute_import, division, print_function, unicode_literals

import sys

from ripple.ledger.args import args
from ripple.util import log
from ripple.util import range
from ripple.util import search

def search(server):
    """yields a stream of ledger numbers that match the given condition."""
    condition = lambda number: args.condition(server, number)
    ledgers = server.ledgers
    if args.binary:
        try:
            position = search.first if args.position == 'first' else search.last
            yield search.binary_search(
                ledgers[0], ledgers[-1], condition, position)
        except:
            log.fatal('no ledgers matching condition "%s".' % condition,
                      file=sys.stderr)
    else:
        for x in search.linear_search(ledgers, condition):
            yield x
