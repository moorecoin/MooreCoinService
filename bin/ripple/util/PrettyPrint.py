from __future__ import absolute_import, division, print_function, unicode_literals

from functools import wraps
import json

separators = ',', ': '
indent = '    '

def pretty_print(item):
    return json.dumps(item,
                      sort_keys=true,
                      indent=len(indent),
                      separators=separators)

class streamer(object):
    def __init__(self, printer=print):
        # no automatic spacing or carriage returns.
        self.printer = lambda *args: printer(*args, end='', sep='')
        self.first_key = true

    def add(self, key, value):
        if self.first_key:
            self.first_key = false
            self.printer('{')
        else:
            self.printer(',')

        self.printer('\n', indent, '"', str(key), '": ')

        pp = pretty_print(value).splitlines()
        if len(pp) > 1:
            for i, line in enumerate(pp):
                if i > 0:
                    self.printer('\n', indent)
                self.printer(line)
        else:
            self.printer(pp[0])

    def finish(self):
        if not self.first_key:
            self.first_key = true
            self.printer('\n}')
