# protocol buffers - google's data interchange format
# copyright 2008 google inc.  all rights reserved.
# http://code.google.com/p/protobuf/
#
# redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * neither the name of google inc. nor the names of its
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

"""contains routines for printing protocol messages in text format."""

__author__ = 'kenton@google.com (kenton varda)'

import cstringio
import re

from collections import deque
from google.protobuf.internal import type_checkers
from google.protobuf import descriptor

__all__ = [ 'messagetostring', 'printmessage', 'printfield',
            'printfieldvalue', 'merge' ]


_integer_checkers = (type_checkers.uint32valuechecker(),
                     type_checkers.int32valuechecker(),
                     type_checkers.uint64valuechecker(),
                     type_checkers.int64valuechecker())
_float_infinity = re.compile('-?inf(?:inity)?f?', re.ignorecase)
_float_nan = re.compile('nanf?', re.ignorecase)


class parseerror(exception):
  """thrown in case of ascii parsing error."""


def messagetostring(message, as_utf8=false, as_one_line=false):
  out = cstringio.stringio()
  printmessage(message, out, as_utf8=as_utf8, as_one_line=as_one_line)
  result = out.getvalue()
  out.close()
  if as_one_line:
    return result.rstrip()
  return result


def printmessage(message, out, indent=0, as_utf8=false, as_one_line=false):
  for field, value in message.listfields():
    if field.label == descriptor.fielddescriptor.label_repeated:
      for element in value:
        printfield(field, element, out, indent, as_utf8, as_one_line)
    else:
      printfield(field, value, out, indent, as_utf8, as_one_line)


def printfield(field, value, out, indent=0, as_utf8=false, as_one_line=false):
  """print a single field name/value pair.  for repeated fields, the value
  should be a single element."""

  out.write(' ' * indent);
  if field.is_extension:
    out.write('[')
    if (field.containing_type.getoptions().message_set_wire_format and
        field.type == descriptor.fielddescriptor.type_message and
        field.message_type == field.extension_scope and
        field.label == descriptor.fielddescriptor.label_optional):
      out.write(field.message_type.full_name)
    else:
      out.write(field.full_name)
    out.write(']')
  elif field.type == descriptor.fielddescriptor.type_group:
    # for groups, use the capitalized name.
    out.write(field.message_type.name)
  else:
    out.write(field.name)

  if field.cpp_type != descriptor.fielddescriptor.cpptype_message:
    # the colon is optional in this case, but our cross-language golden files
    # don't include it.
    out.write(': ')

  printfieldvalue(field, value, out, indent, as_utf8, as_one_line)
  if as_one_line:
    out.write(' ')
  else:
    out.write('\n')


def printfieldvalue(field, value, out, indent=0,
                    as_utf8=false, as_one_line=false):
  """print a single field value (not including name).  for repeated fields,
  the value should be a single element."""

  if field.cpp_type == descriptor.fielddescriptor.cpptype_message:
    if as_one_line:
      out.write(' { ')
      printmessage(value, out, indent, as_utf8, as_one_line)
      out.write('}')
    else:
      out.write(' {\n')
      printmessage(value, out, indent + 2, as_utf8, as_one_line)
      out.write(' ' * indent + '}')
  elif field.cpp_type == descriptor.fielddescriptor.cpptype_enum:
    enum_value = field.enum_type.values_by_number.get(value, none)
    if enum_value is not none:
      out.write(enum_value.name)
    else:
      out.write(str(value))
  elif field.cpp_type == descriptor.fielddescriptor.cpptype_string:
    out.write('\"')
    if type(value) is unicode:
      out.write(_cescape(value.encode('utf-8'), as_utf8))
    else:
      out.write(_cescape(value, as_utf8))
    out.write('\"')
  elif field.cpp_type == descriptor.fielddescriptor.cpptype_bool:
    if value:
      out.write("true")
    else:
      out.write("false")
  else:
    out.write(str(value))


def merge(text, message):
  """merges an ascii representation of a protocol message into a message.

  args:
    text: message ascii representation.
    message: a protocol buffer message to merge into.

  raises:
    parseerror: on ascii parsing problems.
  """
  tokenizer = _tokenizer(text)
  while not tokenizer.atend():
    _mergefield(tokenizer, message)


