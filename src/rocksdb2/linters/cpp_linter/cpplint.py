#!/usr/bin/python
# copyright (c) 2013, facebook, inc.  all rights reserved.
# this source code is licensed under the bsd-style license found in the
# license file in the root directory of this source tree. an additional grant
# of patent rights can be found in the patents file in the same directory.
# copyright (c) 2011 the leveldb authors. all rights reserved.
# use of this source code is governed by a bsd-style license that can be
# found in the license file. see the authors file for names of contributors.
#
# copyright (c) 2009 google inc. all rights reserved.
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#    * redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * neither the name of google inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# this software is provided by the copyright holders and contributors
# "as is" and any express or implied warranties, including, but not
# limited to, the implied warranties of merchantability and fitness for
# a particular purpose are disclaimed. in no event shall the copyright
# owner or contributors be liable for any direct, indirect, incidental,
# special, exemplary, or consequential damages (including, but not
# limited to, procurement of substitute goods or services; loss of use,
# data, or profits; or business interruption) however caused and on any
# theory of liability, whether in contract, strict liability, or tort
# (including negligence or otherwise) arising in any way out of the use
# of this software, even if advised of the possibility of such damage.

"""does google-lint on c++ files.

the goal of this script is to identify places in the code that *may*
be in non-compliance with google style.  it does not attempt to fix
up these problems -- the point is to educate.  it does also not
attempt to find all problems, or to ensure that everything it does
find is legitimately a problem.

in particular, we can get very confused by /* and // inside strings!
we do a small hack, which is to ignore //'s with "'s after them on the
same line, but it is far from perfect (in either direction).
"""

import codecs
import copy
import getopt
import math  # for log
import os
import re
import sre_compile
import string
import sys
import unicodedata


_usage = """
syntax: cpplint.py [--verbose=#] [--output=vs7] [--filter=-x,+y,...]
                   [--counting=total|toplevel|detailed] [--root=subdir]
                   [--linelength=digits]
        <file> [file] ...

  the style guidelines this tries to follow are those in
    http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml

  every problem is given a confidence score from 1-5, with 5 meaning we are
  certain of the problem, and 1 meaning it could be a legitimate construct.
  this will miss some errors, and is not a substitute for a code review.

  to suppress false-positive errors of a certain category, add a
  'nolint(category)' comment to the line.  nolint or nolint(*)
  suppresses errors of all categories on that line.

  the files passed in will be linted; at least one file must be provided.
  default linted extensions are .cc, .cpp, .cu, .cuh and .h.  change the
  extensions with the --extensions flag.

  flags:

    output=vs7
      by default, the output is formatted to ease emacs parsing.  visual studio
      compatible output (vs7) may also be used.  other formats are unsupported.

    verbose=#
      specify a number 0-5 to restrict errors to certain verbosity levels.

    filter=-x,+y,...
      specify a comma-separated list of category-filters to apply: only
      error messages whose category names pass the filters will be printed.
      (category names are printed with the message and look like
      "[whitespace/indent]".)  filters are evaluated left to right.
      "-foo" and "foo" means "do not print categories that start with foo".
      "+foo" means "do print categories that start with foo".

      examples: --filter=-whitespace,+whitespace/braces
                --filter=whitespace,runtime/printf,+runtime/printf_format
                --filter=-,+build/include_what_you_use

      to see a list of all the categories used in cpplint, pass no arg:
         --filter=

    counting=total|toplevel|detailed
      the total number of errors found is always printed. if
      'toplevel' is provided, then the count of errors in each of
      the top-level categories like 'build' and 'whitespace' will
      also be printed. if 'detailed' is provided, then a count
      is provided for each category like 'build/class'.

    root=subdir
      the root directory used for deriving header guard cpp variable.
      by default, the header guard cpp variable is calculated as the relative
      path to the directory that contains .git, .hg, or .svn.  when this flag
      is specified, the relative path is calculated from the specified
      directory. if the specified directory does not exist, this flag is
      ignored.

      examples:
        assuing that src/.git exists, the header guard cpp variables for
        src/chrome/browser/ui/browser.h are:

        no flag => chrome_browser_ui_browser_h_
        --root=chrome => browser_ui_browser_h_
        --root=chrome/browser => ui_browser_h_

    linelength=digits
      this is the allowed line length for the project. the default value is
      80 characters.

      examples:
        --linelength=120

    extensions=extension,extension,...
      the allowed file extensions that cpplint will check

      examples:
        --extensions=hpp,cpp
"""

# we categorize each error message we print.  here are the categories.
# we want an explicit list so we can list them all in cpplint --filter=.
# if you add a new error message with a new category, add it to the list
# here!  cpplint_unittest.py should tell you if you forget to do this.
_error_categories = [
  'build/class',
  'build/deprecated',
  'build/endif_comment',
  'build/explicit_make_pair',
  'build/forward_decl',
  'build/header_guard',
  'build/include',
  'build/include_alpha',
  'build/include_order',
  'build/include_what_you_use',
  'build/namespaces',
  'build/printf_format',
  'build/storage_class',
  'legal/copyright',
  'readability/alt_tokens',
  'readability/braces',
  'readability/casting',
  'readability/check',
  'readability/constructors',
  'readability/fn_size',
  'readability/function',
  'readability/multiline_comment',
  'readability/multiline_string',
  'readability/namespace',
  'readability/nolint',
  'readability/nul',
  'readability/streams',
  'readability/todo',
  'readability/utf8',
  'runtime/arrays',
  'runtime/casting',
  'runtime/explicit',
  'runtime/int',
  'runtime/init',
  'runtime/invalid_increment',
  'runtime/member_string_references',
  'runtime/memset',
  'runtime/operator',
  'runtime/printf',
  'runtime/printf_format',
  'runtime/references',
  'runtime/string',
  'runtime/threadsafe_fn',
  'runtime/vlog',
  'whitespace/blank_line',
  'whitespace/braces',
  'whitespace/comma',
  'whitespace/comments',
  'whitespace/empty_conditional_body',
  'whitespace/empty_loop_body',
  'whitespace/end_of_line',
  'whitespace/ending_newline',
  'whitespace/forcolon',
  'whitespace/indent',
  'whitespace/line_length',
  'whitespace/newline',
  'whitespace/operators',
  'whitespace/parens',
  'whitespace/semicolon',
  'whitespace/tab',
  'whitespace/todo'
  ]

# the default state of the category filter. this is overrided by the --filter=
# flag. by default all errors are on, so only add here categories that should be
# off by default (i.e., categories that must be enabled by the --filter= flags).
# all entries here should start with a '-' or '+', as in the --filter= flag.
_default_filters = ['-build/include_alpha']

# we used to check for high-bit characters, but after much discussion we
# decided those were ok, as long as they were in utf-8 and didn't represent
# hard-coded international strings, which belong in a separate i18n file.


# c++ headers
_cpp_headers = frozenset([
    # legacy
    'algobase.h',
    'algo.h',
    'alloc.h',
    'builtinbuf.h',
    'bvector.h',
    'complex.h',
    'defalloc.h',
    'deque.h',
    'editbuf.h',
    'fstream.h',
    'function.h',
    'hash_map',
    'hash_map.h',
    'hash_set',
    'hash_set.h',
    'hashtable.h',
    'heap.h',
    'indstream.h',
    'iomanip.h',
    'iostream.h',
    'istream.h',
    'iterator.h',
    'list.h',
    'map.h',
    'multimap.h',
    'multiset.h',
    'ostream.h',
    'pair.h',
    'parsestream.h',
    'pfstream.h',
    'procbuf.h',
    'pthread_alloc',
    'pthread_alloc.h',
    'rope',
    'rope.h',
    'ropeimpl.h',
    'set.h',
    'slist',
    'slist.h',
    'stack.h',
    'stdiostream.h',
    'stl_alloc.h',
    'stl_relops.h',
    'streambuf.h',
    'stream.h',
    'strfile.h',
    'strstream.h',
    'tempbuf.h',
    'tree.h',
    'type_traits.h',
    'vector.h',
    # 17.6.1.2 c++ library headers
    'algorithm',
    'array',
    'atomic',
    'bitset',
    'chrono',
    'codecvt',
    'complex',
    'condition_variable',
    'deque',
    'exception',
    'forward_list',
    'fstream',
    'functional',
    'future',
    'initializer_list',
    'iomanip',
    'ios',
    'iosfwd',
    'iostream',
    'istream',
    'iterator',
    'limits',
    'list',
    'locale',
    'map',
    'memory',
    'mutex',
    'new',
    'numeric',
    'ostream',
    'queue',
    'random',
    'ratio',
    'regex',
    'set',
    'sstream',
    'stack',
    'stdexcept',
    'streambuf',
    'string',
    'strstream',
    'system_error',
    'thread',
    'tuple',
    'typeindex',
    'typeinfo',
    'type_traits',
    'unordered_map',
    'unordered_set',
    'utility',
    'valarray',
    'vector',
    # 17.6.1.2 c++ headers for c library facilities
    'cassert',
    'ccomplex',
    'cctype',
    'cerrno',
    'cfenv',
    'cfloat',
    'cinttypes',
    'ciso646',
    'climits',
    'clocale',
    'cmath',
    'csetjmp',
    'csignal',
    'cstdalign',
    'cstdarg',
    'cstdbool',
    'cstddef',
    'cstdint',
    'cstdio',
    'cstdlib',
    'cstring',
    'ctgmath',
    'ctime',
    'cuchar',
    'cwchar',
    'cwctype',
    ])

# assertion macros.  these are defined in base/logging.h and
# testing/base/gunit.h.  note that the _m versions need to come first
# for substring matching to work.
_check_macros = [
    'dcheck', 'check',
    'expect_true_m', 'expect_true',
    'assert_true_m', 'assert_true',
    'expect_false_m', 'expect_false',
    'assert_false_m', 'assert_false',
    ]

# replacement macros for check/dcheck/expect_true/expect_false
_check_replacement = dict([(m, {}) for m in _check_macros])

for op, replacement in [('==', 'eq'), ('!=', 'ne'),
                        ('>=', 'ge'), ('>', 'gt'),
                        ('<=', 'le'), ('<', 'lt')]:
  _check_replacement['dcheck'][op] = 'dcheck_%s' % replacement
  _check_replacement['check'][op] = 'check_%s' % replacement
  _check_replacement['expect_true'][op] = 'expect_%s' % replacement
  _check_replacement['assert_true'][op] = 'assert_%s' % replacement
  _check_replacement['expect_true_m'][op] = 'expect_%s_m' % replacement
  _check_replacement['assert_true_m'][op] = 'assert_%s_m' % replacement

for op, inv_replacement in [('==', 'ne'), ('!=', 'eq'),
                            ('>=', 'lt'), ('>', 'le'),
                            ('<=', 'gt'), ('<', 'ge')]:
  _check_replacement['expect_false'][op] = 'expect_%s' % inv_replacement
  _check_replacement['assert_false'][op] = 'assert_%s' % inv_replacement
  _check_replacement['expect_false_m'][op] = 'expect_%s_m' % inv_replacement
  _check_replacement['assert_false_m'][op] = 'assert_%s_m' % inv_replacement

# alternative tokens and their replacements.  for full list, see section 2.5
# alternative tokens [lex.digraph] in the c++ standard.
#
# digraphs (such as '%:') are not included here since it's a mess to
# match those on a word boundary.
_alt_token_replacement = {
    'and': '&&',
    'bitor': '|',
    'or': '||',
    'xor': '^',
    'compl': '~',
    'bitand': '&',
    'and_eq': '&=',
    'or_eq': '|=',
    'xor_eq': '^=',
    'not': '!',
    'not_eq': '!='
    }

# compile regular expression that matches all the above keywords.  the "[ =()]"
# bit is meant to avoid matching these keywords outside of boolean expressions.
#
# false positives include c-style multi-line comments and multi-line strings
# but those have always been troublesome for cpplint.
_alt_token_replacement_pattern = re.compile(
    r'[ =()](' + ('|'.join(_alt_token_replacement.keys())) + r')(?=[ (]|$)')


# these constants define types of headers for use with
# _includestate.checknextincludeorder().
_c_sys_header = 1
_cpp_sys_header = 2
_likely_my_header = 3
_possible_my_header = 4
_other_header = 5

# these constants define the current inline assembly state
_no_asm = 0       # outside of inline assembly block
_inside_asm = 1   # inside inline assembly block
_end_asm = 2      # last line of inline assembly block
_block_asm = 3    # the whole block is an inline assembly block

# match start of assembly blocks
_match_asm = re.compile(r'^\s*(?:asm|_asm|__asm|__asm__)'
                        r'(?:\s+(volatile|__volatile__))?'
                        r'\s*[{(]')


_regexp_compile_cache = {}

# finds occurrences of nolint or nolint(...).
_re_suppression = re.compile(r'\bnolint\b(\([^)]*\))?')

# {str, set(int)}: a map from error categories to sets of linenumbers
# on which those errors are expected and should be suppressed.
_error_suppressions = {}

# the root directory used for deriving header guard cpp variable.
# this is set by --root flag.
_root = none

# the allowed line length of files.
# this is set by --linelength flag.
_line_length = 80

# the allowed extensions for file names
# this is set by --extensions flag.
_valid_extensions = set(['cc', 'h', 'cpp', 'cu', 'cuh'])

def parsenolintsuppressions(filename, raw_line, linenum, error):
  """updates the global list of error-suppressions.

  parses any nolint comments on the current line, updating the global
  error_suppressions store.  reports an error if the nolint comment
  was malformed.

  args:
    filename: str, the name of the input file.
    raw_line: str, the line of input text, with comments.
    linenum: int, the number of the current line.
    error: function, an error handler.
  """
  # fixme(adonovan): "nolint(" is misparsed as nolint(*).
  matched = _re_suppression.search(raw_line)
  if matched:
    category = matched.group(1)
    if category in (none, '(*)'):  # => "suppress all"
      _error_suppressions.setdefault(none, set()).add(linenum)
    else:
      if category.startswith('(') and category.endswith(')'):
        category = category[1:-1]
        if category in _error_categories:
          _error_suppressions.setdefault(category, set()).add(linenum)
        else:
          error(filename, linenum, 'readability/nolint', 5,
                'unknown nolint error category: %s' % category)


def resetnolintsuppressions():
  "resets the set of nolint suppressions to empty."
  _error_suppressions.clear()


def iserrorsuppressedbynolint(category, linenum):
  """returns true if the specified error category is suppressed on this line.

  consults the global error_suppressions map populated by
  parsenolintsuppressions/resetnolintsuppressions.

  args:
    category: str, the category of the error.
    linenum: int, the current line number.
  returns:
    bool, true iff the error should be suppressed due to a nolint comment.
  """
  return (linenum in _error_suppressions.get(category, set()) or
          linenum in _error_suppressions.get(none, set()))

def match(pattern, s):
  """matches the string with the pattern, caching the compiled regexp."""
  # the regexp compilation caching is inlined in both match and search for
  # performance reasons; factoring it out into a separate function turns out
  # to be noticeably expensive.
  if pattern not in _regexp_compile_cache:
    _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
  return _regexp_compile_cache[pattern].match(s)


def replaceall(pattern, rep, s):
  """replaces instances of pattern in a string with a replacement.

  the compiled regex is kept in a cache shared by match and search.

  args:
    pattern: regex pattern
    rep: replacement text
    s: search string

  returns:
    string with replacements made (or original string if no replacements)
  """
  if pattern not in _regexp_compile_cache:
    _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
  return _regexp_compile_cache[pattern].sub(rep, s)


def search(pattern, s):
  """searches the string for the pattern, caching the compiled regexp."""
  if pattern not in _regexp_compile_cache:
    _regexp_compile_cache[pattern] = sre_compile.compile(pattern)
  return _regexp_compile_cache[pattern].search(s)


class _includestate(dict):
  """tracks line numbers for includes, and the order in which includes appear.

  as a dict, an _includestate object serves as a mapping between include
  filename and line number on which that file was included.

  call checknextincludeorder() once for each header in the file, passing
  in the type constants defined above. calls in an illegal order will
  raise an _includeerror with an appropriate error message.

  """
  # self._section will move monotonically through this set. if it ever
  # needs to move backwards, checknextincludeorder will raise an error.
  _initial_section = 0
  _my_h_section = 1
  _c_section = 2
  _cpp_section = 3
  _other_h_section = 4

  _type_names = {
      _c_sys_header: 'c system header',
      _cpp_sys_header: 'c++ system header',
      _likely_my_header: 'header this file implements',
      _possible_my_header: 'header this file may implement',
      _other_header: 'other header',
      }
  _section_names = {
      _initial_section: "... nothing. (this can't be an error.)",
      _my_h_section: 'a header this file implements',
      _c_section: 'c system header',
      _cpp_section: 'c++ system header',
      _other_h_section: 'other header',
      }

  def __init__(self):
    dict.__init__(self)
    self.resetsection()

  def resetsection(self):
    # the name of the current section.
    self._section = self._initial_section
    # the path of last found header.
    self._last_header = ''

  def setlastheader(self, header_path):
    self._last_header = header_path

  def canonicalizealphabeticalorder(self, header_path):
    """returns a path canonicalized for alphabetical comparison.

    - replaces "-" with "_" so they both cmp the same.
    - removes '-inl' since we don't require them to be after the main header.
    - lowercase everything, just in case.

    args:
      header_path: path to be canonicalized.

    returns:
      canonicalized path.
    """
    return header_path.replace('-inl.h', '.h').replace('-', '_').lower()

  def isinalphabeticalorder(self, clean_lines, linenum, header_path):
    """check if a header is in alphabetical order with the previous header.

    args:
      clean_lines: a cleansedlines instance containing the file.
      linenum: the number of the line to check.
      header_path: canonicalized header to be checked.

    returns:
      returns true if the header is in alphabetical order.
    """
    # if previous section is different from current section, _last_header will
    # be reset to empty string, so it's always less than current header.
    #
    # if previous line was a blank line, assume that the headers are
    # intentionally sorted the way they are.
    if (self._last_header > header_path and
        not match(r'^\s*$', clean_lines.elided[linenum - 1])):
      return false
    return true

  def checknextincludeorder(self, header_type):
    """returns a non-empty error message if the next header is out of order.

    this function also updates the internal state to be ready to check
    the next include.

    args:
      header_type: one of the _xxx_header constants defined above.

    returns:
      the empty string if the header is in the right order, or an
      error message describing what's wrong.

    """
    error_message = ('found %s after %s' %
                     (self._type_names[header_type],
                      self._section_names[self._section]))

    last_section = self._section

    if header_type == _c_sys_header:
      if self._section <= self._c_section:
        self._section = self._c_section
      else:
        self._last_header = ''
        return error_message
    elif header_type == _cpp_sys_header:
      if self._section <= self._cpp_section:
        self._section = self._cpp_section
      else:
        self._last_header = ''
        return error_message
    elif header_type == _likely_my_header:
      if self._section <= self._my_h_section:
        self._section = self._my_h_section
      else:
        self._section = self._other_h_section
    elif header_type == _possible_my_header:
      if self._section <= self._my_h_section:
        self._section = self._my_h_section
      else:
        # this will always be the fallback because we're not sure
        # enough that the header is associated with this file.
        self._section = self._other_h_section
    else:
      assert header_type == _other_header
      self._section = self._other_h_section

    if last_section != self._section:
      self._last_header = ''

    return ''


