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

"""constants and static functions to support protocol buffer wire format."""

__author__ = 'robinson@google.com (will robinson)'

import struct
from google.protobuf import descriptor
from google.protobuf import message


tag_type_bits = 3  # number of bits used to hold type info in a proto tag.
tag_type_mask = (1 << tag_type_bits) - 1  # 0x7

# these numbers identify the wire type of a protocol buffer value.
# we use the least-significant tag_type_bits bits of the varint-encoded
# tag-and-type to store one of these wiretype_* constants.
# these values must match wiretype enum in google/protobuf/wire_format.h.
wiretype_varint = 0
wiretype_fixed64 = 1
wiretype_length_delimited = 2
wiretype_start_group = 3
wiretype_end_group = 4
wiretype_fixed32 = 5
_wiretype_max = 5


# bounds for various integer types.
int32_max = int((1 << 31) - 1)
int32_min = int(-(1 << 31))
uint32_max = (1 << 32) - 1

int64_max = (1 << 63) - 1
int64_min = -(1 << 63)
uint64_max = (1 << 64) - 1

# "struct" format strings that will encode/decode the specified formats.
format_uint32_little_endian = '<i'
format_uint64_little_endian = '<q'
format_float_little_endian = '<f'
format_double_little_endian = '<d'


# we'll have to provide alternate implementations of appendlittleendian*() on
# any architectures where these checks fail.
if struct.calcsize(format_uint32_little_endian) != 4:
  raise assertionerror('format "i" is not a 32-bit number.')
if struct.calcsize(format_uint64_little_endian) != 8:
  raise assertionerror('format "q" is not a 64-bit number.')


def packtag(field_number, wire_type):
  """returns an unsigned 32-bit integer that encodes the field number and
  wire type information in standard protocol message wire format.

  args:
    field_number: expected to be an integer in the range [1, 1 << 29)
    wire_type: one of the wiretype_* constants.
  """
  if not 0 <= wire_type <= _wiretype_max:
    raise message.encodeerror('unknown wire type: %d' % wire_type)
  return (field_number << tag_type_bits) | wire_type


def unpacktag(tag):
  """the inverse of packtag().  given an unsigned 32-bit number,
  returns a (field_number, wire_type) tuple.
  """
  return (tag >> tag_type_bits), (tag & tag_type_mask)


def zigzagencode(value):
  """zigzag transform:  encodes signed integers so that they can be
  effectively used with varint encoding.  see wire_format.h for
  more details.
  """
  if value >= 0:
    return value << 1
  return (value << 1) ^ (~0)


def zigzagdecode(value):
  """inverse of zigzagencode()."""
  if not value & 0x1:
    return value >> 1
  return (value >> 1) ^ (~0)



# the *bytesize() functions below return the number of bytes required to
# serialize "field number + type" information and then serialize the value.


def int32bytesize(field_number, int32):
  return int64bytesize(field_number, int32)


def int32bytesizenotag(int32):
  return _varuint64bytesizenotag(0xffffffffffffffff & int32)


def int64bytesize(field_number, int64):
  # have to convert to uint before calling uint64bytesize().
  return uint64bytesize(field_number, 0xffffffffffffffff & int64)


def uint32bytesize(field_number, uint32):
  return uint64bytesize(field_number, uint32)


def uint64bytesize(field_number, uint64):
  return tagbytesize(field_number) + _varuint64bytesizenotag(uint64)


def sint32bytesize(field_number, int32):
  return uint32bytesize(field_number, zigzagencode(int32))


def sint64bytesize(field_number, int64):
  return uint64bytesize(field_number, zigzagencode(int64))


def fixed32bytesize(field_number, fixed32):
  return tagbytesize(field_number) + 4


def fixed64bytesize(field_number, fixed64):
  return tagbytesize(field_number) + 8


def sfixed32bytesize(field_number, sfixed32):
  return tagbytesize(field_number) + 4


def sfixed64bytesize(field_number, sfixed64):
  return tagbytesize(field_number) + 8


def floatbytesize(field_number, flt):
  return tagbytesize(field_number) + 4


def doublebytesize(field_number, double):
  return tagbytesize(field_number) + 8


def boolbytesize(field_number, b):
  return tagbytesize(field_number) + 1


def enumbytesize(field_number, enum):
  return uint32bytesize(field_number, enum)


def stringbytesize(field_number, string):
  return bytesbytesize(field_number, string.encode('utf-8'))


def bytesbytesize(field_number, b):
  return (tagbytesize(field_number)
          + _varuint64bytesizenotag(len(b))
          + len(b))


def groupbytesize(field_number, message):
  return (2 * tagbytesize(field_number)  # start and end group.
          + message.bytesize())


def messagebytesize(field_number, message):
  return (tagbytesize(field_number)
          + _varuint64bytesizenotag(message.bytesize())
          + message.bytesize())


def messagesetitembytesize(field_number, msg):
  # first compute the sizes of the tags.
  # there are 2 tags for the beginning and ending of the repeated group, that
  # is field number 1, one with field number 2 (type_id) and one with field
  # number 3 (message).
  total_size = (2 * tagbytesize(1) + tagbytesize(2) + tagbytesize(3))

  # add the number of bytes for type_id.
  total_size += _varuint64bytesizenotag(field_number)

  message_size = msg.bytesize()

  # the number of bytes for encoding the length of the message.
  total_size += _varuint64bytesizenotag(message_size)

  # the size of the message.
  total_size += message_size
  return total_size


def tagbytesize(field_number):
  """returns the bytes required to serialize a tag with this field number."""
  # just pass in type 0, since the type won't affect the tag+type size.
  return _varuint64bytesizenotag(packtag(field_number, 0))


# private helper function for the *bytesize() functions above.

def _varuint64bytesizenotag(uint64):
  """returns the number of bytes required to serialize a single varint
  using boundary value comparisons. (unrolled loop optimization -wpierce)
  uint64 must be unsigned.
  """
  if uint64 <= 0x7f: return 1
  if uint64 <= 0x3fff: return 2
  if uint64 <= 0x1fffff: return 3
  if uint64 <= 0xfffffff: return 4
  if uint64 <= 0x7ffffffff: return 5
  if uint64 <= 0x3ffffffffff: return 6
  if uint64 <= 0x1ffffffffffff: return 7
  if uint64 <= 0xffffffffffffff: return 8
  if uint64 <= 0x7fffffffffffffff: return 9
  if uint64 > uint64_max:
    raise message.encodeerror('value out of range: %d' % uint64)
  return 10


non_packable_types = (
  descriptor.fielddescriptor.type_string,
  descriptor.fielddescriptor.type_group,
  descriptor.fielddescriptor.type_message,
  descriptor.fielddescriptor.type_bytes
)


def istypepackable(field_type):
  """return true iff packable = true is valid for fields of this type.

  args:
    field_type: a fielddescriptor::type value.

  returns:
    true iff fields of this type are packable.
  """
  return field_type not in non_packable_types