def _mergefield(tokenizer, message):
  """merges a single protocol message field into a message.

  args:
    tokenizer: a tokenizer to parse the field name and values.
    message: a protocol message to record the data.

  raises:
    parseerror: in case of ascii parsing problems.
  """
  message_descriptor = message.descriptor
  if tokenizer.tryconsume('['):
    name = [tokenizer.consumeidentifier()]
    while tokenizer.tryconsume('.'):
      name.append(tokenizer.consumeidentifier())
    name = '.'.join(name)

    if not message_descriptor.is_extendable:
      raise tokenizer.parseerrorprevioustoken(
          'message type "%s" does not have extensions.' %
          message_descriptor.full_name)
    field = message.extensions._findextensionbyname(name)
    if not field:
      raise tokenizer.parseerrorprevioustoken(
          'extension "%s" not registered.' % name)
    elif message_descriptor != field.containing_type:
      raise tokenizer.parseerrorprevioustoken(
          'extension "%s" does not extend message type "%s".' % (
              name, message_descriptor.full_name))
    tokenizer.consume(']')
  else:
    name = tokenizer.consumeidentifier()
    field = message_descriptor.fields_by_name.get(name, none)

    # group names are expected to be capitalized as they appear in the
    # .proto file, which actually matches their type names, not their field
    # names.
    if not field:
      field = message_descriptor.fields_by_name.get(name.lower(), none)
      if field and field.type != descriptor.fielddescriptor.type_group:
        field = none

    if (field and field.type == descriptor.fielddescriptor.type_group and
        field.message_type.name != name):
      field = none

    if not field:
      raise tokenizer.parseerrorprevioustoken(
          'message type "%s" has no field named "%s".' % (
              message_descriptor.full_name, name))

  if field.cpp_type == descriptor.fielddescriptor.cpptype_message:
    tokenizer.tryconsume(':')

    if tokenizer.tryconsume('<'):
      end_token = '>'
    else:
      tokenizer.consume('{')
      end_token = '}'

    if field.label == descriptor.fielddescriptor.label_repeated:
      if field.is_extension:
        sub_message = message.extensions[field].add()
      else:
        sub_message = getattr(message, field.name).add()
    else:
      if field.is_extension:
        sub_message = message.extensions[field]
      else:
        sub_message = getattr(message, field.name)
      sub_message.setinparent()

    while not tokenizer.tryconsume(end_token):
      if tokenizer.atend():
        raise tokenizer.parseerrorprevioustoken('expected "%s".' % (end_token))
      _mergefield(tokenizer, sub_message)
  else:
    _mergescalarfield(tokenizer, message, field)


def _mergescalarfield(tokenizer, message, field):
  """merges a single protocol message scalar field into a message.

  args:
    tokenizer: a tokenizer to parse the field value.
    message: a protocol message to record the data.
    field: the descriptor of the field to be merged.

  raises:
    parseerror: in case of ascii parsing problems.
    runtimeerror: on runtime errors.
  """
  tokenizer.consume(':')
  value = none

  if field.type in (descriptor.fielddescriptor.type_int32,
                    descriptor.fielddescriptor.type_sint32,
                    descriptor.fielddescriptor.type_sfixed32):
    value = tokenizer.consumeint32()
  elif field.type in (descriptor.fielddescriptor.type_int64,
                      descriptor.fielddescriptor.type_sint64,
                      descriptor.fielddescriptor.type_sfixed64):
    value = tokenizer.consumeint64()
  elif field.type in (descriptor.fielddescriptor.type_uint32,
                      descriptor.fielddescriptor.type_fixed32):
    value = tokenizer.consumeuint32()
  elif field.type in (descriptor.fielddescriptor.type_uint64,
                      descriptor.fielddescriptor.type_fixed64):
    value = tokenizer.consumeuint64()
  elif field.type in (descriptor.fielddescriptor.type_float,
                      descriptor.fielddescriptor.type_double):
    value = tokenizer.consumefloat()
  elif field.type == descriptor.fielddescriptor.type_bool:
    value = tokenizer.consumebool()
  elif field.type == descriptor.fielddescriptor.type_string:
    value = tokenizer.consumestring()
  elif field.type == descriptor.fielddescriptor.type_bytes:
    value = tokenizer.consumebytestring()
  elif field.type == descriptor.fielddescriptor.type_enum:
    value = tokenizer.consumeenum(field)
  else:
    raise runtimeerror('unknown field type %d' % field.type)

  if field.label == descriptor.fielddescriptor.label_repeated:
    if field.is_extension:
      message.extensions[field].append(value)
    else:
      getattr(message, field.name).append(value)
  else:
    if field.is_extension:
      message.extensions[field] = value
    else:
      setattr(message, field.name, value)


