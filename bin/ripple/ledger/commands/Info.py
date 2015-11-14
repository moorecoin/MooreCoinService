from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.ledger.args import args
from ripple.util import log
from ripple.util import range
from ripple.util.prettyprint import pretty_print

safe = true

help = 'info - return server_info'

def info(server):
    log.out('first =', server.first)
    log.out('last =', server.last)
    log.out('closed =', server.closed)
    log.out('current =', server.current)
    log.out('validated =', server.validated)
    log.out('complete =', range.to_string(server.complete))

    if args.full:
        log.out(pretty_print(server.info()))