class _cpplintstate(object):
  """maintains module-wide state.."""

  def __init__(self):
    self.verbose_level = 1  # global setting.
    self.error_count = 0    # global count of reported errors
    # filters to apply when emitting error messages
    self.filters = _default_filters[:]
    self.counting = 'total'  # in what way are we counting errors?
    self.errors_by_category = {}  # string to int dict storing error counts

    # output format:
    # "emacs" - format that emacs can parse (default)
    # "vs7" - format that microsoft visual studio 7 can parse
    self.output_format = 'emacs'

  def setoutputformat(self, output_format):
    """sets the output format for errors."""
    self.output_format = output_format

  def setverboselevel(self, level):
    """sets the module's verbosity, and returns the previous setting."""
    last_verbose_level = self.verbose_level
    self.verbose_level = level
    return last_verbose_level

  def setcountingstyle(self, counting_style):
    """sets the module's counting options."""
    self.counting = counting_style

  def setfilters(self, filters):
    """sets the error-message filters.

    these filters are applied when deciding whether to emit a given
    error message.

    args:
      filters: a string of comma-separated filters (eg "+whitespace/indent").
               each filter should start with + or -; else we die.

    raises:
      valueerror: the comma-separated filters did not all start with '+' or '-'.
                  e.g. "-,+whitespace,-whitespace/indent,whitespace/badfilter"
    """
    # default filters always have less priority than the flag ones.
    self.filters = _default_filters[:]
    for filt in filters.split(','):
      clean_filt = filt.strip()
      if clean_filt:
        self.filters.append(clean_filt)
    for filt in self.filters:
      if not (filt.startswith('+') or filt.startswith('-')):
        raise valueerror('every filter in --filters must start with + or -'
                         ' (%s does not)' % filt)

  def reseterrorcounts(self):
    """sets the module's error statistic back to zero."""
    self.error_count = 0
    self.errors_by_category = {}

  def incrementerrorcount(self, category):
    """bumps the module's error statistic."""
    self.error_count += 1
    if self.counting in ('toplevel', 'detailed'):
      if self.counting != 'detailed':
        category = category.split('/')[0]
      if category not in self.errors_by_category:
        self.errors_by_category[category] = 0
      self.errors_by_category[category] += 1

  def printerrorcounts(self):
    """print a summary of errors by category, and the total."""
    for category, count in self.errors_by_category.iteritems():
      sys.stderr.write('category \'%s\' errors found: %d\n' %
                       (category, count))
    sys.stderr.write('total errors found: %d\n' % self.error_count)

_cpplint_state = _cpplintstate()


def _outputformat():
  """gets the module's output format."""
  return _cpplint_state.output_format


def _setoutputformat(output_format):
  """sets the module's output format."""
  _cpplint_state.setoutputformat(output_format)


def _verboselevel():
  """returns the module's verbosity setting."""
  return _cpplint_state.verbose_level


def _setverboselevel(level):
  """sets the module's verbosity, and returns the previous setting."""
  return _cpplint_state.setverboselevel(level)


def _setcountingstyle(level):
  """sets the module's counting options."""
  _cpplint_state.setcountingstyle(level)


def _filters():
  """returns the module's list of output filters, as a list."""
  return _cpplint_state.filters


def _setfilters(filters):
  """sets the module's error-message filters.

  these filters are applied when deciding whether to emit a given
  error message.

  args:
    filters: a string of comma-separated filters (eg "whitespace/indent").
             each filter should start with + or -; else we die.
  """
  _cpplint_state.setfilters(filters)


class _functionstate(object):
  """tracks current function name and the number of lines in its body."""

  _normal_trigger = 250  # for --v=0, 500 for --v=1, etc.
  _test_trigger = 400    # about 50% more than _normal_trigger.

  def __init__(self):
    self.in_a_function = false
    self.lines_in_function = 0
    self.current_function = ''

  def begin(self, function_name):
    """start analyzing function body.

    args:
      function_name: the name of the function being tracked.
    """
    self.in_a_function = true
    self.lines_in_function = 0
    self.current_function = function_name

  def count(self):
    """count line in current function body."""
    if self.in_a_function:
      self.lines_in_function += 1

  def check(self, error, filename, linenum):
    """report if too many lines in function body.

    args:
      error: the function to call with any errors found.
      filename: the name of the current file.
      linenum: the number of the line to check.
    """
    if match(r't(est|est)', self.current_function):
      base_trigger = self._test_trigger
    else:
      base_trigger = self._normal_trigger
    trigger = base_trigger * 2**_verboselevel()

    if self.lines_in_function > trigger:
      error_level = int(math.log(self.lines_in_function / base_trigger, 2))
      # 50 => 0, 100 => 1, 200 => 2, 400 => 3, 800 => 4, 1600 => 5, ...
      if error_level > 5:
        error_level = 5
      error(filename, linenum, 'readability/fn_size', error_level,
            'small and focused functions are preferred:'
            ' %s has %d non-comment lines'
            ' (error triggered by exceeding %d lines).'  % (
                self.current_function, self.lines_in_function, trigger))

  def end(self):
    """stop analyzing function body."""
    self.in_a_function = false


class _includeerror(exception):
  """indicates a problem with the include order in a file."""
  pass


class fileinfo:
  """provides utility functions for filenames.

  fileinfo provides easy access to the components of a file's path
  relative to the project root.
  """

  def __init__(self, filename):
    self._filename = filename

  def fullname(self):
    """make windows paths like unix."""
    return os.path.abspath(self._filename).replace('\\', '/')

  def repositoryname(self):
    """fullname after removing the local path to the repository.

    if we have a real absolute path name here we can try to do something smart:
    detecting the root of the checkout and truncating /path/to/checkout from
    the name so that we get header guards that don't include things like
    "c:\documents and settings\..." or "/home/username/..." in them and thus
    people on different computers who have checked the source out to different
    locations won't see bogus errors.
    """
    fullname = self.fullname()

    if os.path.exists(fullname):
      project_dir = os.path.dirname(fullname)

      if os.path.exists(os.path.join(project_dir, ".svn")):
        # if there's a .svn file in the current directory, we recursively look
        # up the directory tree for the top of the svn checkout
        root_dir = project_dir
        one_up_dir = os.path.dirname(root_dir)
        while os.path.exists(os.path.join(one_up_dir, ".svn")):
          root_dir = os.path.dirname(root_dir)
          one_up_dir = os.path.dirname(one_up_dir)

        prefix = os.path.commonprefix([root_dir, project_dir])
        return fullname[len(prefix) + 1:]

      # not svn <= 1.6? try to find a git, hg, or svn top level directory by
      # searching up from the current path.
      root_dir = os.path.dirname(fullname)
      while (root_dir != os.path.dirname(root_dir) and
             not os.path.exists(os.path.join(root_dir, ".git")) and
             not os.path.exists(os.path.join(root_dir, ".hg")) and
             not os.path.exists(os.path.join(root_dir, ".svn"))):
        root_dir = os.path.dirname(root_dir)

      if (os.path.exists(os.path.join(root_dir, ".git")) or
          os.path.exists(os.path.join(root_dir, ".hg")) or
          os.path.exists(os.path.join(root_dir, ".svn"))):
        prefix = os.path.commonprefix([root_dir, project_dir])
        return fullname[len(prefix) + 1:]

    # don't know what to do; header guard warnings may be wrong...
    return fullname

  def split(self):
    """splits the file into the directory, basename, and extension.

    for 'chrome/browser/browser.cc', split() would
    return ('chrome/browser', 'browser', '.cc')

    returns:
      a tuple of (directory, basename, extension).
    """

    googlename = self.repositoryname()
    project, rest = os.path.split(googlename)
    return (project,) + os.path.splitext(rest)

  def basename(self):
    """file base name - text after the final slash, before the final period."""
    return self.split()[1]

  def extension(self):
    """file extension - text following the final period."""
    return self.split()[2]

  def noextension(self):
    """file has no source file extension."""
    return '/'.join(self.split()[0:2])

  def issource(self):
    """file has a source file extension."""
    return self.extension()[1:] in ('c', 'cc', 'cpp', 'cxx')


def _shouldprinterror(category, confidence, linenum):
  """if confidence >= verbose, category passes filter and is not suppressed."""

  # there are three ways we might decide not to print an error message:
  # a "nolint(category)" comment appears in the source,
  # the verbosity level isn't high enough, or the filters filter it out.
  if iserrorsuppressedbynolint(category, linenum):
    return false
  if confidence < _cpplint_state.verbose_level:
    return false

  is_filtered = false
  for one_filter in _filters():
    if one_filter.startswith('-'):
      if category.startswith(one_filter[1:]):
        is_filtered = true
    elif one_filter.startswith('+'):
      if category.startswith(one_filter[1:]):
        is_filtered = false
    else:
      assert false  # should have been checked for in setfilter.
  if is_filtered:
    return false

  return true


def error(filename, linenum, category, confidence, message):
  """logs the fact we've found a lint error.

  we log where the error was found, and also our confidence in the error,
  that is, how certain we are this is a legitimate style regression, and
  not a misidentification or a use that's sometimes justified.

  false positives can be suppressed by the use of
  "cpplint(category)"  comments on the offending line.  these are
  parsed into _error_suppressions.

  args:
    filename: the name of the file containing the error.
    linenum: the number of the line containing the error.
    category: a string used to describe the "category" this bug
      falls under: "whitespace", say, or "runtime".  categories
      may have a hierarchy separated by slashes: "whitespace/indent".
    confidence: a number from 1-5 representing a confidence score for
      the error, with 5 meaning that we are certain of the problem,
      and 1 meaning that it could be a legitimate construct.
    message: the error message.
  """
  if _shouldprinterror(category, confidence, linenum):
    _cpplint_state.incrementerrorcount(category)
    if _cpplint_state.output_format == 'vs7':
      sys.stderr.write('%s(%s):  %s  [%s] [%d]\n' % (
          filename, linenum, message, category, confidence))
    elif _cpplint_state.output_format == 'eclipse':
      sys.stderr.write('%s:%s: warning: %s  [%s] [%d]\n' % (
          filename, linenum, message, category, confidence))
    else:
      sys.stderr.write('%s:%s:  %s  [%s] [%d]\n' % (
          filename, linenum, message, category, confidence))


# matches standard c++ escape sequences per 2.13.2.3 of the c++ standard.
_re_pattern_cleanse_line_escapes = re.compile(
    r'\\([abfnrtv?"\\\']|\d+|x[0-9a-fa-f]+)')
# matches strings.  escape codes should already be removed by escapes.
_re_pattern_cleanse_line_double_quotes = re.compile(r'"[^"]*"')
# matches characters.  escape codes should already be removed by escapes.
_re_pattern_cleanse_line_single_quotes = re.compile(r"'.'")
# matches multi-line c++ comments.
# this re is a little bit more complicated than one might expect, because we
# have to take care of space removals tools so we can handle comments inside
# statements better.
# the current rule is: we only clear spaces from both sides when we're at the
# end of the line. otherwise, we try to remove spaces from the right side,
# if this doesn't work we try on left side but only if there's a non-character
# on the right.
_re_pattern_cleanse_line_c_comments = re.compile(
    r"""(\s*/\*.*\*/\s*$|
            /\*.*\*/\s+|
         \s+/\*.*\*/(?=\w)|
            /\*.*\*/)""", re.verbose)


def iscppstring(line):
  """does line terminate so, that the next symbol is in string constant.

  this function does not consider single-line nor multi-line comments.

  args:
    line: is a partial line of code starting from the 0..n.

  returns:
    true, if next character appended to 'line' is inside a
    string constant.
  """

  line = line.replace(r'\\', 'xx')  # after this, \\" does not match to \"
  return ((line.count('"') - line.count(r'\"') - line.count("'\"'")) & 1) == 1


def cleanserawstrings(raw_lines):
  """removes c++11 raw strings from lines.

    before:
      static const char kdata[] = r"(
          multi-line string
          )";

    after:
      static const char kdata[] = ""
          (replaced by blank line)
          "";

  args:
    raw_lines: list of raw lines.

  returns:
    list of lines with c++11 raw strings replaced by empty strings.
  """

  delimiter = none
  lines_without_raw_strings = []
  for line in raw_lines:
    if delimiter:
      # inside a raw string, look for the end
      end = line.find(delimiter)
      if end >= 0:
        # found the end of the string, match leading space for this
        # line and resume copying the original lines, and also insert
        # a "" on the last line.
        leading_space = match(r'^(\s*)\s', line)
        line = leading_space.group(1) + '""' + line[end + len(delimiter):]
        delimiter = none
      else:
        # haven't found the end yet, append a blank line.
        line = ''

    else:
      # look for beginning of a raw string.
      # see 2.14.15 [lex.string] for syntax.
      matched = match(r'^(.*)\b(?:r|u8r|ur|ur|lr)"([^\s\\()]*)\((.*)$', line)
      if matched:
        delimiter = ')' + matched.group(2) + '"'

        end = matched.group(3).find(delimiter)
        if end >= 0:
          # raw string ended on same line
          line = (matched.group(1) + '""' +
                  matched.group(3)[end + len(delimiter):])
          delimiter = none
        else:
          # start of a multi-line raw string
          line = matched.group(1) + '""'

    lines_without_raw_strings.append(line)

  # todo(unknown): if delimiter is not none here, we might want to
  # emit a warning for unterminated string.
  return lines_without_raw_strings


def findnextmultilinecommentstart(lines, lineix):
  """find the beginning marker for a multiline comment."""
  while lineix < len(lines):
    if lines[lineix].strip().startswith('/*'):
      # only return this marker if the comment goes beyond this line
      if lines[lineix].strip().find('*/', 2) < 0:
        return lineix
    lineix += 1
  return len(lines)


def findnextmultilinecommentend(lines, lineix):
  """we are inside a comment, find the end marker."""
  while lineix < len(lines):
    if lines[lineix].strip().endswith('*/'):
      return lineix
    lineix += 1
  return len(lines)


def removemultilinecommentsfromrange(lines, begin, end):
  """clears a range of lines for multi-line comments."""
  # having // dummy comments makes the lines non-empty, so we will not get
  # unnecessary blank line warnings later in the code.
  for i in range(begin, end):
    lines[i] = '// dummy'


def removemultilinecomments(filename, lines, error):
  """removes multiline (c-style) comments from lines."""
  lineix = 0
  while lineix < len(lines):
    lineix_begin = findnextmultilinecommentstart(lines, lineix)
    if lineix_begin >= len(lines):
      return
    lineix_end = findnextmultilinecommentend(lines, lineix_begin)
    if lineix_end >= len(lines):
      error(filename, lineix_begin + 1, 'readability/multiline_comment', 5,
            'could not find end of multi-line comment')
      return
    removemultilinecommentsfromrange(lines, lineix_begin, lineix_end + 1)
    lineix = lineix_end + 1


def cleansecomments(line):
  """removes //-comments and single-line c-style /* */ comments.

  args:
    line: a line of c++ source.

  returns:
    the line with single-line comments removed.
  """
  commentpos = line.find('//')
  if commentpos != -1 and not iscppstring(line[:commentpos]):
    line = line[:commentpos].rstrip()
  # get rid of /* ... */
  return _re_pattern_cleanse_line_c_comments.sub('', line)


class cleansedlines(object):
  """holds 3 copies of all lines with different preprocessing applied to them.

  1) elided member contains lines without strings and comments,
  2) lines member contains lines without comments, and
  3) raw_lines member contains all the lines without processing.
  all these three members are of <type 'list'>, and of the same length.
  """

  def __init__(self, lines):
    self.elided = []
    self.lines = []
    self.raw_lines = lines
    self.num_lines = len(lines)
    self.lines_without_raw_strings = cleanserawstrings(lines)
    for linenum in range(len(self.lines_without_raw_strings)):
      self.lines.append(cleansecomments(
          self.lines_without_raw_strings[linenum]))
      elided = self._collapsestrings(self.lines_without_raw_strings[linenum])
      self.elided.append(cleansecomments(elided))

  def numlines(self):
    """returns the number of lines represented."""
    return self.num_lines

  @staticmethod
  def _collapsestrings(elided):
    """collapses strings and chars on a line to simple "" or '' blocks.

    we nix strings first so we're not fooled by text like '"http://"'

    args:
      elided: the line being processed.

    returns:
      the line with collapsed strings.
    """
    if not _re_pattern_include.match(elided):
      # remove escaped characters first to make quote/single quote collapsing
      # basic.  things that look like escaped characters shouldn't occur
      # outside of strings and chars.
      elided = _re_pattern_cleanse_line_escapes.sub('', elided)
      elided = _re_pattern_cleanse_line_single_quotes.sub("''", elided)
      elided = _re_pattern_cleanse_line_double_quotes.sub('""', elided)
    return elided


def findendofexpressioninline(line, startpos, depth, startchar, endchar):
  """find the position just after the matching endchar.

  args:
    line: a cleansedlines line.
    startpos: start searching at this position.
    depth: nesting level at startpos.
    startchar: expression opening character.
    endchar: expression closing character.

  returns:
    on finding matching endchar: (index just after matching endchar, 0)
    otherwise: (-1, new depth at end of this line)
  """
  for i in xrange(startpos, len(line)):
    if line[i] == startchar:
      depth += 1
    elif line[i] == endchar:
      depth -= 1
      if depth == 0:
        return (i + 1, 0)
  return (-1, depth)


def closeexpression(clean_lines, linenum, pos):
  """if input points to ( or { or [ or <, finds the position that closes it.

  if lines[linenum][pos] points to a '(' or '{' or '[' or '<', finds the
  linenum/pos that correspond to the closing of the expression.

  args:
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    pos: a position on the line.

  returns:
    a tuple (line, linenum, pos) pointer *past* the closing brace, or
    (line, len(lines), -1) if we never find a close.  note we ignore
    strings and comments when matching; and the line we return is the
    'cleansed' line at linenum.
  """

  line = clean_lines.elided[linenum]
  startchar = line[pos]
  if startchar not in '({[<':
    return (line, clean_lines.numlines(), -1)
  if startchar == '(': endchar = ')'
  if startchar == '[': endchar = ']'
  if startchar == '{': endchar = '}'
  if startchar == '<': endchar = '>'

  # check first line
  (end_pos, num_open) = findendofexpressioninline(
      line, pos, 0, startchar, endchar)
  if end_pos > -1:
    return (line, linenum, end_pos)

  # continue scanning forward
  while linenum < clean_lines.numlines() - 1:
    linenum += 1
    line = clean_lines.elided[linenum]
    (end_pos, num_open) = findendofexpressioninline(
        line, 0, num_open, startchar, endchar)
    if end_pos > -1:
      return (line, linenum, end_pos)

  # did not find endchar before end of file, give up
  return (line, clean_lines.numlines(), -1)


def findstartofexpressioninline(line, endpos, depth, startchar, endchar):
  """find position at the matching startchar.

  this is almost the reverse of findendofexpressioninline, but note
  that the input position and returned position differs by 1.

  args:
    line: a cleansedlines line.
    endpos: start searching at this position.
    depth: nesting level at endpos.
    startchar: expression opening character.
    endchar: expression closing character.

  returns:
    on finding matching startchar: (index at matching startchar, 0)
    otherwise: (-1, new depth at beginning of this line)
  """
  for i in xrange(endpos, -1, -1):
    if line[i] == endchar:
      depth += 1
    elif line[i] == startchar:
      depth -= 1
      if depth == 0:
        return (i, 0)
  return (-1, depth)


def reversecloseexpression(clean_lines, linenum, pos):
  """if input points to ) or } or ] or >, finds the position that opens it.

  if lines[linenum][pos] points to a ')' or '}' or ']' or '>', finds the
  linenum/pos that correspond to the opening of the expression.

  args:
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    pos: a position on the line.

  returns:
    a tuple (line, linenum, pos) pointer *at* the opening brace, or
    (line, 0, -1) if we never find the matching opening brace.  note
    we ignore strings and comments when matching; and the line we
    return is the 'cleansed' line at linenum.
  """
  line = clean_lines.elided[linenum]
  endchar = line[pos]
  if endchar not in ')}]>':
    return (line, 0, -1)
  if endchar == ')': startchar = '('
  if endchar == ']': startchar = '['
  if endchar == '}': startchar = '{'
  if endchar == '>': startchar = '<'

  # check last line
  (start_pos, num_open) = findstartofexpressioninline(
      line, pos, 0, startchar, endchar)
  if start_pos > -1:
    return (line, linenum, start_pos)

  # continue scanning backward
  while linenum > 0:
    linenum -= 1
    line = clean_lines.elided[linenum]
    (start_pos, num_open) = findstartofexpressioninline(
        line, len(line) - 1, num_open, startchar, endchar)
    if start_pos > -1:
      return (line, linenum, start_pos)

  # did not find startchar before beginning of file, give up
  return (line, 0, -1)


def checkforcopyright(filename, lines, error):
  """logs an error if no copyright message appears at the top of the file."""

  # we'll say it should occur by line 10. don't forget there's a
  # dummy line at the front.
  for line in xrange(1, min(len(lines), 11)):
    if re.search(r'copyright', lines[line], re.i): break
  else:                       # means no copyright line was found
    error(filename, 0, 'legal/copyright', 5,
          'no copyright message found.  '
          'you should have a line: "copyright [year] <copyright owner>"')


