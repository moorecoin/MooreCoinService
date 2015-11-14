from __future__ import absolute_import, division, print_function, unicode_literals

import sys

verbose = false

def out(*args, **kwds):
    kwds.get('print', print)(*args, file=sys.stdout, **kwds)

def info(*args, **kwds):
    if verbose:
        out(*args, **kwds)

def warn(*args, **kwds):
    out('warning:', *args, **kwds)

def error(*args, **kwds):
    out('error:', *args, **kwds)

def fatal(*args, **kwds):
    raise exception('fatal: ' + ' '.join(str(a) for a in args))
