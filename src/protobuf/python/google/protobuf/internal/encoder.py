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

"""code for encoding protocol message primitives.

contains the logic for encoding every logical protocol field type
into one of the 5 physical wire types.

this code is designed to push the python interpreter's performance to the
limits.

the basic idea is that at startup time, for every field (i.e. every
fielddescriptor) we construct two functions:  a "sizer" and an "encoder".  the
sizer takes a value of this field's type and computes its byte size.  the
encoder takes a writer function and a value.  it encodes the value into byte
strings and invokes the writer function to write those strings.  typically the
writer function is the write() method of a cstringio.

we try to do as much work as possible when constructing the writer and the
sizer rather than when calling them.  in particular:
* we copy any needed global functions to local variables, so that we do not need
  to do costly global table lookups at runtime.
* similarly, we try to do any attribute lookups at startup time if possible.
* every field's tag is encoded to bytes at startup, since it can't change at
  runtime.
* whatever component of the field size we can compute at startup, we do.
* we *avoid* sharing code if doing so would make the code slower and not sharing
  does not burden us too much.  for example, encoders for repeated fields do
  not just call the encoders for singular fields in a loop because this would
  add an extra function call overhead for every loop iteration; instead, we
  manually inline the single-value encoder into the loop.
* if a python function lacks a return statement, python actually generates
  instructions to pop the result of the last statement off the stack, push
  none onto the stack, and then return that.  if we really don't care what
  value is returned, then we can save two instructions by returning the
  result of the last statement.  it looks funny but it helps.
* we assume that type and bounds checking has happened at a higher level.
"""

__author__ = 'kenton@google.com (kenton varda)'

import struct
from google.protobuf.internal import wire_format


# this will overflow and thus become ieee-754 "infinity".  we would use
# "float('inf')" but it doesn't work on windows pre-python-2.6.
_pos_inf = 1e10000
_neg_inf = -_pos_inf


def _varintsize(value):
  """compute the size of a varint value."""
  if value <= 0x7f: return 1
  if value <= 0x3fff: return 2
  if value <= 0x1fffff: return 3
  if value <= 0xfffffff: return 4
  if value <= 0x7ffffffff: return 5
  if value <= 0x3ffffffffff: return 6
  if value <= 0x1ffffffffffff: return 7
  if value <= 0xffffffffffffff: return 8
  if value <= 0x7fffffffffffffff: return 9
  return 10


def _signedvarintsize(value):
  """compute the size of a signed varint value."""
  if value < 0: return 10
  if value <= 0x7f: return 1
  if value <= 0x3fff: return 2
  if value <= 0x1fffff: return 3
  if value <= 0xfffffff: return 4
  if value <= 0x7ffffffff: return 5
  if value <= 0x3ffffffffff: return 6
  if value <= 0x1ffffffffffff: return 7
  if value <= 0xffffffffffffff: return 8
  if value <= 0x7fffffffffffffff: return 9
  return 10


def _tagsize(field_number):
  """returns the number of bytes required to serialize a tag with this field
  number."""
  # just pass in type 0, since the type won't affect the tag+type size.
  return _varintsize(wire_format.packtag(field_number, 0))


# --------------------------------------------------------------------
# in this section we define some generic sizers.  each of these functions
# takes parameters specific to a particular field type, e.g. int32 or fixed64.
# it returns another function which in turn takes parameters specific to a
# particular field, e.g. the field number and whether it is repeated or packed.
# look at the next section to see how these are used.


def _simplesizer(compute_value_size):
  """a sizer which uses the function compute_value_size to compute the size of
  each value.  typically compute_value_size is _varintsize."""

  def specificsizer(field_number, is_repeated, is_packed):
    tag_size = _tagsize(field_number)
    if is_packed:
      local_varintsize = _varintsize
      def packedfieldsize(value):
        result = 0
        for element in value:
          result += compute_value_size(element)
        return result + local_varintsize(result) + tag_size
      return packedfieldsize
    elif is_repeated:
      def repeatedfieldsize(value):
        result = tag_size * len(value)
        for element in value:
          result += compute_value_size(element)
        return result
      return repeatedfieldsize
    else:
      def fieldsize(value):
        return tag_size + compute_value_size(value)
      return fieldsize

  return specificsizer


