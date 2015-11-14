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

"""code for decoding protocol buffer primitives.

this code is very similar to encoder.py -- read the docs for that module first.

a "decoder" is a function with the signature:
  decode(buffer, pos, end, message, field_dict)
the arguments are:
  buffer:     the string containing the encoded message.
  pos:        the current position in the string.
  end:        the position in the string where the current message ends.  may be
              less than len(buffer) if we're reading a sub-message.
  message:    the message object into which we're parsing.
  field_dict: message._fields (avoids a hashtable lookup).
the decoder reads the field and stores it into field_dict, returning the new
buffer position.  a decoder for a repeated field may proactively decode all of
the elements of that field, if they appear consecutively.

note that decoders may throw any of the following:
  indexerror:  indicates a truncated message.
  struct.error:  unpacking of a fixed-width field failed.
  message.decodeerror:  other errors.

decoders are expected to raise an exception if they are called with pos > end.
this allows callers to be lax about bounds checking:  it's fineto read past
"end" as long as you are sure that someone else will notice and throw an
exception later on.

something up the call stack is expected to catch indexerror and struct.error
and convert them to message.decodeerror.

decoders are constructed using decoder constructors with the signature:
  makedecoder(field_number, is_repeated, is_packed, key, new_default)
the arguments are:
  field_number:  the field number of the field we want to decode.
  is_repeated:   is the field a repeated field? (bool)
  is_packed:     is the field a packed field? (bool)
  key:           the key to use when looking up the field within field_dict.
                 (this is actually the fielddescriptor but nothing in this
                 file should depend on that.)
  new_default:   a function which takes a message object as a parameter and
                 returns a new instance of the default value for this field.
                 (this is called for repeated fields and sub-messages, when an
                 instance does not already exist.)

as with encoders, we define a decoder constructor for every type of field.
then, for every field of every message class we construct an actual decoder.
that decoder goes into a dict indexed by tag, so when we decode a message
we repeatedly read a tag, look up the corresponding decoder, and invoke it.
"""

__author__ = 'kenton@google.com (kenton varda)'

import struct
from google.protobuf.internal import encoder
from google.protobuf.internal import wire_format
from google.protobuf import message


# this will overflow and thus become ieee-754 "infinity".  we would use
# "float('inf')" but it doesn't work on windows pre-python-2.6.
_pos_inf = 1e10000
_neg_inf = -_pos_inf
_nan = _pos_inf * 0


# this is not for optimization, but rather to avoid conflicts with local
# variables named "message".
_decodeerror = message.decodeerror


def _varintdecoder(mask):
  """return an encoder for a basic varint value (does not include tag).

  decoded values will be bitwise-anded with the given mask before being
  returned, e.g. to limit them to 32 bits.  the returned decoder does not
  take the usual "end" parameter -- the caller is expected to do bounds checking
  after the fact (often the caller can defer such checking until later).  the
  decoder returns a (value, new_pos) pair.
  """

  local_ord = ord
  def decodevarint(buffer, pos):
    result = 0
    shift = 0
    while 1:
      b = local_ord(buffer[pos])
      result |= ((b & 0x7f) << shift)
      pos += 1
      if not (b & 0x80):
        result &= mask
        return (result, pos)
      shift += 7
      if shift >= 64:
        raise _decodeerror('too many bytes when decoding varint.')
  return decodevarint


def _signedvarintdecoder(mask):
  """like _varintdecoder() but decodes signed values."""

  local_ord = ord
  def decodevarint(buffer, pos):
    result = 0
    shift = 0
    while 1:
      b = local_ord(buffer[pos])
      result |= ((b & 0x7f) << shift)
      pos += 1
      if not (b & 0x80):
        if result > 0x7fffffffffffffff:
          result -= (1 << 64)
          result |= ~mask
        else:
          result &= mask
        return (result, pos)
      shift += 7
      if shift >= 64:
        raise _decodeerror('too many bytes when decoding varint.')
  return decodevarint


_decodevarint = _varintdecoder((1 << 64) - 1)
_decodesignedvarint = _signedvarintdecoder((1 << 64) - 1)

# use these versions for values which must be limited to 32 bits.
_decodevarint32 = _varintdecoder((1 << 32) - 1)
_decodesignedvarint32 = _signedvarintdecoder((1 << 32) - 1)


