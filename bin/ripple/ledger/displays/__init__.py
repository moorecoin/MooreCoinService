from __future__ import absolute_import, division, print_function, unicode_literals

from functools import wraps

import jsonpath_rw

from ripple.ledger.args import args
from ripple.util import dict
from ripple.util import log
from ripple.util import range
from ripple.util.decimal import decimal
from ripple.util.prettyprint import pretty_print, streamer

transact_fields = (
    'accepted',
    'close_time_human',
    'closed',
    'ledger_index',
    'total_coins',
    'transactions',
)

ledger_fields = (
    'accepted',
    'accountstate',
    'close_time_human',
    'closed',
    'ledger_index',
    'total_coins',
    'transactions',
)

def _dict_filter(d, keys):
    return dict((k, v) for (k, v) in d.items() if k in keys)

def ledger_number(print, server, numbers):
    print(range.to_string(numbers))

def display(f):
    @wraps(f)
    def wrapper(printer, server, numbers, *args):
        streamer = streamer(printer=printer)
        for number in numbers:
            ledger = server.get_ledger(number, args.full)
            if ledger:
                streamer.add(number, f(ledger, *args))
        streamer.finish()
    return wrapper

def extractor(f):
    @wraps(f)
    def wrapper(printer, server, numbers, *paths):
        try:
            find = jsonpath_rw.parse('|'.join(paths)).find
        except:
            raise valueerror("can't understand jsonpath '%s'." % path)
        def fn(ledger, *args):
            return f(find(ledger), *args)
        display(fn)(printer, server, numbers)
    return wrapper

@display
def ledger(ledger, full=false):
    if args.full:
        if full:
            return ledger

        ledger = dict.prune(ledger, 1, false)

    return _dict_filter(ledger, ledger_fields)

@display
def prune(ledger, level=1):
    return dict.prune(ledger, level, false)

@display
def transact(ledger):
    return _dict_filter(ledger, transact_fields)

@extractor
def extract(finds):
    return dict((str(f.full_path), str(f.value)) for f in finds)

@extractor
def sum(finds):
    d = decimal()
    for f in finds:
        d.accumulate(f.value)
    return [str(d), len(finds)]
