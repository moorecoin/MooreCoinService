from __future__ import absolute_import, division, print_function, unicode_literals

import json
import os
import subprocess

from ripple.ledger.args import args
from ripple.util import configfile
from ripple.util import database
from ripple.util import file
from ripple.util import log
from ripple.util import range

ledger_query = """
select
     l.*, count(1) validations
from
    (select ledgerhash, ledgerseq from ledgers order by ledgerseq desc) l
    join validations v
    on (v.ledgerhash = l.ledgerhash)
    group by l.ledgerhash
    having validations >= {validation_quorum}
    order by 2;
"""

complete_query = """
select
     l.ledgerseq, count(*) validations
from
    (select ledgerhash, ledgerseq from ledgers order by ledgerseq) l
    join validations v
    on (v.ledgerhash = l.ledgerhash)
    group by l.ledgerhash
    having validations >= :validation_quorum
    order by 2;
"""

_database_name = 'ledger.db'

use_placeholders = false

class databasereader(object):
    def __init__(self, config):
        assert args.database != args.none
        database = args.database or config['database_path']
        if not database.endswith(_database_name):
            database = os.path.join(database, _database_name)
        if use_placeholders:
            cursor = database.fetchall(
                database, complete_query, config)
        else:
            cursor = database.fetchall(
                database, ledger_query.format(**config), {})
        self.complete = [c[1] for c in cursor]

    def name_to_ledger_index(self, ledger_name, is_full=false):
        if not self.complete:
            return none
        if ledger_name == 'closed':
            return self.complete[-1]
        if ledger_name == 'current':
            return none
        if ledger_name == 'validated':
            return self.complete[-1]

    def get_ledger(self, name, is_full=false):
        cmd = ['ledger', str(name)]
        if is_full:
            cmd.append('full')
        response = self._command(*cmd)
        result = response.get('ledger')
        if result:
            return result
        error = response['error']
        etext = _error_text.get(error)
        if etext:
            error = '%s (%s)' % (etext, error)
        log.fatal(_error_text.get(error, error))