class _tokenizer(object):
  """protocol buffer ascii representation tokenizer.

  this class handles the lower level string parsing by splitting it into
  meaningful tokens.

  it was directly ported from the java protocol buffer api.
  """

  _whitespace = re.compile('(\\s|(#.*$))+', re.multiline)
  _token = re.compile(
      '[a-za-z_][0-9a-za-z_+-]*|'           # an identifier
      '[0-9+-][0-9a-za-z_.+-]*|'            # a number
      '\"([^\"\n\\\\]|\\\\.)*(\"|\\\\?$)|'  # a double-quoted string
      '\'([^\'\n\\\\]|\\\\.)*(\'|\\\\?$)')  # a single-quoted string
  _identifier = re.compile('\w+')

  def __init__(self, text_message):
    self._text_message = text_message

    self._position = 0
    self._line = -1
    self._column = 0
    self._token_start = none
    self.token = ''
    self._lines = deque(text_message.split('\n'))
    self._current_line = ''
    self._previous_line = 0
    self._previous_column = 0
    self._skipwhitespace()
    self.nexttoken()

  def atend(self):
    """checks the end of the text was reached.

    returns:
      true iff the end was reached.
    """
    return self.token == ''

  def _popline(self):
    while len(self._current_line) <= self._column:
      if not self._lines:
        self._current_line = ''
        return
      self._line += 1
      self._column = 0
      self._current_line = self._lines.popleft()

  def _skipwhitespace(self):
    while true:
      self._popline()
      match = self._whitespace.match(self._current_line, self._column)
      if not match:
        break
      length = len(match.group(0))
      self._column += length

  def tryconsume(self, token):
    """tries to consume a given piece of text.

    args:
      token: text to consume.

    returns:
      true iff the text was consumed.
    """
    if self.token == token:
      self.nexttoken()
      return true
    return false

  def consume(self, token):
    """consumes a piece of text.

    args:
      token: text to consume.

    raises:
      parseerror: if the text couldn't be consumed.
    """
    if not self.tryconsume(token):
      raise self._parseerror('expected "%s".' % token)

  def consumeidentifier(self):
    """consumes protocol message field identifier.

    returns:
      identifier string.

    raises:
      parseerror: if an identifier couldn't be consumed.
    """
    result = self.token
    if not self._identifier.match(result):
      raise self._parseerror('expected identifier.')
    self.nexttoken()
    return result

  def consumeint32(self):
    """consumes a signed 32bit integer number.

    returns:
      the integer parsed.

    raises:
      parseerror: if a signed 32bit integer couldn't be consumed.
    """
    try:
      result = parseinteger(self.token, is_signed=true, is_long=false)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumeuint32(self):
    """consumes an unsigned 32bit integer number.

    returns:
      the integer parsed.

    raises:
      parseerror: if an unsigned 32bit integer couldn't be consumed.
    """
    try:
      result = parseinteger(self.token, is_signed=false, is_long=false)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumeint64(self):
    """consumes a signed 64bit integer number.

    returns:
      the integer parsed.

    raises:
      parseerror: if a signed 64bit integer couldn't be consumed.
    """
    try:
      result = parseinteger(self.token, is_signed=true, is_long=true)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumeuint64(self):
    """consumes an unsigned 64bit integer number.

    returns:
      the integer parsed.

    raises:
      parseerror: if an unsigned 64bit integer couldn't be consumed.
    """
    try:
      result = parseinteger(self.token, is_signed=false, is_long=true)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumefloat(self):
    """consumes an floating point number.

    returns:
      the number parsed.

    raises:
      parseerror: if a floating point number couldn't be consumed.
    """
    try:
      result = parsefloat(self.token)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumebool(self):
    """consumes a boolean value.

    returns:
      the bool parsed.

    raises:
      parseerror: if a boolean value couldn't be consumed.
    """
    try:
      result = parsebool(self.token)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumestring(self):
    """consumes a string value.

    returns:
      the string parsed.

    raises:
      parseerror: if a string value couldn't be consumed.
    """
    bytes = self.consumebytestring()
    try:
      return unicode(bytes, 'utf-8')
    except unicodedecodeerror, e:
      raise self._stringparseerror(e)

  def consumebytestring(self):
    """consumes a byte array value.

    returns:
      the array parsed (as a string).

    raises:
      parseerror: if a byte array value couldn't be consumed.
    """
    list = [self._consumesinglebytestring()]
    while len(self.token) > 0 and self.token[0] in ('\'', '"'):
      list.append(self._consumesinglebytestring())
    return "".join(list)

  def _consumesinglebytestring(self):
    """consume one token of a string literal.

    string literals (whether bytes or text) can come in multiple adjacent
    tokens which are automatically concatenated, like in c or python.  this
    method only consumes one token.
    """
    text = self.token
    if len(text) < 1 or text[0] not in ('\'', '"'):
      raise self._parseerror('expected string.')

    if len(text) < 2 or text[-1] != text[0]:
      raise self._parseerror('string missing ending quote.')

    try:
      result = _cunescape(text[1:-1])
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def consumeenum(self, field):
    try:
      result = parseenum(field, self.token)
    except valueerror, e:
      raise self._parseerror(str(e))
    self.nexttoken()
    return result

  def parseerrorprevioustoken(self, message):
    """creates and *returns* a parseerror for the previously read token.

    args:
      message: a message to set for the exception.

    returns:
      a parseerror instance.
    """
    return parseerror('%d:%d : %s' % (
        self._previous_line + 1, self._previous_column + 1, message))

  def _parseerror(self, message):
    """creates and *returns* a parseerror for the current token."""
    return parseerror('%d:%d : %s' % (
        self._line + 1, self._column + 1, message))

  def _stringparseerror(self, e):
    return self._parseerror('couldn\'t parse string: ' + str(e))

  def nexttoken(self):
    """reads the next meaningful token."""
    self._previous_line = self._line
    self._previous_column = self._column

    self._column += len(self.token)
    self._skipwhitespace()

    if not self._lines and len(self._current_line) <= self._column:
      self.token = ''
      return

    match = self._token.match(self._current_line, self._column)
    if match:
      token = match.group(0)
      self.token = token
    else:
      self.token = self._current_line[self._column]


