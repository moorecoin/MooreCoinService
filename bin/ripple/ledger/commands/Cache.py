from __future__ import absolute_import, division, print_function, unicode_literals

from ripple.ledger.args import args
from ripple.util import log
from ripple.util import range
from ripple.util.prettyprint import pretty_print

safe = true

help = """cache
return server_info"""

def cache(server, clear=false):
    cache = server.cache(args.full)
    name = ['summary', 'full'][args.full]
    files = cache.file_count()
    if not files:
        log.error('no files in %s cache.' % name)

    elif clear:
        if not clear.strip() == 'clear':
            raise exception("don't understand 'clear %s'." % clear)
        if not args.yes:
            yes = raw_input('ok to clear %s cache? (y/n) ' % name)
            if not yes.lower().startswith('y'):
                log.out('cancelled.')
                return
        cache.clear(args.full)
        log.out('%s cache cleared - %d file%s deleted.' %
                (name.capitalize(), files, '' if files == 1 else 's'))

    else:
        caches = (int(c) for c in cache.cache_list())
        log.out(range.to_string(caches))