def _modifiedsizer(compute_value_size, modify_value):
  """like simplesizer, but modify_value is invoked on each value before it is
  passed to compute_value_size.  modify_value is typically zigzagencode."""

  def specificsizer(field_number, is_repeated, is_packed):
    tag_size = _tagsize(field_number)
    if is_packed:
      local_varintsize = _varintsize
      def packedfieldsize(value):
        result = 0
        for element in value:
          result += compute_value_size(modify_value(element))
        return result + local_varintsize(result) + tag_size
      return packedfieldsize
    elif is_repeated:
      def repeatedfieldsize(value):
        result = tag_size * len(value)
        for element in value:
          result += compute_value_size(modify_value(element))
        return result
      return repeatedfieldsize
    else:
      def fieldsize(value):
        return tag_size + compute_value_size(modify_value(value))
      return fieldsize

  return specificsizer


def _fixedsizer(value_size):
  """like _simplesizer except for a fixed-size field.  the input is the size
  of one value."""

  def specificsizer(field_number, is_repeated, is_packed):
    tag_size = _tagsize(field_number)
    if is_packed:
      local_varintsize = _varintsize
      def packedfieldsize(value):
        result = len(value) * value_size
        return result + local_varintsize(result) + tag_size
      return packedfieldsize
    elif is_repeated:
      element_size = value_size + tag_size
      def repeatedfieldsize(value):
        return len(value) * element_size
      return repeatedfieldsize
    else:
      field_size = value_size + tag_size
      def fieldsize(value):
        return field_size
      return fieldsize

  return specificsizer


# ====================================================================
# here we declare a sizer constructor for each field type.  each "sizer
# constructor" is a function that takes (field_number, is_repeated, is_packed)
# as parameters and returns a sizer, which in turn takes a field value as
# a parameter and returns its encoded size.


int32sizer = int64sizer = enumsizer = _simplesizer(_signedvarintsize)

uint32sizer = uint64sizer = _simplesizer(_varintsize)

sint32sizer = sint64sizer = _modifiedsizer(
    _signedvarintsize, wire_format.zigzagencode)

fixed32sizer = sfixed32sizer = floatsizer  = _fixedsizer(4)
fixed64sizer = sfixed64sizer = doublesizer = _fixedsizer(8)

boolsizer = _fixedsizer(1)


def stringsizer(field_number, is_repeated, is_packed):
  """returns a sizer for a string field."""

  tag_size = _tagsize(field_number)
  local_varintsize = _varintsize
  local_len = len
  assert not is_packed
  if is_repeated:
    def repeatedfieldsize(value):
      result = tag_size * len(value)
      for element in value:
        l = local_len(element.encode('utf-8'))
        result += local_varintsize(l) + l
      return result
    return repeatedfieldsize
  else:
    def fieldsize(value):
      l = local_len(value.encode('utf-8'))
      return tag_size + local_varintsize(l) + l
    return fieldsize


def bytessizer(field_number, is_repeated, is_packed):
  """returns a sizer for a bytes field."""

  tag_size = _tagsize(field_number)
  local_varintsize = _varintsize
  local_len = len
  assert not is_packed
  if is_repeated:
    def repeatedfieldsize(value):
      result = tag_size * len(value)
      for element in value:
        l = local_len(element)
        result += local_varintsize(l) + l
      return result
    return repeatedfieldsize
  else:
    def fieldsize(value):
      l = local_len(value)
      return tag_size + local_varintsize(l) + l
    return fieldsize


def groupsizer(field_number, is_repeated, is_packed):
  """returns a sizer for a group field."""

  tag_size = _tagsize(field_number) * 2
  assert not is_packed
  if is_repeated:
    def repeatedfieldsize(value):
      result = tag_size * len(value)
      for element in value:
        result += element.bytesize()
      return result
    return repeatedfieldsize
  else:
    def fieldsize(value):
      return tag_size + value.bytesize()
    return fieldsize


def messagesizer(field_number, is_repeated, is_packed):
  """returns a sizer for a message field."""

  tag_size = _tagsize(field_number)
  local_varintsize = _varintsize
  assert not is_packed
  if is_repeated:
    def repeatedfieldsize(value):
      result = tag_size * len(value)
      for element in value:
        l = element.bytesize()
        result += local_varintsize(l) + l
      return result
    return repeatedfieldsize
  else:
    def fieldsize(value):
      l = value.bytesize()
      return tag_size + local_varintsize(l) + l
    return fieldsize


# --------------------------------------------------------------------
# messageset is special.