def getheaderguardcppvariable(filename):
  """returns the cpp variable that should be used as a header guard.

  args:
    filename: the name of a c++ header file.

  returns:
    the cpp variable that should be used as a header guard in the
    named file.

  """

  # restores original filename in case that cpplint is invoked from emacs's
  # flymake.
  filename = re.sub(r'_flymake\.h$', '.h', filename)
  filename = re.sub(r'/\.flymake/([^/]*)$', r'/\1', filename)

  fileinfo = fileinfo(filename)
  file_path_from_root = fileinfo.repositoryname()
  if _root:
    file_path_from_root = re.sub('^' + _root + os.sep, '', file_path_from_root)
  return re.sub(r'[-./\s]', '_', file_path_from_root).upper() + '_'


def checkforheaderguard(filename, lines, error):
  """checks that the file contains a header guard.

  logs an error if no #ifndef header guard is present.  for other
  headers, checks that the full pathname is used.

  args:
    filename: the name of the c++ header file.
    lines: an array of strings, each representing a line of the file.
    error: the function to call with any errors found.
  """

  cppvar = getheaderguardcppvariable(filename)

  ifndef = none
  ifndef_linenum = 0
  define = none
  endif = none
  endif_linenum = 0
  for linenum, line in enumerate(lines):
    # already been well guarded, no need for further checking.
    if line.strip() == "#pragma once":
        return
    linesplit = line.split()
    if len(linesplit) >= 2:
      # find the first occurrence of #ifndef and #define, save arg
      if not ifndef and linesplit[0] == '#ifndef':
        # set ifndef to the header guard presented on the #ifndef line.
        ifndef = linesplit[1]
        ifndef_linenum = linenum
      if not define and linesplit[0] == '#define':
        define = linesplit[1]
    # find the last occurrence of #endif, save entire line
    if line.startswith('#endif'):
      endif = line
      endif_linenum = linenum

  if not ifndef:
    error(filename, 0, 'build/header_guard', 5,
          'no #ifndef header guard found, suggested cpp variable is: %s' %
          cppvar)
    return

  if not define:
    error(filename, 0, 'build/header_guard', 5,
          'no #define header guard found, suggested cpp variable is: %s' %
          cppvar)
    return

  # the guard should be path_file_h_, but we also allow path_file_h__
  # for backward compatibility.
  if ifndef != cppvar:
    error_level = 0
    if ifndef != cppvar + '_':
      error_level = 5

    parsenolintsuppressions(filename, lines[ifndef_linenum], ifndef_linenum,
                            error)
    error(filename, ifndef_linenum, 'build/header_guard', error_level,
          '#ifndef header guard has wrong style, please use: %s' % cppvar)

  if define != ifndef:
    error(filename, 0, 'build/header_guard', 5,
          '#ifndef and #define don\'t match, suggested cpp variable is: %s' %
          cppvar)
    return

  if endif != ('#endif  // %s' % cppvar):
    error_level = 0
    if endif != ('#endif  // %s' % (cppvar + '_')):
      error_level = 5

    parsenolintsuppressions(filename, lines[endif_linenum], endif_linenum,
                            error)
    error(filename, endif_linenum, 'build/header_guard', error_level,
          '#endif line should be "#endif  // %s"' % cppvar)


def checkforbadcharacters(filename, lines, error):
  """logs an error for each line containing bad characters.

  two kinds of bad characters:

  1. unicode replacement characters: these indicate that either the file
  contained invalid utf-8 (likely) or unicode replacement characters (which
  it shouldn't).  note that it's possible for this to throw off line
  numbering if the invalid utf-8 occurred adjacent to a newline.

  2. nul bytes.  these are problematic for some tools.

  args:
    filename: the name of the current file.
    lines: an array of strings, each representing a line of the file.
    error: the function to call with any errors found.
  """
  for linenum, line in enumerate(lines):
    if u'\ufffd' in line:
      error(filename, linenum, 'readability/utf8', 5,
            'line contains invalid utf-8 (or unicode replacement character).')
    if '\0' in line:
      error(filename, linenum, 'readability/nul', 5, 'line contains nul byte.')


def checkfornewlineateof(filename, lines, error):
  """logs an error if there is no newline char at the end of the file.

  args:
    filename: the name of the current file.
    lines: an array of strings, each representing a line of the file.
    error: the function to call with any errors found.
  """

  # the array lines() was created by adding two newlines to the
  # original file (go figure), then splitting on \n.
  # to verify that the file ends in \n, we just have to make sure the
  # last-but-two element of lines() exists and is empty.
  if len(lines) < 3 or lines[-2]:
    error(filename, len(lines) - 2, 'whitespace/ending_newline', 5,
          'could not find a newline character at the end of the file.')


def checkformultilinecommentsandstrings(filename, clean_lines, linenum, error):
  """logs an error if we see /* ... */ or "..." that extend past one line.

  /* ... */ comments are legit inside macros, for one line.
  otherwise, we prefer // comments, so it's ok to warn about the
  other.  likewise, it's ok for strings to extend across multiple
  lines, as long as a line continuation character (backslash)
  terminates each line. although not currently prohibited by the c++
  style guide, it's ugly and unnecessary. we don't do well with either
  in this lint program, so we warn about both.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]

  # remove all \\ (escaped backslashes) from the line. they are ok, and the
  # second (escaped) slash may trigger later \" detection erroneously.
  line = line.replace('\\\\', '')

  if line.count('/*') > line.count('*/'):
    error(filename, linenum, 'readability/multiline_comment', 5,
          'complex multi-line /*...*/-style comment found. '
          'lint may give bogus warnings.  '
          'consider replacing these with //-style comments, '
          'with #if 0...#endif, '
          'or with more clearly structured multi-line comments.')

  if (line.count('"') - line.count('\\"')) % 2:
    error(filename, linenum, 'readability/multiline_string', 5,
          'multi-line string ("...") found.  this lint script doesn\'t '
          'do well with such strings, and may give bogus warnings.  '
          'use c++11 raw strings or concatenation instead.')


threading_list = (
    ('asctime(', 'asctime_r('),
    ('ctime(', 'ctime_r('),
    ('getgrgid(', 'getgrgid_r('),
    ('getgrnam(', 'getgrnam_r('),
    ('getlogin(', 'getlogin_r('),
    ('getpwnam(', 'getpwnam_r('),
    ('getpwuid(', 'getpwuid_r('),
    ('gmtime(', 'gmtime_r('),
    ('localtime(', 'localtime_r('),
    ('rand(', 'rand_r('),
    ('strtok(', 'strtok_r('),
    ('ttyname(', 'ttyname_r('),
    )


def checkposixthreading(filename, clean_lines, linenum, error):
  """checks for calls to thread-unsafe functions.

  much code has been originally written without consideration of
  multi-threading. also, engineers are relying on their old experience;
  they have learned posix before threading extensions were added. these
  tests guide the engineers to use thread-safe functions (when using
  posix directly).

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]
  for single_thread_function, multithread_safe_function in threading_list:
    ix = line.find(single_thread_function)
    # comparisons made explicit for clarity -- pylint: disable=g-explicit-bool-comparison
    if ix >= 0 and (ix == 0 or (not line[ix - 1].isalnum() and
                                line[ix - 1] not in ('_', '.', '>'))):
      error(filename, linenum, 'runtime/threadsafe_fn', 2,
            'consider using ' + multithread_safe_function +
            '...) instead of ' + single_thread_function +
            '...) for improved thread safety.')


def checkvlogarguments(filename, clean_lines, linenum, error):
  """checks that vlog() is only used for defining a logging level.

  for example, vlog(2) is correct. vlog(info), vlog(warning), vlog(error), and
  vlog(fatal) are not.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]
  if search(r'\bvlog\((info|error|warning|dfatal|fatal)\)', line):
    error(filename, linenum, 'runtime/vlog', 5,
          'vlog() should be used with numeric verbosity level.  '
          'use log() if you want symbolic severity levels.')


# matches invalid increment: *count++, which moves pointer instead of
# incrementing a value.
_re_pattern_invalid_increment = re.compile(
    r'^\s*\*\w+(\+\+|--);')


def checkinvalidincrement(filename, clean_lines, linenum, error):
  """checks for invalid increment *count++.

  for example following function:
  void increment_counter(int* count) {
    *count++;
  }
  is invalid, because it effectively does count++, moving pointer, and should
  be replaced with ++*count, (*count)++ or *count += 1.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]
  if _re_pattern_invalid_increment.match(line):
    error(filename, linenum, 'runtime/invalid_increment', 5,
          'changing pointer instead of value (or unused value of operator*).')


class _blockinfo(object):
  """stores information about a generic block of code."""

  def __init__(self, seen_open_brace):
    self.seen_open_brace = seen_open_brace
    self.open_parentheses = 0
    self.inline_asm = _no_asm

  def checkbegin(self, filename, clean_lines, linenum, error):
    """run checks that applies to text up to the opening brace.

    this is mostly for checking the text after the class identifier
    and the "{", usually where the base class is specified.  for other
    blocks, there isn't much to check, so we always pass.

    args:
      filename: the name of the current file.
      clean_lines: a cleansedlines instance containing the file.
      linenum: the number of the line to check.
      error: the function to call with any errors found.
    """
    pass

  def checkend(self, filename, clean_lines, linenum, error):
    """run checks that applies to text after the closing brace.

    this is mostly used for checking end of namespace comments.

    args:
      filename: the name of the current file.
      clean_lines: a cleansedlines instance containing the file.
      linenum: the number of the line to check.
      error: the function to call with any errors found.
    """
    pass


class _classinfo(_blockinfo):
  """stores information about a class."""

  def __init__(self, name, class_or_struct, clean_lines, linenum):
    _blockinfo.__init__(self, false)
    self.name = name
    self.starting_linenum = linenum
    self.is_derived = false
    if class_or_struct == 'struct':
      self.access = 'public'
      self.is_struct = true
    else:
      self.access = 'private'
      self.is_struct = false

    # remember initial indentation level for this class.  using raw_lines here
    # instead of elided to account for leading comments.
    initial_indent = match(r'^( *)\s', clean_lines.raw_lines[linenum])
    if initial_indent:
      self.class_indent = len(initial_indent.group(1))
    else:
      self.class_indent = 0

    # try to find the end of the class.  this will be confused by things like:
    #   class a {
    #   } *x = { ...
    #
    # but it's still good enough for checksectionspacing.
    self.last_line = 0
    depth = 0
    for i in range(linenum, clean_lines.numlines()):
      line = clean_lines.elided[i]
      depth += line.count('{') - line.count('}')
      if not depth:
        self.last_line = i
        break

  def checkbegin(self, filename, clean_lines, linenum, error):
    # look for a bare ':'
    if search('(^|[^:]):($|[^:])', clean_lines.elided[linenum]):
      self.is_derived = true

  def checkend(self, filename, clean_lines, linenum, error):
    # check that closing brace is aligned with beginning of the class.
    # only do this if the closing brace is indented by only whitespaces.
    # this means we will not check single-line class definitions.
    indent = match(r'^( *)\}', clean_lines.elided[linenum])
    if indent and len(indent.group(1)) != self.class_indent:
      if self.is_struct:
        parent = 'struct ' + self.name
      else:
        parent = 'class ' + self.name
      error(filename, linenum, 'whitespace/indent', 3,
            'closing brace should be aligned with beginning of %s' % parent)


class _namespaceinfo(_blockinfo):
  """stores information about a namespace."""

  def __init__(self, name, linenum):
    _blockinfo.__init__(self, false)
    self.name = name or ''
    self.starting_linenum = linenum

  def checkend(self, filename, clean_lines, linenum, error):
    """check end of namespace comments."""
    line = clean_lines.raw_lines[linenum]

    # check how many lines is enclosed in this namespace.  don't issue
    # warning for missing namespace comments if there aren't enough
    # lines.  however, do apply checks if there is already an end of
    # namespace comment and it's incorrect.
    #
    # todo(unknown): we always want to check end of namespace comments
    # if a namespace is large, but sometimes we also want to apply the
    # check if a short namespace contained nontrivial things (something
    # other than forward declarations).  there is currently no logic on
    # deciding what these nontrivial things are, so this check is
    # triggered by namespace size only, which works most of the time.
    if (linenum - self.starting_linenum < 10
        and not match(r'};*\s*(//|/\*).*\bnamespace\b', line)):
      return

    # look for matching comment at end of namespace.
    #
    # note that we accept c style "/* */" comments for terminating
    # namespaces, so that code that terminate namespaces inside
    # preprocessor macros can be cpplint clean.
    #
    # we also accept stuff like "// end of namespace <name>." with the
    # period at the end.
    #
    # besides these, we don't accept anything else, otherwise we might
    # get false negatives when existing comment is a substring of the
    # expected namespace.
    if self.name:
      # named namespace
      if not match((r'};*\s*(//|/\*).*\bnamespace\s+' + re.escape(self.name) +
                    r'[\*/\.\\\s]*$'),
                   line):
        error(filename, linenum, 'readability/namespace', 5,
              'namespace should be terminated with "// namespace %s"' %
              self.name)
    else:
      # anonymous namespace
      if not match(r'};*\s*(//|/\*).*\bnamespace[\*/\.\\\s]*$', line):
        error(filename, linenum, 'readability/namespace', 5,
              'namespace should be terminated with "// namespace"')


class _preprocessorinfo(object):
  """stores checkpoints of nesting stacks when #if/#else is seen."""

  def __init__(self, stack_before_if):
    # the entire nesting stack before #if
    self.stack_before_if = stack_before_if

    # the entire nesting stack up to #else
    self.stack_before_else = []

    # whether we have already seen #else or #elif
    self.seen_else = false


class _nestingstate(object):
  """holds states related to parsing braces."""

  def __init__(self):
    # stack for tracking all braces.  an object is pushed whenever we
    # see a "{", and popped when we see a "}".  only 3 types of
    # objects are possible:
    # - _classinfo: a class or struct.
    # - _namespaceinfo: a namespace.
    # - _blockinfo: some other type of block.
    self.stack = []

    # stack of _preprocessorinfo objects.
    self.pp_stack = []

  def seenopenbrace(self):
    """check if we have seen the opening brace for the innermost block.

    returns:
      true if we have seen the opening brace, false if the innermost
      block is still expecting an opening brace.
    """
    return (not self.stack) or self.stack[-1].seen_open_brace

  def innamespacebody(self):
    """check if we are currently one level inside a namespace body.

    returns:
      true if top of the stack is a namespace block, false otherwise.
    """
    return self.stack and isinstance(self.stack[-1], _namespaceinfo)

  def updatepreprocessor(self, line):
    """update preprocessor stack.

    we need to handle preprocessors due to classes like this:
      #ifdef swig
      struct resultdetailspageelementextensionpoint {
      #else
      struct resultdetailspageelementextensionpoint : public extension {
      #endif

    we make the following assumptions (good enough for most files):
    - preprocessor condition evaluates to true from #if up to first
      #else/#elif/#endif.

    - preprocessor condition evaluates to false from #else/#elif up
      to #endif.  we still perform lint checks on these lines, but
      these do not affect nesting stack.

    args:
      line: current line to check.
    """
    if match(r'^\s*#\s*(if|ifdef|ifndef)\b', line):
      # beginning of #if block, save the nesting stack here.  the saved
      # stack will allow us to restore the parsing state in the #else case.
      self.pp_stack.append(_preprocessorinfo(copy.deepcopy(self.stack)))
    elif match(r'^\s*#\s*(else|elif)\b', line):
      # beginning of #else block
      if self.pp_stack:
        if not self.pp_stack[-1].seen_else:
          # this is the first #else or #elif block.  remember the
          # whole nesting stack up to this point.  this is what we
          # keep after the #endif.
          self.pp_stack[-1].seen_else = true
          self.pp_stack[-1].stack_before_else = copy.deepcopy(self.stack)

        # restore the stack to how it was before the #if
        self.stack = copy.deepcopy(self.pp_stack[-1].stack_before_if)
      else:
        # todo(unknown): unexpected #else, issue warning?
        pass
    elif match(r'^\s*#\s*endif\b', line):
      # end of #if or #else blocks.
      if self.pp_stack:
        # if we saw an #else, we will need to restore the nesting
        # stack to its former state before the #else, otherwise we
        # will just continue from where we left off.
        if self.pp_stack[-1].seen_else:
          # here we can just use a shallow copy since we are the last
          # reference to it.
          self.stack = self.pp_stack[-1].stack_before_else
        # drop the corresponding #if
        self.pp_stack.pop()
      else:
        # todo(unknown): unexpected #endif, issue warning?
        pass

  def update(self, filename, clean_lines, linenum, error):
    """update nesting state with current line.

    args:
      filename: the name of the current file.
      clean_lines: a cleansedlines instance containing the file.
      linenum: the number of the line to check.
      error: the function to call with any errors found.
    """
    line = clean_lines.elided[linenum]

    # update pp_stack first
    self.updatepreprocessor(line)

    # count parentheses.  this is to avoid adding struct arguments to
    # the nesting stack.
    if self.stack:
      inner_block = self.stack[-1]
      depth_change = line.count('(') - line.count(')')
      inner_block.open_parentheses += depth_change

      # also check if we are starting or ending an inline assembly block.
      if inner_block.inline_asm in (_no_asm, _end_asm):
        if (depth_change != 0 and
            inner_block.open_parentheses == 1 and
            _match_asm.match(line)):
          # enter assembly block
          inner_block.inline_asm = _inside_asm
        else:
          # not entering assembly block.  if previous line was _end_asm,
          # we will now shift to _no_asm state.
          inner_block.inline_asm = _no_asm
      elif (inner_block.inline_asm == _inside_asm and
            inner_block.open_parentheses == 0):
        # exit assembly block
        inner_block.inline_asm = _end_asm

    # consume namespace declaration at the beginning of the line.  do
    # this in a loop so that we catch same line declarations like this:
    #   namespace proto2 { namespace bridge { class messageset; } }
    while true:
      # match start of namespace.  the "\b\s*" below catches namespace
      # declarations even if it weren't followed by a whitespace, this
      # is so that we don't confuse our namespace checker.  the
      # missing spaces will be flagged by checkspacing.
      namespace_decl_match = match(r'^\s*namespace\b\s*([:\w]+)?(.*)$', line)
      if not namespace_decl_match:
        break

      new_namespace = _namespaceinfo(namespace_decl_match.group(1), linenum)
      self.stack.append(new_namespace)

      line = namespace_decl_match.group(2)
      if line.find('{') != -1:
        new_namespace.seen_open_brace = true
        line = line[line.find('{') + 1:]

    # look for a class declaration in whatever is left of the line
    # after parsing namespaces.  the regexp accounts for decorated classes
    # such as in:
    #   class lockable api object {
    #   };
    #
    # templates with class arguments may confuse the parser, for example:
    #   template <class t
    #             class comparator = less<t>,
    #             class vector = vector<t> >
    #   class heapqueue {
    #
    # because this parser has no nesting state about templates, by the
    # time it saw "class comparator", it may think that it's a new class.
    # nested templates have a similar problem:
    #   template <
    #       typename exportedtype,
    #       typename tupletype,
    #       template <typename, typename> class impltemplate>
    #
    # to avoid these cases, we ignore classes that are followed by '=' or '>'
    class_decl_match = match(
        r'\s*(template\s*<[\w\s<>,:]*>\s*)?'
        r'(class|struct)\s+([a-z_]+\s+)*(\w+(?:::\w+)*)'
        r'(([^=>]|<[^<>]*>|<[^<>]*<[^<>]*>\s*>)*)$', line)
    if (class_decl_match and
        (not self.stack or self.stack[-1].open_parentheses == 0)):
      self.stack.append(_classinfo(
          class_decl_match.group(4), class_decl_match.group(2),
          clean_lines, linenum))
      line = class_decl_match.group(5)

    # if we have not yet seen the opening brace for the innermost block,
    # run checks here.
    if not self.seenopenbrace():
      self.stack[-1].checkbegin(filename, clean_lines, linenum, error)

    # update access control if we are inside a class/struct
    if self.stack and isinstance(self.stack[-1], _classinfo):
      classinfo = self.stack[-1]
      access_match = match(
          r'^(.*)\b(public|private|protected|signals)(\s+(?:slots\s*)?)?'
          r':(?:[^:]|$)',
          line)
      if access_match:
        classinfo.access = access_match.group(2)

        # check that access keywords are indented +1 space.  skip this
        # check if the keywords are not preceded by whitespaces.
        indent = access_match.group(1)
        if (len(indent) != classinfo.class_indent + 1 and
            match(r'^\s*$', indent)):
          if classinfo.is_struct:
            parent = 'struct ' + classinfo.name
          else:
            parent = 'class ' + classinfo.name
          slots = ''
          if access_match.group(3):
            slots = access_match.group(3)
          error(filename, linenum, 'whitespace/indent', 3,
                '%s%s: should be indented +1 space inside %s' % (
                    access_match.group(2), slots, parent))

    # consume braces or semicolons from what's left of the line
    while true:
      # match first brace, semicolon, or closed parenthesis.
      matched = match(r'^[^{;)}]*([{;)}])(.*)$', line)
      if not matched:
        break

      token = matched.group(1)
      if token == '{':
        # if namespace or class hasn't seen a opening brace yet, mark
        # namespace/class head as complete.  push a new block onto the
        # stack otherwise.
        if not self.seenopenbrace():
          self.stack[-1].seen_open_brace = true
        else:
          self.stack.append(_blockinfo(true))
          if _match_asm.match(line):
            self.stack[-1].inline_asm = _block_asm
      elif token == ';' or token == ')':
        # if we haven't seen an opening brace yet, but we already saw
        # a semicolon, this is probably a forward declaration.  pop
        # the stack for these.
        #
        # similarly, if we haven't seen an opening brace yet, but we
        # already saw a closing parenthesis, then these are probably
        # function arguments with extra "class" or "struct" keywords.
        # also pop these stack for these.
        if not self.seenopenbrace():
          self.stack.pop()
      else:  # token == '}'
        # perform end of block checks and pop the stack.
        if self.stack:
          self.stack[-1].checkend(filename, clean_lines, linenum, error)
          self.stack.pop()
      line = matched.group(2)

  def innermostclass(self):
    """get class info on the top of the stack.

    returns:
      a _classinfo object if we are inside a class, or none otherwise.
    """
    for i in range(len(self.stack), 0, -1):
      classinfo = self.stack[i - 1]
      if isinstance(classinfo, _classinfo):
        return classinfo
    return none

  def checkcompletedblocks(self, filename, error):
    """checks that all classes and namespaces have been completely parsed.

    call this when all lines in a file have been processed.
    args:
      filename: the name of the current file.
      error: the function to call with any errors found.
    """
    # note: this test can result in false positives if #ifdef constructs
    # get in the way of brace matching. see the testbuildclass test in
    # cpplint_unittest.py for an example of this.
    for obj in self.stack:
      if isinstance(obj, _classinfo):
        error(filename, obj.starting_linenum, 'build/class', 5,
              'failed to find complete declaration of class %s' %
              obj.name)
      elif isinstance(obj, _namespaceinfo):
        error(filename, obj.starting_linenum, 'build/namespaces', 5,
              'failed to find complete declaration of namespace %s' %
              obj.name)