def readtag(buffer, pos):
  """read a tag from the buffer, and return a (tag_bytes, new_pos) tuple.

  we return the raw bytes of the tag rather than decoding them.  the raw
  bytes can then be used to look up the proper decoder.  this effectively allows
  us to trade some work that would be done in pure-python (decoding a varint)
  for work that is done in c (searching for a byte string in a hash table).
  in a low-level language it would be much cheaper to decode the varint and
  use that, but not in python.
  """

  start = pos
  while ord(buffer[pos]) & 0x80:
    pos += 1
  pos += 1
  return (buffer[start:pos], pos)


# --------------------------------------------------------------------


def _simpledecoder(wire_type, decode_value):
  """return a constructor for a decoder for fields of a particular type.

  args:
      wire_type:  the field's wire type.
      decode_value:  a function which decodes an individual value, e.g.
        _decodevarint()
  """

  def specificdecoder(field_number, is_repeated, is_packed, key, new_default):
    if is_packed:
      local_decodevarint = _decodevarint
      def decodepackedfield(buffer, pos, end, message, field_dict):
        value = field_dict.get(key)
        if value is none:
          value = field_dict.setdefault(key, new_default(message))
        (endpoint, pos) = local_decodevarint(buffer, pos)
        endpoint += pos
        if endpoint > end:
          raise _decodeerror('truncated message.')
        while pos < endpoint:
          (element, pos) = decode_value(buffer, pos)
          value.append(element)
        if pos > endpoint:
          del value[-1]   # discard corrupt value.
          raise _decodeerror('packed element was truncated.')
        return pos
      return decodepackedfield
    elif is_repeated:
      tag_bytes = encoder.tagbytes(field_number, wire_type)
      tag_len = len(tag_bytes)
      def decoderepeatedfield(buffer, pos, end, message, field_dict):
        value = field_dict.get(key)
        if value is none:
          value = field_dict.setdefault(key, new_default(message))
        while 1:
          (element, new_pos) = decode_value(buffer, pos)
          value.append(element)
          # predict that the next tag is another copy of the same repeated
          # field.
          pos = new_pos + tag_len
          if buffer[new_pos:pos] != tag_bytes or new_pos >= end:
            # prediction failed.  return.
            if new_pos > end:
              raise _decodeerror('truncated message.')
            return new_pos
      return decoderepeatedfield
    else:
      def decodefield(buffer, pos, end, message, field_dict):
        (field_dict[key], pos) = decode_value(buffer, pos)
        if pos > end:
          del field_dict[key]  # discard corrupt value.
          raise _decodeerror('truncated message.')
        return pos
      return decodefield

  return specificdecoder


def _modifieddecoder(wire_type, decode_value, modify_value):
  """like simpledecoder but additionally invokes modify_value on every value
  before storing it.  usually modify_value is zigzagdecode.
  """

  # reusing _simpledecoder is slightly slower than copying a bunch of code, but
  # not enough to make a significant difference.

  def innerdecode(buffer, pos):
    (result, new_pos) = decode_value(buffer, pos)
    return (modify_value(result), new_pos)
  return _simpledecoder(wire_type, innerdecode)


def _structpackdecoder(wire_type, format):
  """return a constructor for a decoder for a fixed-width field.

  args:
      wire_type:  the field's wire type.
      format:  the format string to pass to struct.unpack().
  """

  value_size = struct.calcsize(format)
  local_unpack = struct.unpack

  # reusing _simpledecoder is slightly slower than copying a bunch of code, but
  # not enough to make a significant difference.

  # note that we expect someone up-stack to catch struct.error and convert
  # it to _decodeerror -- this way we don't have to set up exception-
  # handling blocks every time we parse one value.

  def innerdecode(buffer, pos):
    new_pos = pos + value_size
    result = local_unpack(format, buffer[pos:new_pos])[0]
    return (result, new_pos)
  return _simpledecoder(wire_type, innerdecode)


def _floatdecoder():
  """returns a decoder for a float field.

  this code works around a bug in struct.unpack for non-finite 32-bit
  floating-point values.
  """

  local_unpack = struct.unpack

  def innerdecode(buffer, pos):
    # we expect a 32-bit value in little-endian byte order.  bit 1 is the sign
    # bit, bits 2-9 represent the exponent, and bits 10-32 are the significand.
    new_pos = pos + 4
    float_bytes = buffer[pos:new_pos]

    # if this value has all its exponent bits set, then it's non-finite.
    # in python 2.4, struct.unpack will convert it to a finite 64-bit value.
    # to avoid that, we parse it specially.
    if ((float_bytes[3] in '\x7f\xff')
        and (float_bytes[2] >= '\x80')):
      # if at least one significand bit is set...
      if float_bytes[0:3] != '\x00\x00\x80':
        return (_nan, new_pos)
      # if sign bit is set...
      if float_bytes[3] == '\xff':
        return (_neg_inf, new_pos)
      return (_pos_inf, new_pos)

    # note that we expect someone up-stack to catch struct.error and convert
    # it to _decodeerror -- this way we don't have to set up exception-
    # handling blocks every time we parse one value.
    result = local_unpack('<f', float_bytes)[0]
    return (result, new_pos)
  return _simpledecoder(wire_format.wiretype_fixed32, innerdecode)