def messagesetitemsizer(field_number):
  """returns a sizer for extensions of messageset.

  the message set message looks like this:
    message messageset {
      repeated group item = 1 {
        required int32 type_id = 2;
        required string message = 3;
      }
    }
  """
  static_size = (_tagsize(1) * 2 + _tagsize(2) + _varintsize(field_number) +
                 _tagsize(3))
  local_varintsize = _varintsize

  def fieldsize(value):
    l = value.bytesize()
    return static_size + local_varintsize(l) + l

  return fieldsize


# ====================================================================
# encoders!


def _varintencoder():
  """return an encoder for a basic varint value (does not include tag)."""

  local_chr = chr
  def encodevarint(write, value):
    bits = value & 0x7f
    value >>= 7
    while value:
      write(local_chr(0x80|bits))
      bits = value & 0x7f
      value >>= 7
    return write(local_chr(bits))

  return encodevarint


def _signedvarintencoder():
  """return an encoder for a basic signed varint value (does not include
  tag)."""

  local_chr = chr
  def encodesignedvarint(write, value):
    if value < 0:
      value += (1 << 64)
    bits = value & 0x7f
    value >>= 7
    while value:
      write(local_chr(0x80|bits))
      bits = value & 0x7f
      value >>= 7
    return write(local_chr(bits))

  return encodesignedvarint


_encodevarint = _varintencoder()
_encodesignedvarint = _signedvarintencoder()


def _varintbytes(value):
  """encode the given integer as a varint and return the bytes.  this is only
  called at startup time so it doesn't need to be fast."""

  pieces = []
  _encodevarint(pieces.append, value)
  return "".join(pieces)


def tagbytes(field_number, wire_type):
  """encode the given tag and return the bytes.  only called at startup."""

  return _varintbytes(wire_format.packtag(field_number, wire_type))

# --------------------------------------------------------------------
# as with sizers (see above), we have a number of common encoder
# implementations.


def _simpleencoder(wire_type, encode_value, compute_value_size):
  """return a constructor for an encoder for fields of a particular type.

  args:
      wire_type:  the field's wire type, for encoding tags.
      encode_value:  a function which encodes an individual value, e.g.
        _encodevarint().
      compute_value_size:  a function which computes the size of an individual
        value, e.g. _varintsize().
  """

  def specificencoder(field_number, is_repeated, is_packed):
    if is_packed:
      tag_bytes = tagbytes(field_number, wire_format.wiretype_length_delimited)
      local_encodevarint = _encodevarint
      def encodepackedfield(write, value):
        write(tag_bytes)
        size = 0
        for element in value:
          size += compute_value_size(element)
        local_encodevarint(write, size)
        for element in value:
          encode_value(write, element)
      return encodepackedfield
    elif is_repeated:
      tag_bytes = tagbytes(field_number, wire_type)
      def encoderepeatedfield(write, value):
        for element in value:
          write(tag_bytes)
          encode_value(write, element)
      return encoderepeatedfield
    else:
      tag_bytes = tagbytes(field_number, wire_type)
      def encodefield(write, value):
        write(tag_bytes)
        return encode_value(write, value)
      return encodefield

  return specificencoder


def _modifiedencoder(wire_type, encode_value, compute_value_size, modify_value):
  """like simpleencoder but additionally invokes modify_value on every value
  before passing it to encode_value.  usually modify_value is zigzagencode."""

  def specificencoder(field_number, is_repeated, is_packed):
    if is_packed:
      tag_bytes = tagbytes(field_number, wire_format.wiretype_length_delimited)
      local_encodevarint = _encodevarint
      def encodepackedfield(write, value):
        write(tag_bytes)
        size = 0
        for element in value:
          size += compute_value_size(modify_value(element))
        local_encodevarint(write, size)
        for element in value:
          encode_value(write, modify_value(element))
      return encodepackedfield
    elif is_repeated:
      tag_bytes = tagbytes(field_number, wire_type)
      def encoderepeatedfield(write, value):
        for element in value:
          write(tag_bytes)
          encode_value(write, modify_value(element))
      return encoderepeatedfield
    else:
      tag_bytes = tagbytes(field_number, wire_type)
      def encodefield(write, value):
        write(tag_bytes)
        return encode_value(write, modify_value(value))
      return encodefield

  return specificencoder