def checkfornonstandardconstructs(filename, clean_lines, linenum,
                                  nesting_state, error):
  r"""logs an error if we see certain non-ansi constructs ignored by gcc-2.

  complain about several constructs which gcc-2 accepts, but which are
  not standard c++.  warning about these in lint is one way to ease the
  transition to new compilers.
  - put storage class first (e.g. "static const" instead of "const static").
  - "%lld" instead of %qd" in printf-type functions.
  - "%1$d" is non-standard in printf-type functions.
  - "\%" is an undefined character escape sequence.
  - text after #endif is not allowed.
  - invalid inner-style forward declaration.
  - >? and <? operators, and their >?= and <?= cousins.

  additionally, check for constructor/destructor style violations and reference
  members, as it is very convenient to do so while checking for
  gcc-2 compliance.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: a callable to which errors are reported, which takes 4 arguments:
           filename, line number, error level, and message
  """

  # remove comments from the line, but leave in strings for now.
  line = clean_lines.lines[linenum]

  if search(r'printf\s*\(.*".*%[-+ ]?\d*q', line):
    error(filename, linenum, 'runtime/printf_format', 3,
          '%q in format strings is deprecated.  use %ll instead.')

  if search(r'printf\s*\(.*".*%\d+\$', line):
    error(filename, linenum, 'runtime/printf_format', 2,
          '%n$ formats are unconventional.  try rewriting to avoid them.')

  # remove escaped backslashes before looking for undefined escapes.
  line = line.replace('\\\\', '')

  if search(r'("|\').*\\(%|\[|\(|{)', line):
    error(filename, linenum, 'build/printf_format', 3,
          '%, [, (, and { are undefined character escapes.  unescape them.')

  # for the rest, work with both comments and strings removed.
  line = clean_lines.elided[linenum]

  if search(r'\b(const|volatile|void|char|short|int|long'
            r'|float|double|signed|unsigned'
            r'|schar|u?int8|u?int16|u?int32|u?int64)'
            r'\s+(register|static|extern|typedef)\b',
            line):
    error(filename, linenum, 'build/storage_class', 5,
          'storage class (static, extern, typedef, etc) should be first.')

  if match(r'\s*#\s*endif\s*[^/\s]+', line):
    error(filename, linenum, 'build/endif_comment', 5,
          'uncommented text after #endif is non-standard.  use a comment.')

  if match(r'\s*class\s+(\w+\s*::\s*)+\w+\s*;', line):
    error(filename, linenum, 'build/forward_decl', 5,
          'inner-style forward declarations are invalid.  remove this line.')

  if search(r'(\w+|[+-]?\d+(\.\d*)?)\s*(<|>)\?=?\s*(\w+|[+-]?\d+)(\.\d*)?',
            line):
    error(filename, linenum, 'build/deprecated', 3,
          '>? and <? (max and min) operators are non-standard and deprecated.')

  if search(r'^\s*const\s*string\s*&\s*\w+\s*;', line):
    # todo(unknown): could it be expanded safely to arbitrary references,
    # without triggering too many false positives? the first
    # attempt triggered 5 warnings for mostly benign code in the regtest, hence
    # the restriction.
    # here's the original regexp, for the reference:
    # type_name = r'\w+((\s*::\s*\w+)|(\s*<\s*\w+?\s*>))?'
    # r'\s*const\s*' + type_name + '\s*&\s*\w+\s*;'
    error(filename, linenum, 'runtime/member_string_references', 2,
          'const string& members are dangerous. it is much better to use '
          'alternatives, such as pointers or simple constants.')

  # everything else in this function operates on class declarations.
  # return early if the top of the nesting stack is not a class, or if
  # the class head is not completed yet.
  classinfo = nesting_state.innermostclass()
  if not classinfo or not classinfo.seen_open_brace:
    return

  # the class may have been declared with namespace or classname qualifiers.
  # the constructor and destructor will not have those qualifiers.
  base_classname = classinfo.name.split('::')[-1]

  # look for single-argument constructors that aren't marked explicit.
  # technically a valid construct, but against style.
  args = match(r'\s+(?:inline\s+)?%s\s*\(([^,()]+)\)'
               % re.escape(base_classname),
               line)
  if (args and
      args.group(1) != 'void' and
      not match(r'(const\s+)?%s(\s+const)?\s*(?:<\w+>\s*)?&'
                % re.escape(base_classname), args.group(1).strip())):
    error(filename, linenum, 'runtime/explicit', 5,
          'single-argument constructors should be marked explicit.')


def checkspacingforfunctioncall(filename, line, linenum, error):
  """checks for the correctness of various spacing around function calls.

  args:
    filename: the name of the current file.
    line: the text of the line to check.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """

  # since function calls often occur inside if/for/while/switch
  # expressions - which have their own, more liberal conventions - we
  # first see if we should be looking inside such an expression for a
  # function call, to which we can apply more strict standards.
  fncall = line    # if there's no control flow construct, look at whole line
  for pattern in (r'\bif\s*\((.*)\)\s*{',
                  r'\bfor\s*\((.*)\)\s*{',
                  r'\bwhile\s*\((.*)\)\s*[{;]',
                  r'\bswitch\s*\((.*)\)\s*{'):
    match = search(pattern, line)
    if match:
      fncall = match.group(1)    # look inside the parens for function calls
      break

  # except in if/for/while/switch, there should never be space
  # immediately inside parens (eg "f( 3, 4 )").  we make an exception
  # for nested parens ( (a+b) + c ).  likewise, there should never be
  # a space before a ( when it's a function argument.  i assume it's a
  # function argument when the char before the whitespace is legal in
  # a function name (alnum + _) and we're not starting a macro. also ignore
  # pointers and references to arrays and functions coz they're too tricky:
  # we use a very simple way to recognize these:
  # " (something)(maybe-something)" or
  # " (something)(maybe-something," or
  # " (something)[something]"
  # note that we assume the contents of [] to be short enough that
  # they'll never need to wrap.
  if (  # ignore control structures.
      not search(r'\b(if|for|while|switch|return|new|delete|catch|sizeof)\b',
                 fncall) and
      # ignore pointers/references to functions.
      not search(r' \([^)]+\)\([^)]*(\)|,$)', fncall) and
      # ignore pointers/references to arrays.
      not search(r' \([^)]+\)\[[^\]]+\]', fncall)):
    if search(r'\w\s*\(\s(?!\s*\\$)', fncall):      # a ( used for a fn call
      error(filename, linenum, 'whitespace/parens', 4,
            'extra space after ( in function call')
    elif search(r'\(\s+(?!(\s*\\)|\()', fncall):
      error(filename, linenum, 'whitespace/parens', 2,
            'extra space after (')
    if (search(r'\w\s+\(', fncall) and
        not search(r'#\s*define|typedef', fncall) and
        not search(r'\w\s+\((\w+::)*\*\w+\)\(', fncall)):
      error(filename, linenum, 'whitespace/parens', 4,
            'extra space before ( in function call')
    # if the ) is followed only by a newline or a { + newline, assume it's
    # part of a control statement (if/while/etc), and don't complain
    if search(r'[^)]\s+\)\s*[^{\s]', fncall):
      # if the closing parenthesis is preceded by only whitespaces,
      # try to give a more descriptive error message.
      if search(r'^\s+\)', fncall):
        error(filename, linenum, 'whitespace/parens', 2,
              'closing ) should be moved to the previous line')
      else:
        error(filename, linenum, 'whitespace/parens', 2,
              'extra space before )')


def isblankline(line):
  """returns true if the given line is blank.

  we consider a line to be blank if the line is empty or consists of
  only white spaces.

  args:
    line: a line of a string.

  returns:
    true, if the given line is blank.
  """
  return not line or line.isspace()


def checkforfunctionlengths(filename, clean_lines, linenum,
                            function_state, error):
  """reports for long function bodies.

  for an overview why this is done, see:
  http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#write_short_functions

  uses a simplistic algorithm assuming other style guidelines
  (especially spacing) are followed.
  only checks unindented functions, so class members are unchecked.
  trivial bodies are unchecked, so constructors with huge initializer lists
  may be missed.
  blank/comment lines are not counted so as to avoid encouraging the removal
  of vertical space and comments just to get through a lint check.
  nolint *on the last line of a function* disables this check.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    function_state: current function name and lines in body so far.
    error: the function to call with any errors found.
  """
  lines = clean_lines.lines
  line = lines[linenum]
  raw = clean_lines.raw_lines
  raw_line = raw[linenum]
  joined_line = ''

  starting_func = false
  regexp = r'(\w(\w|::|\*|\&|\s)*)\('  # decls * & space::name( ...
  match_result = match(regexp, line)
  if match_result:
    # if the name is all caps and underscores, figure it's a macro and
    # ignore it, unless it's test or test_f.
    function_name = match_result.group(1).split()[-1]
    if function_name == 'test' or function_name == 'test_f' or (
        not match(r'[a-z_]+$', function_name)):
      starting_func = true

  if starting_func:
    body_found = false
    for start_linenum in xrange(linenum, clean_lines.numlines()):
      start_line = lines[start_linenum]
      joined_line += ' ' + start_line.lstrip()
      if search(r'(;|})', start_line):  # declarations and trivial functions
        body_found = true
        break                              # ... ignore
      elif search(r'{', start_line):
        body_found = true
        function = search(r'((\w|:)*)\(', line).group(1)
        if match(r'test', function):    # handle test... macros
          parameter_regexp = search(r'(\(.*\))', joined_line)
          if parameter_regexp:             # ignore bad syntax
            function += parameter_regexp.group(1)
        else:
          function += '()'
        function_state.begin(function)
        break
    if not body_found:
      # no body for the function (or evidence of a non-function) was found.
      error(filename, linenum, 'readability/fn_size', 5,
            'lint failed to find start of function body.')
  elif match(r'^\}\s*$', line):  # function end
    function_state.check(error, filename, linenum)
    function_state.end()
  elif not match(r'^\s*$', line):
    function_state.count()  # count non-blank/non-comment lines.


_re_pattern_todo = re.compile(r'^//(\s*)todo(\(.+?\))?:?(\s|$)?')


def checkcomment(comment, filename, linenum, error):
  """checks for common mistakes in todo comments.

  args:
    comment: the text of the comment from the line in question.
    filename: the name of the current file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  match = _re_pattern_todo.match(comment)
  if match:
    # one whitespace is correct; zero whitespace is handled elsewhere.
    leading_whitespace = match.group(1)
    if len(leading_whitespace) > 1:
      error(filename, linenum, 'whitespace/todo', 2,
            'too many spaces before todo')

    username = match.group(2)
    if not username:
      error(filename, linenum, 'readability/todo', 2,
            'missing username in todo; it should look like '
            '"// todo(my_username): stuff."')

    middle_whitespace = match.group(3)
    # comparisons made explicit for correctness -- pylint: disable=g-explicit-bool-comparison
    if middle_whitespace != ' ' and middle_whitespace != '':
      error(filename, linenum, 'whitespace/todo', 2,
            'todo(my_username) should be followed by a space')

def checkaccess(filename, clean_lines, linenum, nesting_state, error):
  """checks for improper use of disallow* macros.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]  # get rid of comments and strings

  matched = match((r'\s*(disallow_copy_and_assign|'
                   r'disallow_evil_constructors|'
                   r'disallow_implicit_constructors)'), line)
  if not matched:
    return
  if nesting_state.stack and isinstance(nesting_state.stack[-1], _classinfo):
    if nesting_state.stack[-1].access != 'private':
      error(filename, linenum, 'readability/constructors', 3,
            '%s must be in the private: section' % matched.group(1))

  else:
    # found disallow* macro outside a class declaration, or perhaps it
    # was used inside a function when it should have been part of the
    # class declaration.  we could issue a warning here, but it
    # probably resulted in a compiler error already.
    pass


def findnextmatchinganglebracket(clean_lines, linenum, init_suffix):
  """find the corresponding > to close a template.

  args:
    clean_lines: a cleansedlines instance containing the file.
    linenum: current line number.
    init_suffix: remainder of the current line after the initial <.

  returns:
    true if a matching bracket exists.
  """
  line = init_suffix
  nesting_stack = ['<']
  while true:
    # find the next operator that can tell us whether < is used as an
    # opening bracket or as a less-than operator.  we only want to
    # warn on the latter case.
    #
    # we could also check all other operators and terminate the search
    # early, e.g. if we got something like this "a<b+c", the "<" is
    # most likely a less-than operator, but then we will get false
    # positives for default arguments and other template expressions.
    match = search(r'^[^<>(),;\[\]]*([<>(),;\[\]])(.*)$', line)
    if match:
      # found an operator, update nesting stack
      operator = match.group(1)
      line = match.group(2)

      if nesting_stack[-1] == '<':
        # expecting closing angle bracket
        if operator in ('<', '(', '['):
          nesting_stack.append(operator)
        elif operator == '>':
          nesting_stack.pop()
          if not nesting_stack:
            # found matching angle bracket
            return true
        elif operator == ',':
          # got a comma after a bracket, this is most likely a template
          # argument.  we have not seen a closing angle bracket yet, but
          # it's probably a few lines later if we look for it, so just
          # return early here.
          return true
        else:
          # got some other operator.
          return false

      else:
        # expecting closing parenthesis or closing bracket
        if operator in ('<', '(', '['):
          nesting_stack.append(operator)
        elif operator in (')', ']'):
          # we don't bother checking for matching () or [].  if we got
          # something like (] or [), it would have been a syntax error.
          nesting_stack.pop()

    else:
      # scan the next line
      linenum += 1
      if linenum >= len(clean_lines.elided):
        break
      line = clean_lines.elided[linenum]

  # exhausted all remaining lines and still no matching angle bracket.
  # most likely the input was incomplete, otherwise we should have
  # seen a semicolon and returned early.
  return true


def findpreviousmatchinganglebracket(clean_lines, linenum, init_prefix):
  """find the corresponding < that started a template.

  args:
    clean_lines: a cleansedlines instance containing the file.
    linenum: current line number.
    init_prefix: part of the current line before the initial >.

  returns:
    true if a matching bracket exists.
  """
  line = init_prefix
  nesting_stack = ['>']
  while true:
    # find the previous operator
    match = search(r'^(.*)([<>(),;\[\]])[^<>(),;\[\]]*$', line)
    if match:
      # found an operator, update nesting stack
      operator = match.group(2)
      line = match.group(1)

      if nesting_stack[-1] == '>':
        # expecting opening angle bracket
        if operator in ('>', ')', ']'):
          nesting_stack.append(operator)
        elif operator == '<':
          nesting_stack.pop()
          if not nesting_stack:
            # found matching angle bracket
            return true
        elif operator == ',':
          # got a comma before a bracket, this is most likely a
          # template argument.  the opening angle bracket is probably
          # there if we look for it, so just return early here.
          return true
        else:
          # got some other operator.
          return false

      else:
        # expecting opening parenthesis or opening bracket
        if operator in ('>', ')', ']'):
          nesting_stack.append(operator)
        elif operator in ('(', '['):
          nesting_stack.pop()

    else:
      # scan the previous line
      linenum -= 1
      if linenum < 0:
        break
      line = clean_lines.elided[linenum]

  # exhausted all earlier lines and still no matching angle bracket.
  return false


