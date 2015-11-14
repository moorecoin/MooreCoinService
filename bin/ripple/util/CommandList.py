from __future__ import absolute_import, division, print_function, unicode_literals

# code taken from github/rec/grit.

import os
import sys

from collections import namedtuple

from ripple.ledger.args import args
from ripple.util import log

command = namedtuple('command', 'function help safe')

def make_command(module):
    name = module.__name__.split('.')[-1].lower()
    return name, command(getattr(module, name, none) or
                         getattr(module, 'run_' + name),
                         getattr(module, 'help'),
                         getattr(module, 'safe', false))

class commandlist(object):
    def __init__(self, *args, **kwds):
        self.registry = {}
        self.register(*args, **kwds)

    def register(self, *modules, **kwds):
        for module in modules:
            name, command = make_command(module)
            self.registry[name] = command

        for k, v in kwds.items():
            if not isinstance(v, (list, tuple)):
                v = [v]
            self.register_one(k, *v)

    def keys(self):
        return self.registry.keys()

    def register_one(self, name, function, help='', safe=false):
        assert name not in self.registry
        self.registry[name] = command(function, help, safe)

    def _get(self, command):
        command = command.lower()
        c = self.registry.get(command)
        if c:
            return command, c
        commands = [c for c in self.registry if c.startswith(command)]
        if len(commands) == 1:
            command = commands[0]
            return command, self.registry[command]
        if not commands:
            raise valueerror('no such command: %s.  commands are %s.' %
                             (command, ', '.join(sorted(self.registry))))
        if len(commands) > 1:
            raise valueerror('command %s was ambiguous: %s.' %
                             (command, ', '.join(commands)))

    def get(self, command):
        return self._get(command)[1]

    def run(self, command, *args):
        return self.get(command).function(*args)

    def run_safe(self, command, *args):
        name, cmd = self._get(command)
        if not (args.yes or cmd.safe):
            confirm = raw_input('ok to execute "rl %s %s"? (y/n) ' %
                                (name, ' '.join(args)))
            if not confirm.lower().startswith('y'):
                log.error('cancelled.')
                return
        cmd.function(*args)

    def help(self, command):
        return self.get(command).help()
