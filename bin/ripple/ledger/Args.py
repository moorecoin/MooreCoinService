from __future__ import absolute_import, division, print_function, unicode_literals

import argparse
import importlib
import os

from ripple.ledger import ledgernumber
from ripple.util import file
from ripple.util import log
from ripple.util import prettyprint
from ripple.util import range
from ripple.util.function import function

name = 'ledgertool'
version = '0.1'
none = '(none)'

_parser = argparse.argumentparser(
    prog=name,
    description='retrieve and process ripple ledgers.',
    epilog=ledgernumber.help,
    )

# positional arguments.
_parser.add_argument(
    'command',
    nargs='*',
    help='command to execute.'
)

# flag arguments.
_parser.add_argument(
    '--binary',
    action='store_true',
    help='if true, searches are binary - by default linear search is used.',
    )

_parser.add_argument(
    '--cache',
    default='~/.local/share/ripple/ledger',
    help='the cache directory.',
    )

_parser.add_argument(
    '--complete',
    action='store_true',
    help='if set, only match complete ledgers.',
    )

_parser.add_argument(
    '--condition', '-c',
    help='the name of a condition function used to match ledgers.',
    )

_parser.add_argument(
    '--config',
    help='the rippled configuration file name.',
    )

_parser.add_argument(
    '--database', '-d',
    nargs='*',
    default=none,
    help='specify a database.',
    )

_parser.add_argument(
    '--display',
    help='specify a function to display ledgers.',
    )

_parser.add_argument(
    '--full', '-f',
    action='store_true',
    help='if true, request full ledgers.',
    )

_parser.add_argument(
    '--indent', '-i',
    type=int,
    default=2,
    help='how many spaces to indent when display in json.',
    )

_parser.add_argument(
    '--offline', '-o',
    action='store_true',
    help='if true, work entirely from cache, do not try to contact the server.',
    )

_parser.add_argument(
    '--position', '-p',
    choices=['all', 'first', 'last'],
    default='last',
    help='select which ledgers to display.',
    )

_parser.add_argument(
    '--rippled', '-r',
    help='the filename of a rippled binary for retrieving ledgers.',
    )

_parser.add_argument(
    '--server', '-s',
    help='ip address of a rippled json server.',
    )

_parser.add_argument(
    '--utc', '-u',
    action='store_true',
    help='if true, display times in utc rather than local time.',
    )

_parser.add_argument(
    '--validations',
    default=3,
    help='the number of validations needed before considering a ledger valid.',
    )

_parser.add_argument(
    '--version',
    action='version',
    version='%(prog)s ' + version,
    help='print the current version of %(prog)s',
    )

_parser.add_argument(
    '--verbose', '-v',
    action='store_true',
    help='if true, give status messages on stderr.',
    )

_parser.add_argument(
    '--window', '-w',
    type=int,
    default=0,
    help='how many ledgers to display around the matching ledger.',
    )

_parser.add_argument(
    '--yes', '-y',
    action='store_true',
    help='if true, don\'t ask for confirmation on large commands.',
)

# read the arguments from the command line.
args = _parser.parse_args()
args.none = none

log.verbose = args.verbose

# now remove any items that look like ledger numbers from the command line.
_command = args.command
_parts = (args.command, args.ledgers) = ([], [])

for c in _command:
    _parts[range.is_range(c, *ledgernumber.ledgers)].append(c)

args.command = args.command or ['print' if args.ledgers else 'info']

args.cache = file.normalize(args.cache)

if not args.ledgers:
    if args.condition:
        log.warn('--condition needs a range of ledgers')
    if args.display:
        log.warn('--display needs a range of ledgers')

args.condition = function(
    args.condition or 'all_ledgers', 'ripple.ledger.conditions')
args.display = function(
    args.display or 'ledger_number', 'ripple.ledger.displays')

if args.window < 0:
    raise valueerror('window cannot be negative: --window=%d' %
                     args.window)

prettyprint.indent = (args.indent * ' ')

_loaders = (args.database != none) + bool(args.rippled) + bool(args.server)

if not _loaders:
    args.rippled = 'rippled'

elif _loaders > 1:
    raise valueerror('at most one of --database,  --rippled and --server '
                     'may be specified')
