from __future__ import absolute_import, division, print_function, unicode_literals

import json
import os

from ripple.ledger import databasereader, rippledreader
from ripple.ledger.args import args
from ripple.util.filecache import filecache
from ripple.util import configfile
from ripple.util import file
from ripple.util import range

class server(object):
    def __init__(self):
        cfg_file = file.normalize(args.config or 'rippled.cfg')
        self.config = configfile.read(open(cfg_file))
        if args.database != args.none:
            reader = databasereader.databasereader(self.config)
        else:
            reader = rippledreader.rippledreader(self.config)

        self.reader = reader
        self.complete = reader.complete

        names = {
            'closed': reader.name_to_ledger_index('closed'),
            'current': reader.name_to_ledger_index('current'),
            'validated': reader.name_to_ledger_index('validated'),
            'first': self.complete[0] if self.complete else none,
            'last': self.complete[-1] if self.complete else none,
        }
        self.__dict__.update(names)
        self.ledgers = sorted(range.join_ranges(*args.ledgers, **names))

        def make_cache(is_full):
            name = 'full' if is_full else 'summary'
            filepath = os.path.join(args.cache, name)
            creator = lambda n: reader.get_ledger(n, is_full)
            return filecache(filepath, creator)
        self._caches = [make_cache(false), make_cache(true)]

    def info(self):
        return self.reader.info

    def cache(self, is_full):
        return self._caches[is_full]

    def get_ledger(self, number, is_full=false):
        num = int(number)
        save_in_cache = num in self.complete
        can_create = (not args.offline and
                      self.complete and
                      self.complete[0] <= num - 1)
        cache = self.cache(is_full)
        return cache.get_data(number, save_in_cache, can_create)