def checkspacing(filename, clean_lines, linenum, nesting_state, error):
  """checks for the correctness of various spacing issues in the code.

  things we check for: spaces around operators, spaces after
  if/for/while/switch, no spaces around parens in function calls, two
  spaces between code and comment, don't start a block with a blank
  line, don't end a function with a blank line, don't add a blank line
  after public/protected/private, don't have too many blank lines in a row.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: the function to call with any errors found.
  """

  # don't use "elided" lines here, otherwise we can't check commented lines.
  # don't want to use "raw" either, because we don't want to check inside c++11
  # raw strings,
  raw = clean_lines.lines_without_raw_strings
  line = raw[linenum]

  # before nixing comments, check if the line is blank for no good
  # reason.  this includes the first line after a block is opened, and
  # blank lines at the end of a function (ie, right before a line like '}'
  #
  # skip all the blank line checks if we are immediately inside a
  # namespace body.  in other words, don't issue blank line warnings
  # for this block:
  #   namespace {
  #
  #   }
  #
  # a warning about missing end of namespace comments will be issued instead.
  if isblankline(line) and not nesting_state.innamespacebody():
    elided = clean_lines.elided
    prev_line = elided[linenum - 1]
    prevbrace = prev_line.rfind('{')
    # todo(unknown): don't complain if line before blank line, and line after,
    #                both start with alnums and are indented the same amount.
    #                this ignores whitespace at the start of a namespace block
    #                because those are not usually indented.
    if prevbrace != -1 and prev_line[prevbrace:].find('}') == -1:
      # ok, we have a blank line at the start of a code block.  before we
      # complain, we check if it is an exception to the rule: the previous
      # non-empty line has the parameters of a function header that are indented
      # 4 spaces (because they did not fit in a 80 column line when placed on
      # the same line as the function name).  we also check for the case where
      # the previous line is indented 6 spaces, which may happen when the
      # initializers of a constructor do not fit into a 80 column line.
      exception = false
      if match(r' {6}\w', prev_line):  # initializer list?
        # we are looking for the opening column of initializer list, which
        # should be indented 4 spaces to cause 6 space indentation afterwards.
        search_position = linenum-2
        while (search_position >= 0
               and match(r' {6}\w', elided[search_position])):
          search_position -= 1
        exception = (search_position >= 0
                     and elided[search_position][:5] == '    :')
      else:
        # search for the function arguments or an initializer list.  we use a
        # simple heuristic here: if the line is indented 4 spaces; and we have a
        # closing paren, without the opening paren, followed by an opening brace
        # or colon (for initializer lists) we assume that it is the last line of
        # a function header.  if we have a colon indented 4 spaces, it is an
        # initializer list.
        exception = (match(r' {4}\w[^\(]*\)\s*(const\s*)?(\{\s*$|:)',
                           prev_line)
                     or match(r' {4}:', prev_line))

      if not exception:
        error(filename, linenum, 'whitespace/blank_line', 2,
              'redundant blank line at the start of a code block '
              'should be deleted.')
    # ignore blank lines at the end of a block in a long if-else
    # chain, like this:
    #   if (condition1) {
    #     // something followed by a blank line
    #
    #   } else if (condition2) {
    #     // something else
    #   }
    if linenum + 1 < clean_lines.numlines():
      next_line = raw[linenum + 1]
      if (next_line
          and match(r'\s*}', next_line)
          and next_line.find('} else ') == -1):
        error(filename, linenum, 'whitespace/blank_line', 3,
              'redundant blank line at the end of a code block '
              'should be deleted.')

    matched = match(r'\s*(public|protected|private):', prev_line)
    if matched:
      error(filename, linenum, 'whitespace/blank_line', 3,
            'do not leave a blank line after "%s:"' % matched.group(1))

  # next, we complain if there's a comment too near the text
  commentpos = line.find('//')
  if commentpos != -1:
    # check if the // may be in quotes.  if so, ignore it
    # comparisons made explicit for clarity -- pylint: disable=g-explicit-bool-comparison
    if (line.count('"', 0, commentpos) -
        line.count('\\"', 0, commentpos)) % 2 == 0:   # not in quotes
      # allow one space for new scopes, two spaces otherwise:
      if (not match(r'^\s*{ //', line) and
          ((commentpos >= 1 and
            line[commentpos-1] not in string.whitespace) or
           (commentpos >= 2 and
            line[commentpos-2] not in string.whitespace))):
        error(filename, linenum, 'whitespace/comments', 2,
              'at least two spaces is best between code and comments')
      # there should always be a space between the // and the comment
      commentend = commentpos + 2
      if commentend < len(line) and not line[commentend] == ' ':
        # but some lines are exceptions -- e.g. if they're big
        # comment delimiters like:
        # //----------------------------------------------------------
        # or are an empty c++ style doxygen comment, like:
        # ///
        # or c++ style doxygen comments placed after the variable:
        # ///<  header comment
        # //!<  header comment
        # or they begin with multiple slashes followed by a space:
        # //////// header comment
        match = (search(r'[=/-]{4,}\s*$', line[commentend:]) or
                 search(r'^/$', line[commentend:]) or
                 search(r'^!< ', line[commentend:]) or
                 search(r'^/< ', line[commentend:]) or
                 search(r'^/+ ', line[commentend:]))
        if not match:
          error(filename, linenum, 'whitespace/comments', 4,
                'should have a space between // and comment')
      checkcomment(line[commentpos:], filename, linenum, error)

  line = clean_lines.elided[linenum]  # get rid of comments and strings

  # don't try to do spacing checks for operator methods
  line = re.sub(r'operator(==|!=|<|<<|<=|>=|>>|>)\(', 'operator\(', line)

  # we allow no-spaces around = within an if: "if ( (a=foo()) == 0 )".
  # otherwise not.  note we only check for non-spaces on *both* sides;
  # sometimes people put non-spaces on one side when aligning ='s among
  # many lines (not that this is behavior that i approve of...)
  if search(r'[\w.]=[\w.]', line) and not search(r'\b(if|while) ', line):
    error(filename, linenum, 'whitespace/operators', 4,
          'missing spaces around =')

  # it's ok not to have spaces around binary operators like + - * /, but if
  # there's too little whitespace, we get concerned.  it's hard to tell,
  # though, so we punt on this one for now.  todo.

  # you should always have whitespace around binary operators.
  #
  # check <= and >= first to avoid false positives with < and >, then
  # check non-include lines for spacing around < and >.
  match = search(r'[^<>=!\s](==|!=|<=|>=)[^<>=!\s]', line)
  if match:
    error(filename, linenum, 'whitespace/operators', 3,
          'missing spaces around %s' % match.group(1))
  # we allow no-spaces around << when used like this: 10<<20, but
  # not otherwise (particularly, not when used as streams)
  # also ignore using ns::operator<<;
  match = search(r'(operator|\s)(?:l|ul|ull|l|ul|ull)?<<(\s)', line)
  if (match and
      not (match.group(1).isdigit() and match.group(2).isdigit()) and
      not (match.group(1) == 'operator' and match.group(2) == ';')):
    error(filename, linenum, 'whitespace/operators', 3,
          'missing spaces around <<')
  elif not match(r'#.*include', line):
    # avoid false positives on ->
    reduced_line = line.replace('->', '')

    # look for < that is not surrounded by spaces.  this is only
    # triggered if both sides are missing spaces, even though
    # technically should should flag if at least one side is missing a
    # space.  this is done to avoid some false positives with shifts.
    match = search(r'[^\s<]<([^\s=<].*)', reduced_line)
    if (match and
        not findnextmatchinganglebracket(clean_lines, linenum, match.group(1))):
      error(filename, linenum, 'whitespace/operators', 3,
            'missing spaces around <')

    # look for > that is not surrounded by spaces.  similar to the
    # above, we only trigger if both sides are missing spaces to avoid
    # false positives with shifts.
    match = search(r'^(.*[^\s>])>[^\s=>]', reduced_line)
    if (match and
        not findpreviousmatchinganglebracket(clean_lines, linenum,
                                             match.group(1))):
      error(filename, linenum, 'whitespace/operators', 3,
            'missing spaces around >')

  # we allow no-spaces around >> for almost anything.  this is because
  # c++11 allows ">>" to close nested templates, which accounts for
  # most cases when ">>" is not followed by a space.
  #
  # we still warn on ">>" followed by alpha character, because that is
  # likely due to ">>" being used for right shifts, e.g.:
  #   value >> alpha
  #
  # when ">>" is used to close templates, the alphanumeric letter that
  # follows would be part of an identifier, and there should still be
  # a space separating the template type and the identifier.
  #   type<type<type>> alpha
  match = search(r'>>[a-za-z_]', line)
  if match:
    error(filename, linenum, 'whitespace/operators', 3,
          'missing spaces around >>')

  # there shouldn't be space around unary operators
  match = search(r'(!\s|~\s|[\s]--[\s;]|[\s]\+\+[\s;])', line)
  if match:
    error(filename, linenum, 'whitespace/operators', 4,
          'extra space for operator %s' % match.group(1))

  # a pet peeve of mine: no spaces after an if, while, switch, or for
  match = search(r' (if\(|for\(|while\(|switch\()', line)
  if match:
    error(filename, linenum, 'whitespace/parens', 5,
          'missing space before ( in %s' % match.group(1))

  # for if/for/while/switch, the left and right parens should be
  # consistent about how many spaces are inside the parens, and
  # there should either be zero or one spaces inside the parens.
  # we don't want: "if ( foo)" or "if ( foo   )".
  # exception: "for ( ; foo; bar)" and "for (foo; bar; )" are allowed.
  match = search(r'\b(if|for|while|switch)\s*'
                 r'\(([ ]*)(.).*[^ ]+([ ]*)\)\s*{\s*$',
                 line)
  if match:
    if len(match.group(2)) != len(match.group(4)):
      if not (match.group(3) == ';' and
              len(match.group(2)) == 1 + len(match.group(4)) or
              not match.group(2) and search(r'\bfor\s*\(.*; \)', line)):
        error(filename, linenum, 'whitespace/parens', 5,
              'mismatching spaces inside () in %s' % match.group(1))
    if len(match.group(2)) not in [0, 1]:
      error(filename, linenum, 'whitespace/parens', 5,
            'should have zero or one spaces inside ( and ) in %s' %
            match.group(1))

  # you should always have a space after a comma (either as fn arg or operator)
  #
  # this does not apply when the non-space character following the
  # comma is another comma, since the only time when that happens is
  # for empty macro arguments.
  #
  # we run this check in two passes: first pass on elided lines to
  # verify that lines contain missing whitespaces, second pass on raw
  # lines to confirm that those missing whitespaces are not due to
  # elided comments.
  if search(r',[^,\s]', line) and search(r',[^,\s]', raw[linenum]):
    error(filename, linenum, 'whitespace/comma', 3,
          'missing space after ,')

  # you should always have a space after a semicolon
  # except for few corner cases
  # todo(unknown): clarify if 'if (1) { return 1;}' is requires one more
  # space after ;
  if search(r';[^\s};\\)/]', line):
    error(filename, linenum, 'whitespace/semicolon', 3,
          'missing space after ;')

  # next we will look for issues with function calls.
  checkspacingforfunctioncall(filename, line, linenum, error)

  # except after an opening paren, or after another opening brace (in case of
  # an initializer list, for instance), you should have spaces before your
  # braces. and since you should never have braces at the beginning of a line,
  # this is an easy test.
  match = match(r'^(.*[^ ({]){', line)
  if match:
    # try a bit harder to check for brace initialization.  this
    # happens in one of the following forms:
    #   constructor() : initializer_list_{} { ... }
    #   constructor{}.memberfunction()
    #   type variable{};
    #   functioncall(type{}, ...);
    #   lastargument(..., type{});
    #   log(info) << type{} << " ...";
    #   map_of_type[{...}] = ...;
    #
    # we check for the character following the closing brace, and
    # silence the warning if it's one of those listed above, i.e.
    # "{.;,)<]".
    #
    # to account for nested initializer list, we allow any number of
    # closing braces up to "{;,)<".  we can't simply silence the
    # warning on first sight of closing brace, because that would
    # cause false negatives for things that are not initializer lists.
    #   silence this:         but not this:
    #     outer{                if (...) {
    #       inner{...}            if (...){  // missing space before {
    #     };                    }
    #
    # there is a false negative with this approach if people inserted
    # spurious semicolons, e.g. "if (cond){};", but we will catch the
    # spurious semicolon with a separate check.
    (endline, endlinenum, endpos) = closeexpression(
        clean_lines, linenum, len(match.group(1)))
    trailing_text = ''
    if endpos > -1:
      trailing_text = endline[endpos:]
    for offset in xrange(endlinenum + 1,
                         min(endlinenum + 3, clean_lines.numlines() - 1)):
      trailing_text += clean_lines.elided[offset]
    if not match(r'^[\s}]*[{.;,)<\]]', trailing_text):
      error(filename, linenum, 'whitespace/braces', 5,
            'missing space before {')

  # make sure '} else {' has spaces.
  if search(r'}else', line):
    error(filename, linenum, 'whitespace/braces', 5,
          'missing space before else')

  # you shouldn't have spaces before your brackets, except maybe after
  # 'delete []' or 'new char * []'.
  if search(r'\w\s+\[', line) and not search(r'delete\s+\[', line):
    error(filename, linenum, 'whitespace/braces', 5,
          'extra space before [')

  # you shouldn't have a space before a semicolon at the end of the line.
  # there's a special case for "for" since the style guide allows space before
  # the semicolon there.
  if search(r':\s*;\s*$', line):
    error(filename, linenum, 'whitespace/semicolon', 5,
          'semicolon defining empty statement. use {} instead.')
  elif search(r'^\s*;\s*$', line):
    error(filename, linenum, 'whitespace/semicolon', 5,
          'line contains only semicolon. if this should be an empty statement, '
          'use {} instead.')
  elif (search(r'\s+;\s*$', line) and
        not search(r'\bfor\b', line)):
    error(filename, linenum, 'whitespace/semicolon', 5,
          'extra space before last semicolon. if this should be an empty '
          'statement, use {} instead.')

  # in range-based for, we wanted spaces before and after the colon, but
  # not around "::" tokens that might appear.
  if (search('for *\(.*[^:]:[^: ]', line) or
      search('for *\(.*[^: ]:[^:]', line)):
    error(filename, linenum, 'whitespace/forcolon', 2,
          'missing space around colon in range-based for loop')