def _structpackencoder(wire_type, format):
  """return a constructor for an encoder for a fixed-width field.

  args:
      wire_type:  the field's wire type, for encoding tags.
      format:  the format string to pass to struct.pack().
  """

  value_size = struct.calcsize(format)

  def specificencoder(field_number, is_repeated, is_packed):
    local_struct_pack = struct.pack
    if is_packed:
      tag_bytes = tagbytes(field_number, wire_format.wiretype_length_delimited)
      local_encodevarint = _encodevarint
      def encodepackedfield(write, value):
        write(tag_bytes)
        local_encodevarint(write, len(value) * value_size)
        for element in value:
          write(local_struct_pack(format, element))
      return encodepackedfield
    elif is_repeated:
      tag_bytes = tagbytes(field_number, wire_type)
      def encoderepeatedfield(write, value):
        for element in value:
          write(tag_bytes)
          write(local_struct_pack(format, element))
      return encoderepeatedfield
    else:
      tag_bytes = tagbytes(field_number, wire_type)
      def encodefield(write, value):
        write(tag_bytes)
        return write(local_struct_pack(format, value))
      return encodefield

  return specificencoder


def _floatingpointencoder(wire_type, format):
  """return a constructor for an encoder for float fields.

  this is like structpackencoder, but catches errors that may be due to
  passing non-finite floating-point values to struct.pack, and makes a
  second attempt to encode those values.

  args:
      wire_type:  the field's wire type, for encoding tags.
      format:  the format string to pass to struct.pack().
  """

  value_size = struct.calcsize(format)
  if value_size == 4:
    def encodenonfiniteorraise(write, value):
      # remember that the serialized form uses little-endian byte order.
      if value == _pos_inf:
        write('\x00\x00\x80\x7f')
      elif value == _neg_inf:
        write('\x00\x00\x80\xff')
      elif value != value:           # nan
        write('\x00\x00\xc0\x7f')
      else:
        raise
  elif value_size == 8:
    def encodenonfiniteorraise(write, value):
      if value == _pos_inf:
        write('\x00\x00\x00\x00\x00\x00\xf0\x7f')
      elif value == _neg_inf:
        write('\x00\x00\x00\x00\x00\x00\xf0\xff')
      elif value != value:                         # nan
        write('\x00\x00\x00\x00\x00\x00\xf8\x7f')
      else:
        raise
  else:
    raise valueerror('can\'t encode floating-point values that are '
                     '%d bytes long (only 4 or 8)' % value_size)

  def specificencoder(field_number, is_repeated, is_packed):
    local_struct_pack = struct.pack
    if is_packed:
      tag_bytes = tagbytes(field_number, wire_format.wiretype_length_delimited)
      local_encodevarint = _encodevarint
      def encodepackedfield(write, value):
        write(tag_bytes)
        local_encodevarint(write, len(value) * value_size)
        for element in value:
          # this try/except block is going to be faster than any code that
          # we could write to check whether element is finite.
          try:
            write(local_struct_pack(format, element))
          except systemerror:
            encodenonfiniteorraise(write, element)
      return encodepackedfield
    elif is_repeated:
      tag_bytes = tagbytes(field_number, wire_type)
      def encoderepeatedfield(write, value):
        for element in value:
          write(tag_bytes)
          try:
            write(local_struct_pack(format, element))
          except systemerror:
            encodenonfiniteorraise(write, element)
      return encoderepeatedfield
    else:
      tag_bytes = tagbytes(field_number, wire_type)
      def encodefield(write, value):
        write(tag_bytes)
        try:
          write(local_struct_pack(format, value))
        except systemerror:
          encodenonfiniteorraise(write, value)
      return encodefield

  return specificencoder


# ====================================================================
# here we declare an encoder constructor for each field type.  these work
# very similarly to sizer constructors, described earlier.


int32encoder = int64encoder = enumencoder = _simpleencoder(
    wire_format.wiretype_varint, _encodesignedvarint, _signedvarintsize)

uint32encoder = uint64encoder = _simpleencoder(
    wire_format.wiretype_varint, _encodevarint, _varintsize)

sint32encoder = sint64encoder = _modifiedencoder(
    wire_format.wiretype_varint, _encodevarint, _varintsize,
    wire_format.zigzagencode)

# note that python conveniently guarantees that when using the '<' prefix on
# formats, they will also have the same size across all platforms (as opposed
# to without the prefix, where their sizes depend on the c compiler's basic
# type sizes).
fixed32encoder  = _structpackencoder(wire_format.wiretype_fixed32, '<i')
fixed64encoder  = _structpackencoder(wire_format.wiretype_fixed64, '<q')
sfixed32encoder = _structpackencoder(wire_format.wiretype_fixed32, '<i')
sfixed64encoder = _structpackencoder(wire_format.wiretype_fixed64, '<q')
floatencoder    = _floatingpointencoder(wire_format.wiretype_fixed32, '<f')
doubleencoder   = _floatingpointencoder(wire_format.wiretype_fixed64, '<d')