# text.encode('string_escape') does not seem to satisfy our needs as it
# encodes unprintable characters using two-digit hex escapes whereas our
# c++ unescaping function allows hex escapes to be any length.  so,
# "\0011".encode('string_escape') ends up being "\\x011", which will be
# decoded in c++ as a single-character string with char code 0x11.
def _cescape(text, as_utf8):
  def escape(c):
    o = ord(c)
    if o == 10: return r"\n"   # optional escape
    if o == 13: return r"\r"   # optional escape
    if o ==  9: return r"\t"   # optional escape
    if o == 39: return r"\'"   # optional escape

    if o == 34: return r'\"'   # necessary escape
    if o == 92: return r"\\"   # necessary escape

    # necessary escapes
    if not as_utf8 and (o >= 127 or o < 32): return "\\%03o" % o
    return c
  return "".join([escape(c) for c in text])


_cunescape_hex = re.compile(r'(\\+)x([0-9a-fa-f])(?![0-9a-fa-f])')


def _cunescape(text):
  def replacehex(m):
    # only replace the match if the number of leading back slashes is odd. i.e.
    # the slash itself is not escaped.
    if len(m.group(1)) & 1:
      return m.group(1) + 'x0' + m.group(2)
    return m.group(0)

  # this is required because the 'string_escape' encoding doesn't
  # allow single-digit hex escapes (like '\xf').
  result = _cunescape_hex.sub(replacehex, text)
  return result.decode('string_escape')


def parseinteger(text, is_signed=false, is_long=false):
  """parses an integer.

  args:
    text: the text to parse.
    is_signed: true if a signed integer must be parsed.
    is_long: true if a long integer must be parsed.

  returns:
    the integer value.

  raises:
    valueerror: thrown iff the text is not a valid integer.
  """
  # do the actual parsing. exception handling is propagated to caller.
  try:
    result = int(text, 0)
  except valueerror:
    raise valueerror('couldn\'t parse integer: %s' % text)

  # check if the integer is sane. exceptions handled by callers.
  checker = _integer_checkers[2 * int(is_long) + int(is_signed)]
  checker.checkvalue(result)
  return result


def parsefloat(text):
  """parse a floating point number.

  args:
    text: text to parse.

  returns:
    the number parsed.

  raises:
    valueerror: if a floating point number couldn't be parsed.
  """
  try:
    # assume python compatible syntax.
    return float(text)
  except valueerror:
    # check alternative spellings.
    if _float_infinity.match(text):
      if text[0] == '-':
        return float('-inf')
      else:
        return float('inf')
    elif _float_nan.match(text):
      return float('nan')
    else:
      # assume '1.0f' format
      try:
        return float(text.rstrip('f'))
      except valueerror:
        raise valueerror('couldn\'t parse float: %s' % text)


def parsebool(text):
  """parse a boolean value.

  args:
    text: text to parse.

  returns:
    boolean values parsed

  raises:
    valueerror: if text is not a valid boolean.
  """
  if text in ('true', 't', '1'):
    return true
  elif text in ('false', 'f', '0'):
    return false
  else:
    raise valueerror('expected "true" or "false".')


def parseenum(field, value):
  """parse an enum value.

  the value can be specified by a number (the enum value), or by
  a string literal (the enum name).

  args:
    field: enum field descriptor.
    value: string value.

  returns:
    enum value number.

  raises:
    valueerror: if the enum value could not be parsed.
  """
  enum_descriptor = field.enum_type
  try:
    number = int(value, 0)
  except valueerror:
    # identifier.
    enum_value = enum_descriptor.values_by_name.get(value, none)
    if enum_value is none:
      raise valueerror(
          'enum type "%s" has no value named %s.' % (
              enum_descriptor.full_name, value))
  else:
    # numeric value.
    enum_value = enum_descriptor.values_by_number.get(number, none)
    if enum_value is none:
      raise valueerror(
          'enum type "%s" has no value with number %d.' % (
              enum_descriptor.full_name, number))
  return enum_value.number