def checksectionspacing(filename, clean_lines, class_info, linenum, error):
  """checks for additional blank line issues related to sections.

  currently the only thing checked here is blank line before protected/private.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    class_info: a _classinfo objects.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  # skip checks if the class is small, where small means 25 lines or less.
  # 25 lines seems like a good cutoff since that's the usual height of
  # terminals, and any class that can't fit in one screen can't really
  # be considered "small".
  #
  # also skip checks if we are on the first line.  this accounts for
  # classes that look like
  #   class foo { public: ... };
  #
  # if we didn't find the end of the class, last_line would be zero,
  # and the check will be skipped by the first condition.
  if (class_info.last_line - class_info.starting_linenum <= 24 or
      linenum <= class_info.starting_linenum):
    return

  matched = match(r'\s*(public|protected|private):', clean_lines.lines[linenum])
  if matched:
    # issue warning if the line before public/protected/private was
    # not a blank line, but don't do this if the previous line contains
    # "class" or "struct".  this can happen two ways:
    #  - we are at the beginning of the class.
    #  - we are forward-declaring an inner class that is semantically
    #    private, but needed to be public for implementation reasons.
    # also ignores cases where the previous line ends with a backslash as can be
    # common when defining classes in c macros.
    prev_line = clean_lines.lines[linenum - 1]
    if (not isblankline(prev_line) and
        not search(r'\b(class|struct)\b', prev_line) and
        not search(r'\\$', prev_line)):
      # try a bit harder to find the beginning of the class.  this is to
      # account for multi-line base-specifier lists, e.g.:
      #   class derived
      #       : public base {
      end_class_head = class_info.starting_linenum
      for i in range(class_info.starting_linenum, linenum):
        if search(r'\{\s*$', clean_lines.lines[i]):
          end_class_head = i
          break
      if end_class_head < linenum - 1:
        error(filename, linenum, 'whitespace/blank_line', 3,
              '"%s:" should be preceded by a blank line' % matched.group(1))


def getpreviousnonblankline(clean_lines, linenum):
  """return the most recent non-blank line and its line number.

  args:
    clean_lines: a cleansedlines instance containing the file contents.
    linenum: the number of the line to check.

  returns:
    a tuple with two elements.  the first element is the contents of the last
    non-blank line before the current line, or the empty string if this is the
    first non-blank line.  the second is the line number of that line, or -1
    if this is the first non-blank line.
  """

  prevlinenum = linenum - 1
  while prevlinenum >= 0:
    prevline = clean_lines.elided[prevlinenum]
    if not isblankline(prevline):     # if not a blank line...
      return (prevline, prevlinenum)
    prevlinenum -= 1
  return ('', -1)


def checkbraces(filename, clean_lines, linenum, error):
  """looks for misplaced braces (e.g. at the end of line).

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """

  line = clean_lines.elided[linenum]        # get rid of comments and strings

  if match(r'\s*{\s*$', line):
    # we allow an open brace to start a line in the case where someone is using
    # braces in a block to explicitly create a new scope, which is commonly used
    # to control the lifetime of stack-allocated variables.  braces are also
    # used for brace initializers inside function calls.  we don't detect this
    # perfectly: we just don't complain if the last non-whitespace character on
    # the previous non-blank line is ',', ';', ':', '(', '{', or '}', or if the
    # previous line starts a preprocessor block.
    prevline = getpreviousnonblankline(clean_lines, linenum)[0]
    if (not search(r'[,;:}{(]\s*$', prevline) and
        not match(r'\s*#', prevline)):
      error(filename, linenum, 'whitespace/braces', 4,
            '{ should almost always be at the end of the previous line')

  # an else clause should be on the same line as the preceding closing brace.
  if match(r'\s*else\s*', line):
    prevline = getpreviousnonblankline(clean_lines, linenum)[0]
    if match(r'\s*}\s*$', prevline):
      error(filename, linenum, 'whitespace/newline', 4,
            'an else should appear on the same line as the preceding }')

  # if braces come on one side of an else, they should be on both.
  # however, we have to worry about "else if" that spans multiple lines!
  if search(r'}\s*else[^{]*$', line) or match(r'[^}]*else\s*{', line):
    if search(r'}\s*else if([^{]*)$', line):       # could be multi-line if
      # find the ( after the if
      pos = line.find('else if')
      pos = line.find('(', pos)
      if pos > 0:
        (endline, _, endpos) = closeexpression(clean_lines, linenum, pos)
        if endline[endpos:].find('{') == -1:    # must be brace after if
          error(filename, linenum, 'readability/braces', 5,
                'if an else has a brace on one side, it should have it on both')
    else:            # common case: else not followed by a multi-line if
      error(filename, linenum, 'readability/braces', 5,
            'if an else has a brace on one side, it should have it on both')

  # likewise, an else should never have the else clause on the same line
  if search(r'\belse [^\s{]', line) and not search(r'\belse if\b', line):
    error(filename, linenum, 'whitespace/newline', 4,
          'else clause should never be on same line as else (use 2 lines)')

  # in the same way, a do/while should never be on one line
  if match(r'\s*do [^\s{]', line):
    error(filename, linenum, 'whitespace/newline', 4,
          'do/while clauses should not be on a single line')

  # block bodies should not be followed by a semicolon.  due to c++11
  # brace initialization, there are more places where semicolons are
  # required than not, so we use a whitelist approach to check these
  # rather than a blacklist.  these are the places where "};" should
  # be replaced by just "}":
  # 1. some flavor of block following closing parenthesis:
  #    for (;;) {};
  #    while (...) {};
  #    switch (...) {};
  #    function(...) {};
  #    if (...) {};
  #    if (...) else if (...) {};
  #
  # 2. else block:
  #    if (...) else {};
  #
  # 3. const member function:
  #    function(...) const {};
  #
  # 4. block following some statement:
  #    x = 42;
  #    {};
  #
  # 5. block at the beginning of a function:
  #    function(...) {
  #      {};
  #    }
  #
  #    note that naively checking for the preceding "{" will also match
  #    braces inside multi-dimensional arrays, but this is fine since
  #    that expression will not contain semicolons.
  #
  # 6. block following another block:
  #    while (true) {}
  #    {};
  #
  # 7. end of namespaces:
  #    namespace {};
  #
  #    these semicolons seems far more common than other kinds of
  #    redundant semicolons, possibly due to people converting classes
  #    to namespaces.  for now we do not warn for this case.
  #
  # try matching case 1 first.
  match = match(r'^(.*\)\s*)\{', line)
  if match:
    # matched closing parenthesis (case 1).  check the token before the
    # matching opening parenthesis, and don't warn if it looks like a
    # macro.  this avoids these false positives:
    #  - macro that defines a base class
    #  - multi-line macro that defines a base class
    #  - macro that defines the whole class-head
    #
    # but we still issue warnings for macros that we know are safe to
    # warn, specifically:
    #  - test, test_f, test_p, matcher, matcher_p
    #  - typed_test
    #  - interface_def
    #  - exclusive_locks_required, shared_locks_required, locks_excluded:
    #
    # we implement a whitelist of safe macros instead of a blacklist of
    # unsafe macros, even though the latter appears less frequently in
    # google code and would have been easier to implement.  this is because
    # the downside for getting the whitelist wrong means some extra
    # semicolons, while the downside for getting the blacklist wrong
    # would result in compile errors.
    #
    # in addition to macros, we also don't want to warn on compound
    # literals.
    closing_brace_pos = match.group(1).rfind(')')
    opening_parenthesis = reversecloseexpression(
        clean_lines, linenum, closing_brace_pos)
    if opening_parenthesis[2] > -1:
      line_prefix = opening_parenthesis[0][0:opening_parenthesis[2]]
      macro = search(r'\b([a-z_]+)\s*$', line_prefix)
      if ((macro and
           macro.group(1) not in (
               'test', 'test_f', 'matcher', 'matcher_p', 'typed_test',
               'exclusive_locks_required', 'shared_locks_required',
               'locks_excluded', 'interface_def')) or
          search(r'\s+=\s*$', line_prefix)):
        match = none
    # whitelist lambda function definition which also requires a ";" after
    # closing brace
    if match:
        if match(r'^.*\[.*\]\s*(.*\)\s*)\{', line):
            match = none

  else:
    # try matching cases 2-3.
    match = match(r'^(.*(?:else|\)\s*const)\s*)\{', line)
    if not match:
      # try matching cases 4-6.  these are always matched on separate lines.
      #
      # note that we can't simply concatenate the previous line to the
      # current line and do a single match, otherwise we may output
      # duplicate warnings for the blank line case:
      #   if (cond) {
      #     // blank line
      #   }
      prevline = getpreviousnonblankline(clean_lines, linenum)[0]
      if prevline and search(r'[;{}]\s*$', prevline):
        match = match(r'^(\s*)\{', line)

  # check matching closing brace
  if match:
    (endline, endlinenum, endpos) = closeexpression(
        clean_lines, linenum, len(match.group(1)))
    if endpos > -1 and match(r'^\s*;', endline[endpos:]):
      # current {} pair is eligible for semicolon check, and we have found
      # the redundant semicolon, output warning here.
      #
      # note: because we are scanning forward for opening braces, and
      # outputting warnings for the matching closing brace, if there are
      # nested blocks with trailing semicolons, we will get the error
      # messages in reversed order.
      error(filename, endlinenum, 'readability/braces', 4,
            "you don't need a ; after a }")


def checkemptyblockbody(filename, clean_lines, linenum, error):
  """look for empty loop/conditional body with only a single semicolon.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """

  # search for loop keywords at the beginning of the line.  because only
  # whitespaces are allowed before the keywords, this will also ignore most
  # do-while-loops, since those lines should start with closing brace.
  #
  # we also check "if" blocks here, since an empty conditional block
  # is likely an error.
  line = clean_lines.elided[linenum]
  matched = match(r'\s*(for|while|if)\s*\(', line)
  if matched:
    # find the end of the conditional expression
    (end_line, end_linenum, end_pos) = closeexpression(
        clean_lines, linenum, line.find('('))

    # output warning if what follows the condition expression is a semicolon.
    # no warning for all other cases, including whitespace or newline, since we
    # have a separate check for semicolons preceded by whitespace.
    if end_pos >= 0 and match(r';', end_line[end_pos:]):
      if matched.group(1) == 'if':
        error(filename, end_linenum, 'whitespace/empty_conditional_body', 5,
              'empty conditional bodies should use {}')
      else:
        error(filename, end_linenum, 'whitespace/empty_loop_body', 5,
              'empty loop bodies should use {} or continue')


def checkcheck(filename, clean_lines, linenum, error):
  """checks the use of check and expect macros.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """

  # decide the set of replacement macros that should be suggested
  lines = clean_lines.elided
  check_macro = none
  start_pos = -1
  for macro in _check_macros:
    i = lines[linenum].find(macro)
    if i >= 0:
      check_macro = macro

      # find opening parenthesis.  do a regular expression match here
      # to make sure that we are matching the expected check macro, as
      # opposed to some other macro that happens to contain the check
      # substring.
      matched = match(r'^(.*\b' + check_macro + r'\s*)\(', lines[linenum])
      if not matched:
        continue
      start_pos = len(matched.group(1))
      break
  if not check_macro or start_pos < 0:
    # don't waste time here if line doesn't contain 'check' or 'expect'
    return

  # find end of the boolean expression by matching parentheses
  (last_line, end_line, end_pos) = closeexpression(
      clean_lines, linenum, start_pos)
  if end_pos < 0:
    return
  if linenum == end_line:
    expression = lines[linenum][start_pos + 1:end_pos - 1]
  else:
    expression = lines[linenum][start_pos + 1:]
    for i in xrange(linenum + 1, end_line):
      expression += lines[i]
    expression += last_line[0:end_pos - 1]

  # parse expression so that we can take parentheses into account.
  # this avoids false positives for inputs like "check((a < 4) == b)",
  # which is not replaceable by check_le.
  lhs = ''
  rhs = ''
  operator = none
  while expression:
    matched = match(r'^\s*(<<|<<=|>>|>>=|->\*|->|&&|\|\||'
                    r'==|!=|>=|>|<=|<|\()(.*)$', expression)
    if matched:
      token = matched.group(1)
      if token == '(':
        # parenthesized operand
        expression = matched.group(2)
        (end, _) = findendofexpressioninline(expression, 0, 1, '(', ')')
        if end < 0:
          return  # unmatched parenthesis
        lhs += '(' + expression[0:end]
        expression = expression[end:]
      elif token in ('&&', '||'):
        # logical and/or operators.  this means the expression
        # contains more than one term, for example:
        #   check(42 < a && a < b);
        #
        # these are not replaceable with check_le, so bail out early.
        return
      elif token in ('<<', '<<=', '>>', '>>=', '->*', '->'):
        # non-relational operator
        lhs += token
        expression = matched.group(2)
      else:
        # relational operator
        operator = token
        rhs = matched.group(2)
        break
    else:
      # unparenthesized operand.  instead of appending to lhs one character
      # at a time, we do another regular expression match to consume several
      # characters at once if possible.  trivial benchmark shows that this
      # is more efficient when the operands are longer than a single
      # character, which is generally the case.
      matched = match(r'^([^-=!<>()&|]+)(.*)$', expression)
      if not matched:
        matched = match(r'^(\s*\s)(.*)$', expression)
        if not matched:
          break
      lhs += matched.group(1)
      expression = matched.group(2)

  # only apply checks if we got all parts of the boolean expression
  if not (lhs and operator and rhs):
    return

  # check that rhs do not contain logical operators.  we already know
  # that lhs is fine since the loop above parses out && and ||.
  if rhs.find('&&') > -1 or rhs.find('||') > -1:
    return

  # at least one of the operands must be a constant literal.  this is
  # to avoid suggesting replacements for unprintable things like
  # check(variable != iterator)
  #
  # the following pattern matches decimal, hex integers, strings, and
  # characters (in that order).
  lhs = lhs.strip()
  rhs = rhs.strip()
  match_constant = r'^([-+]?(\d+|0[xx][0-9a-fa-f]+)[lluu]{0,3}|".*"|\'.*\')$'
  if match(match_constant, lhs) or match(match_constant, rhs):
    # note: since we know both lhs and rhs, we can provide a more
    # descriptive error message like:
    #   consider using check_eq(x, 42) instead of check(x == 42)
    # instead of:
    #   consider using check_eq instead of check(a == b)
    #
    # we are still keeping the less descriptive message because if lhs
    # or rhs gets long, the error message might become unreadable.
    error(filename, linenum, 'readability/check', 2,
          'consider using %s instead of %s(a %s b)' % (
              _check_replacement[check_macro][operator],
              check_macro, operator))


def checkalttokens(filename, clean_lines, linenum, error):
  """check alternative keywords being used in boolean expressions.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]

  # avoid preprocessor lines
  if match(r'^\s*#', line):
    return

  # last ditch effort to avoid multi-line comments.  this will not help
  # if the comment started before the current line or ended after the
  # current line, but it catches most of the false positives.  at least,
  # it provides a way to workaround this warning for people who use
  # multi-line comments in preprocessor macros.
  #
  # todo(unknown): remove this once cpplint has better support for
  # multi-line comments.
  if line.find('/*') >= 0 or line.find('*/') >= 0:
    return

  for match in _alt_token_replacement_pattern.finditer(line):
    error(filename, linenum, 'readability/alt_tokens', 2,
          'use operator %s instead of %s' % (
              _alt_token_replacement[match.group(1)], match.group(1)))


def getlinewidth(line):
  """determines the width of the line in column positions.

  args:
    line: a string, which may be a unicode string.

  returns:
    the width of the line in column positions, accounting for unicode
    combining characters and wide characters.
  """
  if isinstance(line, unicode):
    width = 0
    for uc in unicodedata.normalize('nfc', line):
      if unicodedata.east_asian_width(uc) in ('w', 'f'):
        width += 2
      elif not unicodedata.combining(uc):
        width += 1
    return width
  else:
    return len(line)


def checkstyle(filename, clean_lines, linenum, file_extension, nesting_state,
               error):
  """checks rules from the 'c++ style rules' section of cppguide.html.

  most of these rules are hard to test (naming, comment style), but we
  do what we can.  in particular we check for 2-space indents, line lengths,
  tab usage, spaces inside code, etc.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    file_extension: the extension (without the dot) of the filename.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: the function to call with any errors found.
  """

  # don't use "elided" lines here, otherwise we can't check commented lines.
  # don't want to use "raw" either, because we don't want to check inside c++11
  # raw strings,
  raw_lines = clean_lines.lines_without_raw_strings
  line = raw_lines[linenum]

  if line.find('\t') != -1:
    error(filename, linenum, 'whitespace/tab', 1,
          'tab found; better to use spaces')

  # one or three blank spaces at the beginning of the line is weird; it's
  # hard to reconcile that with 2-space indents.
  # note: here are the conditions rob pike used for his tests.  mine aren't
  # as sophisticated, but it may be worth becoming so:  rlength==initial_spaces
  # if(rlength > 20) complain = 0;
  # if(match($0, " +(error|private|public|protected):")) complain = 0;
  # if(match(prev, "&& *$")) complain = 0;
  # if(match(prev, "\\|\\| *$")) complain = 0;
  # if(match(prev, "[\",=><] *$")) complain = 0;
  # if(match($0, " <<")) complain = 0;
  # if(match(prev, " +for \\(")) complain = 0;
  # if(prevodd && match(prevprev, " +for \\(")) complain = 0;
  initial_spaces = 0
  cleansed_line = clean_lines.elided[linenum]
  while initial_spaces < len(line) and line[initial_spaces] == ' ':
    initial_spaces += 1
  if line and line[-1].isspace():
    error(filename, linenum, 'whitespace/end_of_line', 4,
          'line ends in whitespace.  consider deleting these extra spaces.')
  # there are certain situations we allow one space, notably for section labels
  elif ((initial_spaces == 1 or initial_spaces == 3) and
        not match(r'\s*\w+\s*:\s*$', cleansed_line)):
    error(filename, linenum, 'whitespace/indent', 3,
          'weird number of spaces at line-start.  '
          'are you using a 2-space indent?')

  # check if the line is a header guard.
  is_header_guard = false
  if file_extension == 'h':
    cppvar = getheaderguardcppvariable(filename)
    if (line.startswith('#ifndef %s' % cppvar) or
        line.startswith('#define %s' % cppvar) or
        line.startswith('#endif  // %s' % cppvar)):
      is_header_guard = true
  # #include lines and header guards can be long, since there's no clean way to
  # split them.
  #
  # urls can be long too.  it's possible to split these, but it makes them
  # harder to cut&paste.
  #
  # the "$id:...$" comment may also get very long without it being the
  # developers fault.
  if (not line.startswith('#include') and not is_header_guard and
      not match(r'^\s*//.*http(s?)://\s*$', line) and
      not match(r'^// \$id:.*#[0-9]+ \$$', line)):
    line_width = getlinewidth(line)
    extended_length = int((_line_length * 1.25))
    if line_width > extended_length:
      error(filename, linenum, 'whitespace/line_length', 4,
            'lines should very rarely be longer than %i characters' %
            extended_length)
    elif line_width > _line_length:
      error(filename, linenum, 'whitespace/line_length', 2,
            'lines should be <= %i characters long' % _line_length)

  if (cleansed_line.count(';') > 1 and
      # for loops are allowed two ;'s (and may run over two lines).
      cleansed_line.find('for') == -1 and
      (getpreviousnonblankline(clean_lines, linenum)[0].find('for') == -1 or
       getpreviousnonblankline(clean_lines, linenum)[0].find(';') != -1) and
      # it's ok to have many commands in a switch case that fits in 1 line
      not ((cleansed_line.find('case ') != -1 or
            cleansed_line.find('default:') != -1) and
           cleansed_line.find('break;') != -1)):
    error(filename, linenum, 'whitespace/newline', 0,
          'more than one command on the same line')

  # some more style checks
  checkbraces(filename, clean_lines, linenum, error)
  checkemptyblockbody(filename, clean_lines, linenum, error)
  checkaccess(filename, clean_lines, linenum, nesting_state, error)
  checkspacing(filename, clean_lines, linenum, nesting_state, error)
  checkcheck(filename, clean_lines, linenum, error)
  checkalttokens(filename, clean_lines, linenum, error)
  classinfo = nesting_state.innermostclass()
  if classinfo:
    checksectionspacing(filename, clean_lines, classinfo, linenum, error)


_re_pattern_include_new_style = re.compile(r'#include +"[^/]+\.h"')
_re_pattern_include = re.compile(r'^\s*#\s*include\s*([<"])([^>"]*)[>"].*$')
# matches the first component of a filename delimited by -s and _s. that is:
#  _re_first_component.match('foo').group(0) == 'foo'
#  _re_first_component.match('foo.cc').group(0) == 'foo'
#  _re_first_component.match('foo-bar_baz.cc').group(0) == 'foo'
#  _re_first_component.match('foo_bar-baz.cc').group(0) == 'foo'
_re_first_component = re.compile(r'^[^-_.]+')


def _dropcommonsuffixes(filename):
  """drops common suffixes like _test.cc or -inl.h from filename.

  for example:
    >>> _dropcommonsuffixes('foo/foo-inl.h')
    'foo/foo'
    >>> _dropcommonsuffixes('foo/bar/foo.cc')
    'foo/bar/foo'
    >>> _dropcommonsuffixes('foo/foo_internal.h')
    'foo/foo'
    >>> _dropcommonsuffixes('foo/foo_unusualinternal.h')
    'foo/foo_unusualinternal'

  args:
    filename: the input filename.

  returns:
    the filename with the common suffix removed.
  """
  for suffix in ('test.cc', 'regtest.cc', 'unittest.cc',
                 'inl.h', 'impl.h', 'internal.h'):
    if (filename.endswith(suffix) and len(filename) > len(suffix) and
        filename[-len(suffix) - 1] in ('-', '_')):
      return filename[:-len(suffix) - 1]
  return os.path.splitext(filename)[0]


def _istestfilename(filename):
  """determines if the given filename has a suffix that identifies it as a test.

  args:
    filename: the input filename.

  returns:
    true if 'filename' looks like a test, false otherwise.
  """
  if (filename.endswith('_test.cc') or
      filename.endswith('_unittest.cc') or
      filename.endswith('_regtest.cc')):
    return true
  else:
    return false


def _classifyinclude(fileinfo, include, is_system):
  """figures out what kind of header 'include' is.

  args:
    fileinfo: the current file cpplint is running over. a fileinfo instance.
    include: the path to a #included file.
    is_system: true if the #include used <> rather than "".

  returns:
    one of the _xxx_header constants.

  for example:
    >>> _classifyinclude(fileinfo('foo/foo.cc'), 'stdio.h', true)
    _c_sys_header
    >>> _classifyinclude(fileinfo('foo/foo.cc'), 'string', true)
    _cpp_sys_header
    >>> _classifyinclude(fileinfo('foo/foo.cc'), 'foo/foo.h', false)
    _likely_my_header
    >>> _classifyinclude(fileinfo('foo/foo_unknown_extension.cc'),
    ...                  'bar/foo_other_ext.h', false)
    _possible_my_header
    >>> _classifyinclude(fileinfo('foo/foo.cc'), 'foo/bar.h', false)
    _other_header
  """
  # this is a list of all standard c++ header files, except
  # those already checked for above.
  is_cpp_h = include in _cpp_headers

  if is_system:
    if is_cpp_h:
      return _cpp_sys_header
    else:
      return _c_sys_header

  # if the target file and the include we're checking share a
  # basename when we drop common extensions, and the include
  # lives in . , then it's likely to be owned by the target file.
  target_dir, target_base = (
      os.path.split(_dropcommonsuffixes(fileinfo.repositoryname())))
  include_dir, include_base = os.path.split(_dropcommonsuffixes(include))
  if target_base == include_base and (
      include_dir == target_dir or
      include_dir == os.path.normpath(target_dir + '/../public')):
    return _likely_my_header

  # if the target and include share some initial basename
  # component, it's possible the target is implementing the
  # include, so it's allowed to be first, but we'll never
  # complain if it's not there.
  target_first_component = _re_first_component.match(target_base)
  include_first_component = _re_first_component.match(include_base)
  if (target_first_component and include_first_component and
      target_first_component.group(0) ==
      include_first_component.group(0)):
    return _possible_my_header

  return _other_header



def checkincludeline(filename, clean_lines, linenum, include_state, error):
  """check rules that are applicable to #include lines.

  strings on #include lines are not removed from elided line, to make
  certain tasks easier. however, to prevent false positives, checks
  applicable to #include lines in checklanguage must be put here.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    include_state: an _includestate instance in which the headers are inserted.
    error: the function to call with any errors found.
  """
  fileinfo = fileinfo(filename)

  line = clean_lines.lines[linenum]

  # "include" should use the new style "foo/bar.h" instead of just "bar.h"
  if _re_pattern_include_new_style.search(line):
    error(filename, linenum, 'build/include', 4,
          'include the directory when naming .h files')

  # we shouldn't include a file more than once. actually, there are a
  # handful of instances where doing so is okay, but in general it's
  # not.
  match = _re_pattern_include.search(line)
  if match:
    include = match.group(2)
    is_system = (match.group(1) == '<')
    if include in include_state:
      error(filename, linenum, 'build/include', 4,
            '"%s" already included at %s:%s' %
            (include, filename, include_state[include]))
    else:
      include_state[include] = linenum

      # we want to ensure that headers appear in the right order:
      # 1) for foo.cc, foo.h  (preferred location)
      # 2) c system files
      # 3) cpp system files
      # 4) for foo.cc, foo.h  (deprecated location)
      # 5) other google headers
      #
      # we classify each include statement as one of those 5 types
      # using a number of techniques. the include_state object keeps
      # track of the highest type seen, and complains if we see a
      # lower type after that.
      error_message = include_state.checknextincludeorder(
          _classifyinclude(fileinfo, include, is_system))
      if error_message:
        error(filename, linenum, 'build/include_order', 4,
              '%s. should be: %s.h, c system, c++ system, other.' %
              (error_message, fileinfo.basename()))
      canonical_include = include_state.canonicalizealphabeticalorder(include)
      if not include_state.isinalphabeticalorder(
          clean_lines, linenum, canonical_include):
        error(filename, linenum, 'build/include_alpha', 4,
              'include "%s" not in alphabetical order' % include)
      include_state.setlastheader(canonical_include)

  # look for any of the stream classes that are part of standard c++.
  match = _re_pattern_include.match(line)
  if match:
    include = match.group(2)
    if match(r'(f|ind|io|i|o|parse|pf|stdio|str|)?stream$', include):
      # many unit tests use cout, so we exempt them.
      if not _istestfilename(filename):
        error(filename, linenum, 'readability/streams', 3,
              'streams are highly discouraged.')