def boolencoder(field_number, is_repeated, is_packed):
  """returns an encoder for a boolean field."""

  false_byte = chr(0)
  true_byte = chr(1)
  if is_packed:
    tag_bytes = tagbytes(field_number, wire_format.wiretype_length_delimited)
    local_encodevarint = _encodevarint
    def encodepackedfield(write, value):
      write(tag_bytes)
      local_encodevarint(write, len(value))
      for element in value:
        if element:
          write(true_byte)
        else:
          write(false_byte)
    return encodepackedfield
  elif is_repeated:
    tag_bytes = tagbytes(field_number, wire_format.wiretype_varint)
    def encoderepeatedfield(write, value):
      for element in value:
        write(tag_bytes)
        if element:
          write(true_byte)
        else:
          write(false_byte)
    return encoderepeatedfield
  else:
    tag_bytes = tagbytes(field_number, wire_format.wiretype_varint)
    def encodefield(write, value):
      write(tag_bytes)
      if value:
        return write(true_byte)
      return write(false_byte)
    return encodefield


def stringencoder(field_number, is_repeated, is_packed):
  """returns an encoder for a string field."""

  tag = tagbytes(field_number, wire_format.wiretype_length_delimited)
  local_encodevarint = _encodevarint
  local_len = len
  assert not is_packed
  if is_repeated:
    def encoderepeatedfield(write, value):
      for element in value:
        encoded = element.encode('utf-8')
        write(tag)
        local_encodevarint(write, local_len(encoded))
        write(encoded)
    return encoderepeatedfield
  else:
    def encodefield(write, value):
      encoded = value.encode('utf-8')
      write(tag)
      local_encodevarint(write, local_len(encoded))
      return write(encoded)
    return encodefield


def bytesencoder(field_number, is_repeated, is_packed):
  """returns an encoder for a bytes field."""

  tag = tagbytes(field_number, wire_format.wiretype_length_delimited)
  local_encodevarint = _encodevarint
  local_len = len
  assert not is_packed
  if is_repeated:
    def encoderepeatedfield(write, value):
      for element in value:
        write(tag)
        local_encodevarint(write, local_len(element))
        write(element)
    return encoderepeatedfield
  else:
    def encodefield(write, value):
      write(tag)
      local_encodevarint(write, local_len(value))
      return write(value)
    return encodefield


def groupencoder(field_number, is_repeated, is_packed):
  """returns an encoder for a group field."""

  start_tag = tagbytes(field_number, wire_format.wiretype_start_group)
  end_tag = tagbytes(field_number, wire_format.wiretype_end_group)
  assert not is_packed
  if is_repeated:
    def encoderepeatedfield(write, value):
      for element in value:
        write(start_tag)
        element._internalserialize(write)
        write(end_tag)
    return encoderepeatedfield
  else:
    def encodefield(write, value):
      write(start_tag)
      value._internalserialize(write)
      return write(end_tag)
    return encodefield


def messageencoder(field_number, is_repeated, is_packed):
  """returns an encoder for a message field."""

  tag = tagbytes(field_number, wire_format.wiretype_length_delimited)
  local_encodevarint = _encodevarint
  assert not is_packed
  if is_repeated:
    def encoderepeatedfield(write, value):
      for element in value:
        write(tag)
        local_encodevarint(write, element.bytesize())
        element._internalserialize(write)
    return encoderepeatedfield
  else:
    def encodefield(write, value):
      write(tag)
      local_encodevarint(write, value.bytesize())
      return value._internalserialize(write)
    return encodefield


# --------------------------------------------------------------------
# as before, messageset is special.


def messagesetitemencoder(field_number):
  """encoder for extensions of messageset.

  the message set message looks like this:
    message messageset {
      repeated group item = 1 {
        required int32 type_id = 2;
        required string message = 3;
      }
    }
  """
  start_bytes = "".join([
      tagbytes(1, wire_format.wiretype_start_group),
      tagbytes(2, wire_format.wiretype_varint),
      _varintbytes(field_number),
      tagbytes(3, wire_format.wiretype_length_delimited)])
  end_bytes = tagbytes(1, wire_format.wiretype_end_group)
  local_encodevarint = _encodevarint

  def encodefield(write, value):
    write(start_bytes)
    local_encodevarint(write, value.bytesize())
    value._internalserialize(write)
    return write(end_bytes)

  return encodefield