def _doubledecoder():
  """returns a decoder for a double field.

  this code works around a bug in struct.unpack for not-a-number.
  """

  local_unpack = struct.unpack

  def innerdecode(buffer, pos):
    # we expect a 64-bit value in little-endian byte order.  bit 1 is the sign
    # bit, bits 2-12 represent the exponent, and bits 13-64 are the significand.
    new_pos = pos + 8
    double_bytes = buffer[pos:new_pos]

    # if this value has all its exponent bits set and at least one significand
    # bit set, it's not a number.  in python 2.4, struct.unpack will treat it
    # as inf or -inf.  to avoid that, we treat it specially.
    if ((double_bytes[7] in '\x7f\xff')
        and (double_bytes[6] >= '\xf0')
        and (double_bytes[0:7] != '\x00\x00\x00\x00\x00\x00\xf0')):
      return (_nan, new_pos)

    # note that we expect someone up-stack to catch struct.error and convert
    # it to _decodeerror -- this way we don't have to set up exception-
    # handling blocks every time we parse one value.
    result = local_unpack('<d', double_bytes)[0]
    return (result, new_pos)
  return _simpledecoder(wire_format.wiretype_fixed64, innerdecode)


# --------------------------------------------------------------------


int32decoder = enumdecoder = _simpledecoder(
    wire_format.wiretype_varint, _decodesignedvarint32)

int64decoder = _simpledecoder(
    wire_format.wiretype_varint, _decodesignedvarint)

uint32decoder = _simpledecoder(wire_format.wiretype_varint, _decodevarint32)
uint64decoder = _simpledecoder(wire_format.wiretype_varint, _decodevarint)

sint32decoder = _modifieddecoder(
    wire_format.wiretype_varint, _decodevarint32, wire_format.zigzagdecode)
sint64decoder = _modifieddecoder(
    wire_format.wiretype_varint, _decodevarint, wire_format.zigzagdecode)

# note that python conveniently guarantees that when using the '<' prefix on
# formats, they will also have the same size across all platforms (as opposed
# to without the prefix, where their sizes depend on the c compiler's basic
# type sizes).
fixed32decoder  = _structpackdecoder(wire_format.wiretype_fixed32, '<i')
fixed64decoder  = _structpackdecoder(wire_format.wiretype_fixed64, '<q')
sfixed32decoder = _structpackdecoder(wire_format.wiretype_fixed32, '<i')
sfixed64decoder = _structpackdecoder(wire_format.wiretype_fixed64, '<q')
floatdecoder = _floatdecoder()
doubledecoder = _doubledecoder()

booldecoder = _modifieddecoder(
    wire_format.wiretype_varint, _decodevarint, bool)


def stringdecoder(field_number, is_repeated, is_packed, key, new_default):
  """returns a decoder for a string field."""

  local_decodevarint = _decodevarint
  local_unicode = unicode

  assert not is_packed
  if is_repeated:
    tag_bytes = encoder.tagbytes(field_number,
                                 wire_format.wiretype_length_delimited)
    tag_len = len(tag_bytes)
    def decoderepeatedfield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      while 1:
        (size, pos) = local_decodevarint(buffer, pos)
        new_pos = pos + size
        if new_pos > end:
          raise _decodeerror('truncated string.')
        value.append(local_unicode(buffer[pos:new_pos], 'utf-8'))
        # predict that the next tag is another copy of the same repeated field.
        pos = new_pos + tag_len
        if buffer[new_pos:pos] != tag_bytes or new_pos == end:
          # prediction failed.  return.
          return new_pos
    return decoderepeatedfield
  else:
    def decodefield(buffer, pos, end, message, field_dict):
      (size, pos) = local_decodevarint(buffer, pos)
      new_pos = pos + size
      if new_pos > end:
        raise _decodeerror('truncated string.')
      field_dict[key] = local_unicode(buffer[pos:new_pos], 'utf-8')
      return new_pos
    return decodefield