def _gettextinside(text, start_pattern):
  r"""retrieves all the text between matching open and close parentheses.

  given a string of lines and a regular expression string, retrieve all the text
  following the expression and between opening punctuation symbols like
  (, [, or {, and the matching close-punctuation symbol. this properly nested
  occurrences of the punctuations, so for the text like
    printf(a(), b(c()));
  a call to _gettextinside(text, r'printf\(') will return 'a(), b(c())'.
  start_pattern must match string having an open punctuation symbol at the end.

  args:
    text: the lines to extract text. its comments and strings must be elided.
           it can be single line and can span multiple lines.
    start_pattern: the regexp string indicating where to start extracting
                   the text.
  returns:
    the extracted text.
    none if either the opening string or ending punctuation could not be found.
  """
  # todo(sugawarayu): audit cpplint.py to see what places could be profitably
  # rewritten to use _gettextinside (and use inferior regexp matching today).

  # give opening punctuations to get the matching close-punctuations.
  matching_punctuation = {'(': ')', '{': '}', '[': ']'}
  closing_punctuation = set(matching_punctuation.itervalues())

  # find the position to start extracting text.
  match = re.search(start_pattern, text, re.m)
  if not match:  # start_pattern not found in text.
    return none
  start_position = match.end(0)

  assert start_position > 0, (
      'start_pattern must ends with an opening punctuation.')
  assert text[start_position - 1] in matching_punctuation, (
      'start_pattern must ends with an opening punctuation.')
  # stack of closing punctuations we expect to have in text after position.
  punctuation_stack = [matching_punctuation[text[start_position - 1]]]
  position = start_position
  while punctuation_stack and position < len(text):
    if text[position] == punctuation_stack[-1]:
      punctuation_stack.pop()
    elif text[position] in closing_punctuation:
      # a closing punctuation without matching opening punctuations.
      return none
    elif text[position] in matching_punctuation:
      punctuation_stack.append(matching_punctuation[text[position]])
    position += 1
  if punctuation_stack:
    # opening punctuations left without matching close-punctuations.
    return none
  # punctuations match.
  return text[start_position:position - 1]


# patterns for matching call-by-reference parameters.
#
# supports nested templates up to 2 levels deep using this messy pattern:
#   < (?: < (?: < [^<>]*
#               >
#           |   [^<>] )*
#         >
#     |   [^<>] )*
#   >
_re_pattern_ident = r'[_a-za-z]\w*'  # =~ [[:alpha:]][[:alnum:]]*
_re_pattern_type = (
    r'(?:const\s+)?(?:typename\s+|class\s+|struct\s+|union\s+|enum\s+)?'
    r'(?:\w|'
    r'\s*<(?:<(?:<[^<>]*>|[^<>])*>|[^<>])*>|'
    r'::)+')
# a call-by-reference parameter ends with '& identifier'.
_re_pattern_ref_param = re.compile(
    r'(' + _re_pattern_type + r'(?:\s*(?:\bconst\b|[*]))*\s*'
    r'&\s*' + _re_pattern_ident + r')\s*(?:=[^,()]+)?[,)]')
# a call-by-const-reference parameter either ends with 'const& identifier'
# or looks like 'const type& identifier' when 'type' is atomic.
_re_pattern_const_ref_param = (
    r'(?:.*\s*\bconst\s*&\s*' + _re_pattern_ident +
    r'|const\s+' + _re_pattern_type + r'\s*&\s*' + _re_pattern_ident + r')')


def checklanguage(filename, clean_lines, linenum, file_extension,
                  include_state, nesting_state, error):
  """checks rules from the 'c++ language rules' section of cppguide.html.

  some of these rules are hard to test (function overloading, using
  uint32 inappropriately), but we do the best we can.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    file_extension: the extension (without the dot) of the filename.
    include_state: an _includestate instance in which the headers are inserted.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: the function to call with any errors found.
  """
  # if the line is empty or consists of entirely a comment, no need to
  # check it.
  line = clean_lines.elided[linenum]
  if not line:
    return

  match = _re_pattern_include.search(line)
  if match:
    checkincludeline(filename, clean_lines, linenum, include_state, error)
    return

  # reset include state across preprocessor directives.  this is meant
  # to silence warnings for conditional includes.
  if match(r'^\s*#\s*(?:ifdef|elif|else|endif)\b', line):
    include_state.resetsection()

  # make windows paths like unix.
  fullname = os.path.abspath(filename).replace('\\', '/')

  # todo(unknown): figure out if they're using default arguments in fn proto.

  # check to see if they're using an conversion function cast.
  # i just try to capture the most common basic types, though there are more.
  # parameterless conversion functions, such as bool(), are allowed as they are
  # probably a member operator declaration or default constructor.
  match = search(
      r'(\bnew\s+)?\b'  # grab 'new' operator, if it's there
      r'(int|float|double|bool|char|int32|uint32|int64|uint64)'
      r'(\([^)].*)', line)
  if match:
    matched_new = match.group(1)
    matched_type = match.group(2)
    matched_funcptr = match.group(3)

    # gmock methods are defined using some variant of mock_methodx(name, type)
    # where type may be float(), int(string), etc.  without context they are
    # virtually indistinguishable from int(x) casts. likewise, gmock's
    # mockcallback takes a template parameter of the form return_type(arg_type),
    # which looks much like the cast we're trying to detect.
    #
    # std::function<> wrapper has a similar problem.
    #
    # return types for function pointers also look like casts if they
    # don't have an extra space.
    if (matched_new is none and  # if new operator, then this isn't a cast
        not (match(r'^\s*mock_(const_)?method\d+(_t)?\(', line) or
             search(r'\bmockcallback<.*>', line) or
             search(r'\bstd::function<.*>', line)) and
        not (matched_funcptr and
             match(r'\((?:[^() ]+::\s*\*\s*)?[^() ]+\)\s*\(',
                   matched_funcptr))):
      # try a bit harder to catch gmock lines: the only place where
      # something looks like an old-style cast is where we declare the
      # return type of the mocked method, and the only time when we
      # are missing context is if mock_method was split across
      # multiple lines.  the missing mock_method is usually one or two
      # lines back, so scan back one or two lines.
      #
      # it's not possible for gmock macros to appear in the first 2
      # lines, since the class head + section name takes up 2 lines.
      if (linenum < 2 or
          not (match(r'^\s*mock_(?:const_)?method\d+(?:_t)?\((?:\s+,)?\s*$',
                     clean_lines.elided[linenum - 1]) or
               match(r'^\s*mock_(?:const_)?method\d+(?:_t)?\(\s*$',
                     clean_lines.elided[linenum - 2]))):
        error(filename, linenum, 'readability/casting', 4,
              'using deprecated casting style.  '
              'use static_cast<%s>(...) instead' %
              matched_type)

  checkcstylecast(filename, linenum, line, clean_lines.raw_lines[linenum],
                  'static_cast',
                  r'\((int|float|double|bool|char|u?int(16|32|64))\)', error)

  # this doesn't catch all cases. consider (const char * const)"hello".
  #
  # (char *) "foo" should always be a const_cast (reinterpret_cast won't
  # compile).
  if checkcstylecast(filename, linenum, line, clean_lines.raw_lines[linenum],
                     'const_cast', r'\((char\s?\*+\s?)\)\s*"', error):
    pass
  else:
    # check pointer casts for other than string constants
    checkcstylecast(filename, linenum, line, clean_lines.raw_lines[linenum],
                    'reinterpret_cast', r'\((\w+\s?\*+\s?)\)', error)

  # in addition, we look for people taking the address of a cast.  this
  # is dangerous -- casts can assign to temporaries, so the pointer doesn't
  # point where you think.
  match = search(
      r'(?:&\(([^)]+)\)[\w(])|'
      r'(?:&(static|dynamic|down|reinterpret)_cast\b)', line)
  if match and match.group(1) != '*':
    error(filename, linenum, 'runtime/casting', 4,
          ('are you taking an address of a cast?  '
           'this is dangerous: could be a temp var.  '
           'take the address before doing the cast, rather than after'))

  # create an extended_line, which is the concatenation of the current and
  # next lines, for more effective checking of code that may span more than one
  # line.
  if linenum + 1 < clean_lines.numlines():
    extended_line = line + clean_lines.elided[linenum + 1]
  else:
    extended_line = line

  # check for people declaring static/global stl strings at the top level.
  # this is dangerous because the c++ language does not guarantee that
  # globals with constructors are initialized before the first access.
  match = match(
      r'((?:|static +)(?:|const +))string +([a-za-z0-9_:]+)\b(.*)',
      line)
  # make sure it's not a function.
  # function template specialization looks like: "string foo<type>(...".
  # class template definitions look like: "string foo<type>::method(...".
  #
  # also ignore things that look like operators.  these are matched separately
  # because operator names cross non-word boundaries.  if we change the pattern
  # above, we would decrease the accuracy of matching identifiers.
  if (match and
      not search(r'\boperator\w', line) and
      not match(r'\s*(<.*>)?(::[a-za-z0-9_]+)?\s*\(([^"]|$)', match.group(3))):
    error(filename, linenum, 'runtime/string', 4,
          'for a static/global string constant, use a c style string instead: '
          '"%schar %s[]".' %
          (match.group(1), match.group(2)))

  if search(r'\b([a-za-z0-9_]*_)\(\1\)', line):
    error(filename, linenum, 'runtime/init', 4,
          'you seem to be initializing a member variable with itself.')

  if file_extension == 'h':
    # todo(unknown): check that 1-arg constructors are explicit.
    #                how to tell it's a constructor?
    #                (handled in checkfornonstandardconstructs for now)
    # todo(unknown): check that classes have disallow_evil_constructors
    #                (level 1 error)
    pass

  # check if people are using the verboten c basic types.  the only exception
  # we regularly allow is "unsigned short port" for port.
  if search(r'\bshort port\b', line):
    if not search(r'\bunsigned short port\b', line):
      error(filename, linenum, 'runtime/int', 4,
            'use "unsigned short" for ports, not "short"')
  else:
    match = search(r'\b(short|long(?! +double)|long long)\b', line)
    if match:
      error(filename, linenum, 'runtime/int', 4,
            'use int16/int64/etc, rather than the c type %s' % match.group(1))

  # when snprintf is used, the second argument shouldn't be a literal.
  match = search(r'snprintf\s*\(([^,]*),\s*([0-9]*)\s*,', line)
  if match and match.group(2) != '0':
    # if 2nd arg is zero, snprintf is used to calculate size.
    error(filename, linenum, 'runtime/printf', 3,
          'if you can, use sizeof(%s) instead of %s as the 2nd arg '
          'to snprintf.' % (match.group(1), match.group(2)))

  # check if some verboten c functions are being used.
  if search(r'\bsprintf\b', line):
    error(filename, linenum, 'runtime/printf', 5,
          'never use sprintf.  use snprintf instead.')
  match = search(r'\b(strcpy|strcat)\b', line)
  if match:
    error(filename, linenum, 'runtime/printf', 4,
          'almost always, snprintf is better than %s' % match.group(1))

  # check if some verboten operator overloading is going on
  # todo(unknown): catch out-of-line unary operator&:
  #   class x {};
  #   int operator&(const x& x) { return 42; }  // unary operator&
  # the trick is it's hard to tell apart from binary operator&:
  #   class y { int operator&(const y& x) { return 23; } }; // binary operator&
  if search(r'\boperator\s*&\s*\(\s*\)', line):
    error(filename, linenum, 'runtime/operator', 4,
          'unary operator& is dangerous.  do not use it.')

  # check for suspicious usage of "if" like
  # } if (a == b) {
  if search(r'\}\s*if\s*\(', line):
    error(filename, linenum, 'readability/braces', 4,
          'did you mean "else if"? if not, start a new line for "if".')

  # check for potential format string bugs like printf(foo).
  # we constrain the pattern not to pick things like docidforprintf(foo).
  # not perfect but it can catch printf(foo.c_str()) and printf(foo->c_str())
  # todo(sugawarayu): catch the following case. need to change the calling
  # convention of the whole function to process multiple line to handle it.
  #   printf(
  #       boy_this_is_a_really_long_variable_that_cannot_fit_on_the_prev_line);
  printf_args = _gettextinside(line, r'(?i)\b(string)?printf\s*\(')
  if printf_args:
    match = match(r'([\w.\->()]+)$', printf_args)
    if match and match.group(1) != '__va_args__':
      function_name = re.search(r'\b((?:string)?printf)\s*\(',
                                line, re.i).group(1)
      error(filename, linenum, 'runtime/printf', 4,
            'potential format string bug. do %s("%%s", %s) instead.'
            % (function_name, match.group(1)))

  # check for potential memset bugs like memset(buf, sizeof(buf), 0).
  match = search(r'memset\s*\(([^,]*),\s*([^,]*),\s*0\s*\)', line)
  if match and not match(r"^''|-?[0-9]+|0x[0-9a-fa-f]$", match.group(2)):
    error(filename, linenum, 'runtime/memset', 4,
          'did you mean "memset(%s, 0, %s)"?'
          % (match.group(1), match.group(2)))

  if search(r'\busing namespace\b', line):
    error(filename, linenum, 'build/namespaces', 5,
          'do not use namespace using-directives.  '
          'use using-declarations instead.')

  # detect variable-length arrays.
  match = match(r'\s*(.+::)?(\w+) [a-z]\w*\[(.+)];', line)
  if (match and match.group(2) != 'return' and match.group(2) != 'delete' and
      match.group(3).find(']') == -1):
    # split the size using space and arithmetic operators as delimiters.
    # if any of the resulting tokens are not compile time constants then
    # report the error.
    tokens = re.split(r'\s|\+|\-|\*|\/|<<|>>]', match.group(3))
    is_const = true
    skip_next = false
    for tok in tokens:
      if skip_next:
        skip_next = false
        continue

      if search(r'sizeof\(.+\)', tok): continue
      if search(r'arraysize\(\w+\)', tok): continue

      tok = tok.lstrip('(')
      tok = tok.rstrip(')')
      if not tok: continue
      if match(r'\d+', tok): continue
      if match(r'0[xx][0-9a-fa-f]+', tok): continue
      if match(r'k[a-z0-9]\w*', tok): continue
      if match(r'(.+::)?k[a-z0-9]\w*', tok): continue
      if match(r'(.+::)?[a-z][a-z0-9_]*', tok): continue
      # a catch all for tricky sizeof cases, including 'sizeof expression',
      # 'sizeof(*type)', 'sizeof(const type)', 'sizeof(struct structname)'
      # requires skipping the next token because we split on ' ' and '*'.
      if tok.startswith('sizeof'):
        skip_next = true
        continue
      is_const = false
      break
    if not is_const:
      error(filename, linenum, 'runtime/arrays', 1,
            'do not use variable-length arrays.  use an appropriately named '
            "('k' followed by camelcase) compile-time constant for the size.")

  # if disallow_evil_constructors, disallow_copy_and_assign, or
  # disallow_implicit_constructors is present, then it should be the last thing
  # in the class declaration.
  match = match(
      (r'\s*'
       r'(disallow_(evil_constructors|copy_and_assign|implicit_constructors))'
       r'\(.*\);$'),
      line)
  if match and linenum + 1 < clean_lines.numlines():
    next_line = clean_lines.elided[linenum + 1]
    # we allow some, but not all, declarations of variables to be present
    # in the statement that defines the class.  the [\w\*,\s]* fragment of
    # the regular expression below allows users to declare instances of
    # the class or pointers to instances, but not less common types such
    # as function pointers or arrays.  it's a tradeoff between allowing
    # reasonable code and avoiding trying to parse more c++ using regexps.
    if not search(r'^\s*}[\w\*,\s]*;', next_line):
      error(filename, linenum, 'readability/constructors', 3,
            match.group(1) + ' should be the last thing in the class')

  # check for use of unnamed namespaces in header files.  registration
  # macros are typically ok, so we allow use of "namespace {" on lines
  # that end with backslashes.
  if (file_extension == 'h'
      and search(r'\bnamespace\s*{', line)
      and line[-1] != '\\'):
    error(filename, linenum, 'build/namespaces', 4,
          'do not use unnamed namespaces in header files.  see '
          'http://google-styleguide.googlecode.com/svn/trunk/cppguide.xml#namespaces'
          ' for more information.')

def checkfornonconstreference(filename, clean_lines, linenum,
                              nesting_state, error):
  """check for non-const references.

  separate from checklanguage since it scans backwards from current
  line, instead of scanning forward.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: the function to call with any errors found.
  """
  # do nothing if there is no '&' on current line.
  line = clean_lines.elided[linenum]
  if '&' not in line:
    return

  # long type names may be broken across multiple lines, usually in one
  # of these forms:
  #   longtype
  #       ::longtypecontinued &identifier
  #   longtype::
  #       longtypecontinued &identifier
  #   longtype<
  #       ...>::longtypecontinued &identifier
  #
  # if we detected a type split across two lines, join the previous
  # line to current line so that we can match const references
  # accordingly.
  #
  # note that this only scans back one line, since scanning back
  # arbitrary number of lines would be expensive.  if you have a type
  # that spans more than 2 lines, please use a typedef.
  if linenum > 1:
    previous = none
    if match(r'\s*::(?:[\w<>]|::)+\s*&\s*\s', line):
      # previous_line\n + ::current_line
      previous = search(r'\b((?:const\s*)?(?:[\w<>]|::)+[\w<>])\s*$',
                        clean_lines.elided[linenum - 1])
    elif match(r'\s*[a-za-z_]([\w<>]|::)+\s*&\s*\s', line):
      # previous_line::\n + current_line
      previous = search(r'\b((?:const\s*)?(?:[\w<>]|::)+::)\s*$',
                        clean_lines.elided[linenum - 1])
    if previous:
      line = previous.group(1) + line.lstrip()
    else:
      # check for templated parameter that is split across multiple lines
      endpos = line.rfind('>')
      if endpos > -1:
        (_, startline, startpos) = reversecloseexpression(
            clean_lines, linenum, endpos)
        if startpos > -1 and startline < linenum:
          # found the matching < on an earlier line, collect all
          # pieces up to current line.
          line = ''
          for i in xrange(startline, linenum + 1):
            line += clean_lines.elided[i].strip()

  # check for non-const references in function parameters.  a single '&' may
  # found in the following places:
  #   inside expression: binary & for bitwise and
  #   inside expression: unary & for taking the address of something
  #   inside declarators: reference parameter
  # we will exclude the first two cases by checking that we are not inside a
  # function body, including one that was just introduced by a trailing '{'.
  # todo(unknwon): doesn't account for preprocessor directives.
  # todo(unknown): doesn't account for 'catch(exception& e)' [rare].
  check_params = false
  if not nesting_state.stack:
    check_params = true  # top level
  elif (isinstance(nesting_state.stack[-1], _classinfo) or
        isinstance(nesting_state.stack[-1], _namespaceinfo)):
    check_params = true  # within class or namespace
  elif match(r'.*{\s*$', line):
    if (len(nesting_state.stack) == 1 or
        isinstance(nesting_state.stack[-2], _classinfo) or
        isinstance(nesting_state.stack[-2], _namespaceinfo)):
      check_params = true  # just opened global/class/namespace block
  # we allow non-const references in a few standard places, like functions
  # called "swap()" or iostream operators like "<<" or ">>".  do not check
  # those function parameters.
  #
  # we also accept & in static_assert, which looks like a function but
  # it's actually a declaration expression.
  whitelisted_functions = (r'(?:[ss]wap(?:<\w:+>)?|'
                           r'operator\s*[<>][<>]|'
                           r'static_assert|compile_assert'
                           r')\s*\(')
  if search(whitelisted_functions, line):
    check_params = false
  elif not search(r'\s+\([^)]*$', line):
    # don't see a whitelisted function on this line.  actually we
    # didn't see any function name on this line, so this is likely a
    # multi-line parameter list.  try a bit harder to catch this case.
    for i in xrange(2):
      if (linenum > i and
          search(whitelisted_functions, clean_lines.elided[linenum - i - 1])):
        check_params = false
        break

  if check_params:
    decls = replaceall(r'{[^}]*}', ' ', line)  # exclude function body
    for parameter in re.findall(_re_pattern_ref_param, decls):
      if not match(_re_pattern_const_ref_param, parameter):
        error(filename, linenum, 'runtime/references', 2,
              'is this a non-const reference? '
              'if so, make const or use a pointer: ' +
              replaceall(' *<', '<', parameter))


