# beast.py
# copyright 2014 by:
#   vinnie falco <vinnie.falco@gmail.com>
#   tom ritchford <?>
#   nik bougalis <?>
# this file is part of beast: http://github.com/vinniefalco/beast

from __future__ import absolute_import, division, print_function, unicode_literals

import os
import platform
import subprocess
import sys
import scons.node
import scons.util

#-------------------------------------------------------------------------------
#
# environment
#
#-------------------------------------------------------------------------------

def _execute(args, include_errors=true, **kwds):
    """execute a shell command and return the value.  if args is a string,
    it's split on spaces - if some of your arguments contain spaces, args should
    instead be a list of arguments."""
    def single_line(line, report_errors=true, joiner='+'):
        """force a string to be a single line with no carriage returns, and report
        a warning if there was more than one line."""
        lines = line.strip().splitlines()
        if report_errors and len(lines) > 1:
          print('multiline result:', lines)
        return joiner.join(lines)
    def is_string(s):
        """is s a string? - in either python 2.x or 3.x."""
        return isinstance(s, (str, unicode))
    if is_string(args):
        args = args.split()
    stderr = subprocess.stdout if include_errors else none
    return single_line(subprocess.check_output(args, stderr=stderr, **kwds))

class __system(object):
    """provides information about the host platform"""
    def __init__(self):
        self.name = platform.system()
        self.linux = self.name == 'linux'
        self.osx = self.name == 'darwin'
        self.windows = self.name == 'windows'
        self.distro = none
        self.version = none

        # true if building under the travis ci (http://travis-ci.org)
        self.travis = (
            os.environ.get('travis', '0') == 'true') and (
            os.environ.get('ci', '0') == 'true')

        if self.linux:
            self.distro, self.version, _ = platform.linux_distribution()
            self.__display = '%s %s (%s)' % (self.distro, self.version, self.name)

        elif self.osx:
            parts = platform.mac_ver()[0].split('.')
            while len(parts) < 3:
                parts.append('0')
            self.__display = '%s %s' % (self.name, '.'.join(parts))
        elif self.windows:
            release, version, csd, ptype = platform.win32_ver()
            self.__display = '%s %s %s (%s)' % (self.name, release, version, ptype)

        else:
            raise exception('unknown system platform "' + self.name + '"')

        self.platform = self.distro or self.name

    def __str__(self):
        return self.__display

class git(object):
    """provides information about git and the repository we are called from"""
    def __init__(self, env):
        self.tags = self.branch = self.user = ''
        self.exists = env.detect('git')
        if self.exists:
            try:
                self.tags = _execute('git describe --tags --always --dirty')
                self.branch = _execute('git rev-parse --abbrev-ref head')
                remote = _execute('git config remote.origin.url')
                self.user = remote.split(':')[1].split('/')[0]
            except:
                self.exists = false

system = __system()

#-------------------------------------------------------------------------------

def printchildren(target):
    def doprint(tgt, level, found):
        for item in tgt:
            if scons.util.is_list(item):
                doprint(item, level, found)
            else:
                if item.abspath in found:
                    continue
                found[item.abspath] = false
                print('\t'*level + item.path)
                #doprint(item.children(scan=1), level+1, found)
                item.scan()
                doprint(item.all_children(), level+1, found)
    doprint(target, 0, {})

def variantfile(path, variant_dirs):
    '''returns the path to the corresponding dict entry in variant_dirs'''
    path = str(path)
    for dest, source in variant_dirs.iteritems():
        common = os.path.commonprefix([path, source])
        if common == source:
            return os.path.join(dest, path[len(common)+1:])
    return path

def variantfiles(files, variant_dirs):
    '''returns a list of files remapped to their variant directories'''
    result = []
    for path in files:
        result.append(variantfile(path, variant_dirs))
    return result

def printenv(env, keys):
    if type(keys) != list:
        keys = list(keys)
    s = ''
    for key in keys:
        if key in env:
            value = env[key]
        else:
            value = ''
        s+=('%s=%s, ' % (key, value))
    print('[' + s + ']')

#-------------------------------------------------------------------------------
#
# output
#
#-------------------------------------------------------------------------------

# see https://stackoverflow.com/questions/7445658/how-to-detect-if-the-console-does-support-ansi-escape-codes-in-python
can_change_color = (
  hasattr(sys.stderr, "isatty")
  and sys.stderr.isatty()
  and not system.windows
  and not os.environ.get('inside_emacs')
  )

# see https://en.wikipedia.org/wiki/ansi_escape_code
blue = 94
green = 92
red = 91
yellow = 93

def add_mode(text, *modes):
    if can_change_color:
        modes = ';'.join(str(m) for m in modes)
        return '\033[%sm%s\033[0m' % (modes, text)
    else:
        return text

def blue(text):
    return add_mode(text, blue)

def green(text):
    return add_mode(text, green)

def red(text):
    return add_mode(text, red)

def yellow(text):
    return add_mode(text, yellow)

def warn(text, print=print):
    print('%s %s' % (red('warning:'), text))

# prints command lines using environment substitutions
def print_coms(coms, env):
    if type(coms) is str:
        coms=list(coms)
    for key in coms:
        cmdline = env.subst(env[key], 0,
            env.file('<target>'), env.file('<sources>'))
        print (green(cmdline))
