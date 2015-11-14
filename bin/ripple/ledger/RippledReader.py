from __future__ import absolute_import, division, print_function, unicode_literals

import json
import os
import subprocess

from ripple.ledger.args import args
from ripple.util import file
from ripple.util import log
from ripple.util import range

_error_code_reason = {
    62: 'no rippled server is running.',
}

_error_text = {
    'lgrnotfound': 'the ledger you requested was not found.',
    'nocurrent': 'the server has no current ledger.',
    'nonetwork': 'the server did not respond to your request.',
}

_default_error_ = "couldn't connect to server."

class rippledreader(object):
    def __init__(self, config):
        fname = file.normalize(args.rippled)
        if not os.path.exists(fname):
            raise exception('no rippled found at %s.' % fname)
        self.cmd = [fname]
        if args.config:
            self.cmd.extend(['--conf', file.normalize(args.config)])
        self.info = self._command('server_info')['info']
        c = self.info.get('complete_ledgers')
        if c == 'empty':
            self.complete = []
        else:
            self.complete = sorted(range.from_string(c))

    def name_to_ledger_index(self, ledger_name, is_full=false):
        return self.get_ledger(ledger_name, is_full)['ledger_index']

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

    def _command(self, *cmds):
        cmd = self.cmd + list(cmds)
        try:
            data = subprocess.check_output(cmd, stderr=subprocess.pipe)
        except subprocess.calledprocesserror as e:
            raise exception(_error_code_reason.get(
                e.returncode, _default_error_))

        part = json.loads(data)
        try:
            return part['result']
        except:
            raise valueerror(part.get('error', 'unknown error'))