def checkcstylecast(filename, linenum, line, raw_line, cast_type, pattern,
                    error):
  """checks for a c-style cast by looking for the pattern.

  args:
    filename: the name of the current file.
    linenum: the number of the line to check.
    line: the line of code to check.
    raw_line: the raw line of code to check, with comments.
    cast_type: the string for the c++ cast to recommend.  this is either
      reinterpret_cast, static_cast, or const_cast, depending.
    pattern: the regular expression used to find c-style casts.
    error: the function to call with any errors found.

  returns:
    true if an error was emitted.
    false otherwise.
  """
  match = search(pattern, line)
  if not match:
    return false

  # exclude lines with sizeof, since sizeof looks like a cast.
  sizeof_match = match(r'.*sizeof\s*$', line[0:match.start(1) - 1])
  if sizeof_match:
    return false

  # operator++(int) and operator--(int)
  if (line[0:match.start(1) - 1].endswith(' operator++') or
      line[0:match.start(1) - 1].endswith(' operator--')):
    return false

  # a single unnamed argument for a function tends to look like old
  # style cast.  if we see those, don't issue warnings for deprecated
  # casts, instead issue warnings for unnamed arguments where
  # appropriate.
  #
  # these are things that we want warnings for, since the style guide
  # explicitly require all parameters to be named:
  #   function(int);
  #   function(int) {
  #   constmember(int) const;
  #   constmember(int) const {
  #   exceptionmember(int) throw (...);
  #   exceptionmember(int) throw (...) {
  #   purevirtual(int) = 0;
  #
  # these are functions of some sort, where the compiler would be fine
  # if they had named parameters, but people often omit those
  # identifiers to reduce clutter:
  #   (functionpointer)(int);
  #   (functionpointer)(int) = value;
  #   function((function_pointer_arg)(int))
  #   <templateargument(int)>;
  #   <(functionpointertemplateargument)(int)>;
  remainder = line[match.end(0):]
  if match(r'^\s*(?:;|const\b|throw\b|=|>|\{|\))', remainder):
    # looks like an unnamed parameter.

    # don't warn on any kind of template arguments.
    if match(r'^\s*>', remainder):
      return false

    # don't warn on assignments to function pointers, but keep warnings for
    # unnamed parameters to pure virtual functions.  note that this pattern
    # will also pass on assignments of "0" to function pointers, but the
    # preferred values for those would be "nullptr" or "null".
    matched_zero = match(r'^\s=\s*(\s+)\s*;', remainder)
    if matched_zero and matched_zero.group(1) != '0':
      return false

    # don't warn on function pointer declarations.  for this we need
    # to check what came before the "(type)" string.
    if match(r'.*\)\s*$', line[0:match.start(0)]):
      return false

    # don't warn if the parameter is named with block comments, e.g.:
    #  function(int /*unused_param*/);
    if '/*' in raw_line:
      return false

    # passed all filters, issue warning here.
    error(filename, linenum, 'readability/function', 3,
          'all parameters should be named in a function')
    return true

  # at this point, all that should be left is actual casts.
  error(filename, linenum, 'readability/casting', 4,
        'using c-style cast.  use %s<%s>(...) instead' %
        (cast_type, match.group(1)))

  return true


_headers_containing_templates = (
    ('<deque>', ('deque',)),
    ('<functional>', ('unary_function', 'binary_function',
                      'plus', 'minus', 'multiplies', 'divides', 'modulus',
                      'negate',
                      'equal_to', 'not_equal_to', 'greater', 'less',
                      'greater_equal', 'less_equal',
                      'logical_and', 'logical_or', 'logical_not',
                      'unary_negate', 'not1', 'binary_negate', 'not2',
                      'bind1st', 'bind2nd',
                      'pointer_to_unary_function',
                      'pointer_to_binary_function',
                      'ptr_fun',
                      'mem_fun_t', 'mem_fun', 'mem_fun1_t', 'mem_fun1_ref_t',
                      'mem_fun_ref_t',
                      'const_mem_fun_t', 'const_mem_fun1_t',
                      'const_mem_fun_ref_t', 'const_mem_fun1_ref_t',
                      'mem_fun_ref',
                     )),
    ('<limits>', ('numeric_limits',)),
    ('<list>', ('list',)),
    ('<map>', ('map', 'multimap',)),
    ('<memory>', ('allocator',)),
    ('<queue>', ('queue', 'priority_queue',)),
    ('<set>', ('set', 'multiset',)),
    ('<stack>', ('stack',)),
    ('<string>', ('char_traits', 'basic_string',)),
    ('<utility>', ('pair',)),
    ('<vector>', ('vector',)),

    # gcc extensions.
    # note: std::hash is their hash, ::hash is our hash
    ('<hash_map>', ('hash_map', 'hash_multimap',)),
    ('<hash_set>', ('hash_set', 'hash_multiset',)),
    ('<slist>', ('slist',)),
    )

_re_pattern_string = re.compile(r'\bstring\b')

_re_pattern_algorithm_header = []
for _template in ('copy', 'max', 'min', 'min_element', 'sort', 'swap',
                  'transform'):
  # match max<type>(..., ...), max(..., ...), but not foo->max, foo.max or
  # type::max().
  _re_pattern_algorithm_header.append(
      (re.compile(r'[^>.]\b' + _template + r'(<.*?>)?\([^\)]'),
       _template,
       '<algorithm>'))

_re_pattern_templates = []
for _header, _templates in _headers_containing_templates:
  for _template in _templates:
    _re_pattern_templates.append(
        (re.compile(r'(\<|\b)' + _template + r'\s*\<'),
         _template + '<>',
         _header))


def filesbelongtosamemodule(filename_cc, filename_h):
  """check if these two filenames belong to the same module.

  the concept of a 'module' here is a as follows:
  foo.h, foo-inl.h, foo.cc, foo_test.cc and foo_unittest.cc belong to the
  same 'module' if they are in the same directory.
  some/path/public/xyzzy and some/path/internal/xyzzy are also considered
  to belong to the same module here.

  if the filename_cc contains a longer path than the filename_h, for example,
  '/absolute/path/to/base/sysinfo.cc', and this file would include
  'base/sysinfo.h', this function also produces the prefix needed to open the
  header. this is used by the caller of this function to more robustly open the
  header file. we don't have access to the real include paths in this context,
  so we need this guesswork here.

  known bugs: tools/base/bar.cc and base/bar.h belong to the same module
  according to this implementation. because of this, this function gives
  some false positives. this should be sufficiently rare in practice.

  args:
    filename_cc: is the path for the .cc file
    filename_h: is the path for the header path

  returns:
    tuple with a bool and a string:
    bool: true if filename_cc and filename_h belong to the same module.
    string: the additional prefix needed to open the header file.
  """

  if not filename_cc.endswith('.cc'):
    return (false, '')
  filename_cc = filename_cc[:-len('.cc')]
  if filename_cc.endswith('_unittest'):
    filename_cc = filename_cc[:-len('_unittest')]
  elif filename_cc.endswith('_test'):
    filename_cc = filename_cc[:-len('_test')]
  filename_cc = filename_cc.replace('/public/', '/')
  filename_cc = filename_cc.replace('/internal/', '/')

  if not filename_h.endswith('.h'):
    return (false, '')
  filename_h = filename_h[:-len('.h')]
  if filename_h.endswith('-inl'):
    filename_h = filename_h[:-len('-inl')]
  filename_h = filename_h.replace('/public/', '/')
  filename_h = filename_h.replace('/internal/', '/')

  files_belong_to_same_module = filename_cc.endswith(filename_h)
  common_path = ''
  if files_belong_to_same_module:
    common_path = filename_cc[:-len(filename_h)]
  return files_belong_to_same_module, common_path


def updateincludestate(filename, include_state, io=codecs):
  """fill up the include_state with new includes found from the file.

  args:
    filename: the name of the header to read.
    include_state: an _includestate instance in which the headers are inserted.
    io: the io factory to use to read the file. provided for testability.

  returns:
    true if a header was successfully added. false otherwise.
  """
  headerfile = none
  try:
    headerfile = io.open(filename, 'r', 'utf8', 'replace')
  except ioerror:
    return false
  linenum = 0
  for line in headerfile:
    linenum += 1
    clean_line = cleansecomments(line)
    match = _re_pattern_include.search(clean_line)
    if match:
      include = match.group(2)
      # the value formatting is cute, but not really used right now.
      # what matters here is that the key is in include_state.
      include_state.setdefault(include, '%s:%d' % (filename, linenum))
  return true


def checkforincludewhatyouuse(filename, clean_lines, include_state, error,
                              io=codecs):
  """reports for missing stl includes.

  this function will output warnings to make sure you are including the headers
  necessary for the stl containers and functions that you use. we only give one
  reason to include a header. for example, if you use both equal_to<> and
  less<> in a .h file, only one (the latter in the file) of these will be
  reported as a reason to include the <functional>.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    include_state: an _includestate instance.
    error: the function to call with any errors found.
    io: the io factory to use to read the header file. provided for unittest
        injection.
  """
  required = {}  # a map of header name to linenumber and the template entity.
                 # example of required: { '<functional>': (1219, 'less<>') }

  for linenum in xrange(clean_lines.numlines()):
    line = clean_lines.elided[linenum]
    if not line or line[0] == '#':
      continue

    # string is special -- it is a non-templatized type in stl.
    matched = _re_pattern_string.search(line)
    if matched:
      # don't warn about strings in non-stl namespaces:
      # (we check only the first match per line; good enough.)
      prefix = line[:matched.start()]
      if prefix.endswith('std::') or not prefix.endswith('::'):
        required['<string>'] = (linenum, 'string')

    for pattern, template, header in _re_pattern_algorithm_header:
      if pattern.search(line):
        required[header] = (linenum, template)

    # the following function is just a speed up, no semantics are changed.
    if not '<' in line:  # reduces the cpu time usage by skipping lines.
      continue

    for pattern, template, header in _re_pattern_templates:
      if pattern.search(line):
        required[header] = (linenum, template)

  # the policy is that if you #include something in foo.h you don't need to
  # include it again in foo.cc. here, we will look at possible includes.
  # let's copy the include_state so it is only messed up within this function.
  include_state = include_state.copy()

  # did we find the header for this file (if any) and successfully load it?
  header_found = false

  # use the absolute path so that matching works properly.
  abs_filename = fileinfo(filename).fullname()

  # for emacs's flymake.
  # if cpplint is invoked from emacs's flymake, a temporary file is generated
  # by flymake and that file name might end with '_flymake.cc'. in that case,
  # restore original file name here so that the corresponding header file can be
  # found.
  # e.g. if the file name is 'foo_flymake.cc', we should search for 'foo.h'
  # instead of 'foo_flymake.h'
  abs_filename = re.sub(r'_flymake\.cc$', '.cc', abs_filename)

  # include_state is modified during iteration, so we iterate over a copy of
  # the keys.
  header_keys = include_state.keys()
  for header in header_keys:
    (same_module, common_path) = filesbelongtosamemodule(abs_filename, header)
    fullpath = common_path + header
    if same_module and updateincludestate(fullpath, include_state, io):
      header_found = true

  # if we can't find the header file for a .cc, assume it's because we don't
  # know where to look. in that case we'll give up as we're not sure they
  # didn't include it in the .h file.
  # todo(unknown): do a better job of finding .h files so we are confident that
  # not having the .h file means there isn't one.
  if filename.endswith('.cc') and not header_found:
    return

  # all the lines have been processed, report the errors found.
  for required_header_unstripped in required:
    template = required[required_header_unstripped][1]
    if required_header_unstripped.strip('<>"') not in include_state:
      error(filename, required[required_header_unstripped][0],
            'build/include_what_you_use', 4,
            'add #include ' + required_header_unstripped + ' for ' + template)


_re_pattern_explicit_makepair = re.compile(r'\bmake_pair\s*<')


def checkmakepairusesdeduction(filename, clean_lines, linenum, error):
  """check that make_pair's template arguments are deduced.

  g++ 4.6 in c++0x mode fails badly if make_pair's template arguments are
  specified explicitly, and such use isn't intended in any case.

  args:
    filename: the name of the current file.
    clean_lines: a cleansedlines instance containing the file.
    linenum: the number of the line to check.
    error: the function to call with any errors found.
  """
  line = clean_lines.elided[linenum]
  match = _re_pattern_explicit_makepair.search(line)
  if match:
    error(filename, linenum, 'build/explicit_make_pair',
          4,  # 4 = high confidence
          'for c++11-compatibility, omit template arguments from make_pair'
          ' or use pair directly or if appropriate, construct a pair directly')


def processline(filename, file_extension, clean_lines, line,
                include_state, function_state, nesting_state, error,
                extra_check_functions=[]):
  """processes a single line in the file.

  args:
    filename: filename of the file that is being processed.
    file_extension: the extension (dot not included) of the file.
    clean_lines: an array of strings, each representing a line of the file,
                 with comments stripped.
    line: number of line being processed.
    include_state: an _includestate instance in which the headers are inserted.
    function_state: a _functionstate instance which counts function lines, etc.
    nesting_state: a _nestingstate instance which maintains information about
                   the current stack of nested blocks being parsed.
    error: a callable to which errors are reported, which takes 4 arguments:
           filename, line number, error level, and message
    extra_check_functions: an array of additional check functions that will be
                           run on each source line. each function takes 4
                           arguments: filename, clean_lines, line, error
  """
  raw_lines = clean_lines.raw_lines
  parsenolintsuppressions(filename, raw_lines[line], line, error)
  nesting_state.update(filename, clean_lines, line, error)
  if nesting_state.stack and nesting_state.stack[-1].inline_asm != _no_asm:
    return
  checkforfunctionlengths(filename, clean_lines, line, function_state, error)
  checkformultilinecommentsandstrings(filename, clean_lines, line, error)
  checkstyle(filename, clean_lines, line, file_extension, nesting_state, error)
  checklanguage(filename, clean_lines, line, file_extension, include_state,
                nesting_state, error)
  checkfornonconstreference(filename, clean_lines, line, nesting_state, error)
  checkfornonstandardconstructs(filename, clean_lines, line,
                                nesting_state, error)
  checkvlogarguments(filename, clean_lines, line, error)
  checkposixthreading(filename, clean_lines, line, error)
  checkinvalidincrement(filename, clean_lines, line, error)
  checkmakepairusesdeduction(filename, clean_lines, line, error)
  for check_fn in extra_check_functions:
    check_fn(filename, clean_lines, line, error)

def processfiledata(filename, file_extension, lines, error,
                    extra_check_functions=[]):
  """performs lint checks and reports any errors to the given error function.

  args:
    filename: filename of the file that is being processed.
    file_extension: the extension (dot not included) of the file.
    lines: an array of strings, each representing a line of the file, with the
           last element being empty if the file is terminated with a newline.
    error: a callable to which errors are reported, which takes 4 arguments:
           filename, line number, error level, and message
    extra_check_functions: an array of additional check functions that will be
                           run on each source line. each function takes 4
                           arguments: filename, clean_lines, line, error
  """
  lines = (['// marker so line numbers and indices both start at 1'] + lines +
           ['// marker so line numbers end in a known way'])

  include_state = _includestate()
  function_state = _functionstate()
  nesting_state = _nestingstate()

  resetnolintsuppressions()

  checkforcopyright(filename, lines, error)

  if file_extension == 'h':
    checkforheaderguard(filename, lines, error)

  removemultilinecomments(filename, lines, error)
  clean_lines = cleansedlines(lines)
  for line in xrange(clean_lines.numlines()):
    processline(filename, file_extension, clean_lines, line,
                include_state, function_state, nesting_state, error,
                extra_check_functions)
  nesting_state.checkcompletedblocks(filename, error)

  checkforincludewhatyouuse(filename, clean_lines, include_state, error)

  # we check here rather than inside processline so that we see raw
  # lines rather than "cleaned" lines.
  checkforbadcharacters(filename, lines, error)

  checkfornewlineateof(filename, lines, error)

def processfile(filename, vlevel, extra_check_functions=[]):
  """does google-lint on a single file.

  args:
    filename: the name of the file to parse.

    vlevel: the level of errors to report.  every error of confidence
    >= verbose_level will be reported.  0 is a good default.

    extra_check_functions: an array of additional check functions that will be
                           run on each source line. each function takes 4
                           arguments: filename, clean_lines, line, error
  """

  _setverboselevel(vlevel)

  try:
    # support the unix convention of using "-" for stdin.  note that
    # we are not opening the file with universal newline support
    # (which codecs doesn't support anyway), so the resulting lines do
    # contain trailing '\r' characters if we are reading a file that
    # has crlf endings.
    # if after the split a trailing '\r' is present, it is removed
    # below. if it is not expected to be present (i.e. os.linesep !=
    # '\r\n' as in windows), a warning is issued below if this file
    # is processed.

    if filename == '-':
      lines = codecs.streamreaderwriter(sys.stdin,
                                        codecs.getreader('utf8'),
                                        codecs.getwriter('utf8'),
                                        'replace').read().split('\n')
    else:
      lines = codecs.open(filename, 'r', 'utf8', 'replace').read().split('\n')

    carriage_return_found = false
    # remove trailing '\r'.
    for linenum in range(len(lines)):
      if lines[linenum].endswith('\r'):
        lines[linenum] = lines[linenum].rstrip('\r')
        carriage_return_found = true

  except ioerror:
    sys.stderr.write(
        "skipping input '%s': can't open for reading\n" % filename)
    return

  # note, if no dot is found, this will give the entire filename as the ext.
  file_extension = filename[filename.rfind('.') + 1:]

  # when reading from stdin, the extension is unknown, so no cpplint tests
  # should rely on the extension.
  if filename != '-' and file_extension not in _valid_extensions:
    sys.stderr.write('ignoring %s; not a valid file name '
                     '(%s)\n' % (filename, ', '.join(_valid_extensions)))
  else:
    processfiledata(filename, file_extension, lines, error,
                    extra_check_functions)
    if carriage_return_found and os.linesep != '\r\n':
      # use 0 for linenum since outputting only one error for potentially
      # several lines.
      error(filename, 0, 'whitespace/newline', 1,
            'one or more unexpected \\r (^m) found;'
            'better to use only a \\n')

  sys.stderr.write('done processing %s\n' % filename)


def printusage(message):
  """prints a brief usage string and exits, optionally with an error message.

  args:
    message: the optional error message.
  """
  sys.stderr.write(_usage)
  if message:
    sys.exit('\nfatal error: ' + message)
  else:
    sys.exit(1)


def printcategories():
  """prints a list of all the error-categories used by error messages.

  these are the categories used to filter messages via --filter.
  """
  sys.stderr.write(''.join('  %s\n' % cat for cat in _error_categories))
  sys.exit(0)


def parsearguments(args):
  """parses the command line arguments.

  this may set the output format and verbosity level as side-effects.

  args:
    args: the command line arguments:

  returns:
    the list of filenames to lint.
  """
  try:
    (opts, filenames) = getopt.getopt(args, '', ['help', 'output=', 'verbose=',
                                                 'counting=',
                                                 'filter=',
                                                 'root=',
                                                 'linelength=',
                                                 'extensions='])
  except getopt.getopterror:
    printusage('invalid arguments.')

  verbosity = _verboselevel()
  output_format = _outputformat()
  filters = ''
  counting_style = ''

  for (opt, val) in opts:
    if opt == '--help':
      printusage(none)
    elif opt == '--output':
      if val not in ('emacs', 'vs7', 'eclipse'):
        printusage('the only allowed output formats are emacs, vs7 and eclipse.')
      output_format = val
    elif opt == '--verbose':
      verbosity = int(val)
    elif opt == '--filter':
      filters = val
      if not filters:
        printcategories()
    elif opt == '--counting':
      if val not in ('total', 'toplevel', 'detailed'):
        printusage('valid counting options are total, toplevel, and detailed')
      counting_style = val
    elif opt == '--root':
      global _root
      _root = val
    elif opt == '--linelength':
      global _line_length
      try:
          _line_length = int(val)
      except valueerror:
          printusage('line length must be digits.')
    elif opt == '--extensions':
      global _valid_extensions
      try:
          _valid_extensions = set(val.split(','))
      except valueerror:
          printusage('extensions must be comma separated list.')

  if not filenames:
    printusage('no files were specified.')

  _setoutputformat(output_format)
  _setverboselevel(verbosity)
  _setfilters(filters)
  _setcountingstyle(counting_style)

  return filenames


def main():
  filenames = parsearguments(sys.argv[1:])

  # change stderr to write with replacement characters so we don't die
  # if we try to print something containing non-ascii characters.
  sys.stderr = codecs.streamreaderwriter(sys.stderr,
                                         codecs.getreader('utf8'),
                                         codecs.getwriter('utf8'),
                                         'replace')

  _cpplint_state.reseterrorcounts()
  for filename in filenames:
    processfile(filename, _cpplint_state.verbose_level)
  _cpplint_state.printerrorcounts()

  sys.exit(_cpplint_state.error_count > 0)


if __name__ == '__main__':
  main()