def bytesdecoder(field_number, is_repeated, is_packed, key, new_default):
  """returns a decoder for a bytes field."""

  local_decodevarint = _decodevarint

  assert not is_packed
  if is_repeated:
    tag_bytes = encoder.tagbytes(field_number,
                                 wire_format.wiretype_length_delimited)
    tag_len = len(tag_bytes)
    def decoderepeatedfield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      while 1:
        (size, pos) = local_decodevarint(buffer, pos)
        new_pos = pos + size
        if new_pos > end:
          raise _decodeerror('truncated string.')
        value.append(buffer[pos:new_pos])
        # predict that the next tag is another copy of the same repeated field.
        pos = new_pos + tag_len
        if buffer[new_pos:pos] != tag_bytes or new_pos == end:
          # prediction failed.  return.
          return new_pos
    return decoderepeatedfield
  else:
    def decodefield(buffer, pos, end, message, field_dict):
      (size, pos) = local_decodevarint(buffer, pos)
      new_pos = pos + size
      if new_pos > end:
        raise _decodeerror('truncated string.')
      field_dict[key] = buffer[pos:new_pos]
      return new_pos
    return decodefield


def groupdecoder(field_number, is_repeated, is_packed, key, new_default):
  """returns a decoder for a group field."""

  end_tag_bytes = encoder.tagbytes(field_number,
                                   wire_format.wiretype_end_group)
  end_tag_len = len(end_tag_bytes)

  assert not is_packed
  if is_repeated:
    tag_bytes = encoder.tagbytes(field_number,
                                 wire_format.wiretype_start_group)
    tag_len = len(tag_bytes)
    def decoderepeatedfield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      while 1:
        value = field_dict.get(key)
        if value is none:
          value = field_dict.setdefault(key, new_default(message))
        # read sub-message.
        pos = value.add()._internalparse(buffer, pos, end)
        # read end tag.
        new_pos = pos+end_tag_len
        if buffer[pos:new_pos] != end_tag_bytes or new_pos > end:
          raise _decodeerror('missing group end tag.')
        # predict that the next tag is another copy of the same repeated field.
        pos = new_pos + tag_len
        if buffer[new_pos:pos] != tag_bytes or new_pos == end:
          # prediction failed.  return.
          return new_pos
    return decoderepeatedfield
  else:
    def decodefield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      # read sub-message.
      pos = value._internalparse(buffer, pos, end)
      # read end tag.
      new_pos = pos+end_tag_len
      if buffer[pos:new_pos] != end_tag_bytes or new_pos > end:
        raise _decodeerror('missing group end tag.')
      return new_pos
    return decodefield


def messagedecoder(field_number, is_repeated, is_packed, key, new_default):
  """returns a decoder for a message field."""

  local_decodevarint = _decodevarint

  assert not is_packed
  if is_repeated:
    tag_bytes = encoder.tagbytes(field_number,
                                 wire_format.wiretype_length_delimited)
    tag_len = len(tag_bytes)
    def decoderepeatedfield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      while 1:
        value = field_dict.get(key)
        if value is none:
          value = field_dict.setdefault(key, new_default(message))
        # read length.
        (size, pos) = local_decodevarint(buffer, pos)
        new_pos = pos + size
        if new_pos > end:
          raise _decodeerror('truncated message.')
        # read sub-message.
        if value.add()._internalparse(buffer, pos, new_pos) != new_pos:
          # the only reason _internalparse would return early is if it
          # encountered an end-group tag.
          raise _decodeerror('unexpected end-group tag.')
        # predict that the next tag is another copy of the same repeated field.
        pos = new_pos + tag_len
        if buffer[new_pos:pos] != tag_bytes or new_pos == end:
          # prediction failed.  return.
          return new_pos
    return decoderepeatedfield
  else:
    def decodefield(buffer, pos, end, message, field_dict):
      value = field_dict.get(key)
      if value is none:
        value = field_dict.setdefault(key, new_default(message))
      # read length.
      (size, pos) = local_decodevarint(buffer, pos)
      new_pos = pos + size
      if new_pos > end:
        raise _decodeerror('truncated message.')
      # read sub-message.
      if value._internalparse(buffer, pos, new_pos) != new_pos:
        # the only reason _internalparse would return early is if it encountered
        # an end-group tag.
        raise _decodeerror('unexpected end-group tag.')
      return new_pos
    return decodefield


# --------------------------------------------------------------------

message_set_item_tag = encoder.tagbytes(1, wire_format.wiretype_start_group)

def messagesetitemdecoder(extensions_by_number):
  """returns a decoder for a messageset item.

  the parameter is the _extensions_by_number map for the message class.

  the message set message looks like this:
    message messageset {
      repeated group item = 1 {
        required int32 type_id = 2;
        required string message = 3;
      }
    }
  """

  type_id_tag_bytes = encoder.tagbytes(2, wire_format.wiretype_varint)
  message_tag_bytes = encoder.tagbytes(3, wire_format.wiretype_length_delimited)
  item_end_tag_bytes = encoder.tagbytes(1, wire_format.wiretype_end_group)

  local_readtag = readtag
  local_decodevarint = _decodevarint
  local_skipfield = skipfield

  def decodeitem(buffer, pos, end, message, field_dict):
    message_set_item_start = pos
    type_id = -1
    message_start = -1
    message_end = -1

    # technically, type_id and message can appear in any order, so we need
    # a little loop here.
    while 1:
      (tag_bytes, pos) = local_readtag(buffer, pos)
      if tag_bytes == type_id_tag_bytes:
        (type_id, pos) = local_decodevarint(buffer, pos)
      elif tag_bytes == message_tag_bytes:
        (size, message_start) = local_decodevarint(buffer, pos)
        pos = message_end = message_start + size
      elif tag_bytes == item_end_tag_bytes:
        break
      else:
        pos = skipfield(buffer, pos, end, tag_bytes)
        if pos == -1:
          raise _decodeerror('missing group end tag.')

    if pos > end:
      raise _decodeerror('truncated message.')

    if type_id == -1:
      raise _decodeerror('messageset item missing type_id.')
    if message_start == -1:
      raise _decodeerror('messageset item missing message.')

    extension = extensions_by_number.get(type_id)
    if extension is not none:
      value = field_dict.get(extension)
      if value is none:
        value = field_dict.setdefault(
            extension, extension.message_type._concrete_class())
      if value._internalparse(buffer, message_start,message_end) != message_end:
        # the only reason _internalparse would return early is if it encountered
        # an end-group tag.
        raise _decodeerror('unexpected end-group tag.')
    else:
      if not message._unknown_fields:
        message._unknown_fields = []
      message._unknown_fields.append((message_set_item_tag,
                                      buffer[message_set_item_start:pos]))

    return pos

  return decodeitem

# --------------------------------------------------------------------
# optimization is not as heavy here because calls to skipfield() are rare,
# except for handling end-group tags.

def _skipvarint(buffer, pos, end):
  """skip a varint value.  returns the new position."""

  while ord(buffer[pos]) & 0x80:
    pos += 1
  pos += 1
  if pos > end:
    raise _decodeerror('truncated message.')
  return pos

def _skipfixed64(buffer, pos, end):
  """skip a fixed64 value.  returns the new position."""

  pos += 8
  if pos > end:
    raise _decodeerror('truncated message.')
  return pos

def _skiplengthdelimited(buffer, pos, end):
  """skip a length-delimited value.  returns the new position."""

  (size, pos) = _decodevarint(buffer, pos)
  pos += size
  if pos > end:
    raise _decodeerror('truncated message.')
  return pos

def _skipgroup(buffer, pos, end):
  """skip sub-group.  returns the new position."""

  while 1:
    (tag_bytes, pos) = readtag(buffer, pos)
    new_pos = skipfield(buffer, pos, end, tag_bytes)
    if new_pos == -1:
      return pos
    pos = new_pos

def _endgroup(buffer, pos, end):
  """skipping an end_group tag returns -1 to tell the parent loop to break."""

  return -1

def _skipfixed32(buffer, pos, end):
  """skip a fixed32 value.  returns the new position."""

  pos += 4
  if pos > end:
    raise _decodeerror('truncated message.')
  return pos

def _raiseinvalidwiretype(buffer, pos, end):
  """skip function for unknown wire types.  raises an exception."""

  raise _decodeerror('tag had invalid wire type.')

def _fieldskipper():
  """constructs the skipfield function."""

  wiretype_to_skipper = [
      _skipvarint,
      _skipfixed64,
      _skiplengthdelimited,
      _skipgroup,
      _endgroup,
      _skipfixed32,
      _raiseinvalidwiretype,
      _raiseinvalidwiretype,
      ]

  wiretype_mask = wire_format.tag_type_mask
  local_ord = ord

  def skipfield(buffer, pos, end, tag_bytes):
    """skips a field with the specified tag.

    |pos| should point to the byte immediately after the tag.

    returns:
        the new position (after the tag value), or -1 if the tag is an end-group
        tag (in which case the calling loop should break).
    """

    # the wire type is always in the first byte since varints are little-endian.
    wire_type = local_ord(tag_bytes[0]) & wiretype_mask
    return wiretype_to_skipper[wire_type](buffer, pos, end)

  return skipfield

skipfield = _fieldskipper()
