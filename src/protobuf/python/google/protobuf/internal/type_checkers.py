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

"""provides type checking routines.

this module defines type checking utilities in the forms of dictionaries:

value_checkers: a dictionary of field types and a value validation object.
type_to_byte_size_fn: a dictionary with field types and a size computing
  function.
type_to_serialize_method: a dictionary with field types and serialization
  function.
field_type_to_wire_type: a dictionary with field typed and their
  coresponding wire types.
type_to_deserialize_method: a dictionary with field types and deserialization
  function.
"""

__author__ = 'robinson@google.com (will robinson)'

from google.protobuf.internal import decoder
from google.protobuf.internal import encoder
from google.protobuf.internal import wire_format
from google.protobuf import descriptor

_fielddescriptor = descriptor.fielddescriptor


def gettypechecker(cpp_type, field_type):
  """returns a type checker for a message field of the specified types.

  args:
    cpp_type: c++ type of the field (see descriptor.py).
    field_type: protocol message field type (see descriptor.py).

  returns:
    an instance of typechecker which can be used to verify the types
    of values assigned to a field of the specified type.
  """
  if (cpp_type == _fielddescriptor.cpptype_string and
      field_type == _fielddescriptor.type_string):
    return unicodevaluechecker()
  return _value_checkers[cpp_type]


# none of the typecheckers below make any attempt to guard against people
# subclassing builtin types and doing weird things.  we're not trying to
# protect against malicious clients here, just people accidentally shooting
# themselves in the foot in obvious ways.

class typechecker(object):

  """type checker used to catch type errors as early as possible
  when the client is setting scalar fields in protocol messages.
  """

  def __init__(self, *acceptable_types):
    self._acceptable_types = acceptable_types

  def checkvalue(self, proposed_value):
    if not isinstance(proposed_value, self._acceptable_types):
      message = ('%.1024r has type %s, but expected one of: %s' %
                 (proposed_value, type(proposed_value), self._acceptable_types))
      raise typeerror(message)


# intvaluechecker and its subclasses perform integer type-checks
# and bounds-checks.
class intvaluechecker(object):

  """checker used for integer fields.  performs type-check and range check."""

  def checkvalue(self, proposed_value):
    if not isinstance(proposed_value, (int, long)):
      message = ('%.1024r has type %s, but expected one of: %s' %
                 (proposed_value, type(proposed_value), (int, long)))
      raise typeerror(message)
    if not self._min <= proposed_value <= self._max:
      raise valueerror('value out of range: %d' % proposed_value)


class unicodevaluechecker(object):

  """checker used for string fields."""

  def checkvalue(self, proposed_value):
    if not isinstance(proposed_value, (str, unicode)):
      message = ('%.1024r has type %s, but expected one of: %s' %
                 (proposed_value, type(proposed_value), (str, unicode)))
      raise typeerror(message)

    # if the value is of type 'str' make sure that it is in 7-bit ascii
    # encoding.
    if isinstance(proposed_value, str):
      try:
        unicode(proposed_value, 'ascii')
      except unicodedecodeerror:
        raise valueerror('%.1024r has type str, but isn\'t in 7-bit ascii '
                         'encoding. non-ascii strings must be converted to '
                         'unicode objects before being added.' %
                         (proposed_value))


class int32valuechecker(intvaluechecker):
  # we're sure to use ints instead of longs here since comparison may be more
  # efficient.
  _min = -2147483648
  _max = 2147483647


class uint32valuechecker(intvaluechecker):
  _min = 0
  _max = (1 << 32) - 1


class int64valuechecker(intvaluechecker):
  _min = -(1 << 63)
  _max = (1 << 63) - 1


class uint64valuechecker(intvaluechecker):
  _min = 0
  _max = (1 << 64) - 1


# type-checkers for all scalar cpptypes.
_value_checkers = {
    _fielddescriptor.cpptype_int32: int32valuechecker(),
    _fielddescriptor.cpptype_int64: int64valuechecker(),
    _fielddescriptor.cpptype_uint32: uint32valuechecker(),
    _fielddescriptor.cpptype_uint64: uint64valuechecker(),
    _fielddescriptor.cpptype_double: typechecker(
        float, int, long),
    _fielddescriptor.cpptype_float: typechecker(
        float, int, long),
    _fielddescriptor.cpptype_bool: typechecker(bool, int),
    _fielddescriptor.cpptype_enum: int32valuechecker(),
    _fielddescriptor.cpptype_string: typechecker(str),
    }


# map from field type to a function f, such that f(field_num, value)
# gives the total byte size for a value of the given type.  this
# byte size includes tag information and any other additional space
# associated with serializing "value".
type_to_byte_size_fn = {
    _fielddescriptor.type_double: wire_format.doublebytesize,
    _fielddescriptor.type_float: wire_format.floatbytesize,
    _fielddescriptor.type_int64: wire_format.int64bytesize,
    _fielddescriptor.type_uint64: wire_format.uint64bytesize,
    _fielddescriptor.type_int32: wire_format.int32bytesize,
    _fielddescriptor.type_fixed64: wire_format.fixed64bytesize,
    _fielddescriptor.type_fixed32: wire_format.fixed32bytesize,
    _fielddescriptor.type_bool: wire_format.boolbytesize,
    _fielddescriptor.type_string: wire_format.stringbytesize,
    _fielddescriptor.type_group: wire_format.groupbytesize,
    _fielddescriptor.type_message: wire_format.messagebytesize,
    _fielddescriptor.type_bytes: wire_format.bytesbytesize,
    _fielddescriptor.type_uint32: wire_format.uint32bytesize,
    _fielddescriptor.type_enum: wire_format.enumbytesize,
    _fielddescriptor.type_sfixed32: wire_format.sfixed32bytesize,
    _fielddescriptor.type_sfixed64: wire_format.sfixed64bytesize,
    _fielddescriptor.type_sint32: wire_format.sint32bytesize,
    _fielddescriptor.type_sint64: wire_format.sint64bytesize
    }


# maps from field types to encoder constructors.
type_to_encoder = {
    _fielddescriptor.type_double: encoder.doubleencoder,
    _fielddescriptor.type_float: encoder.floatencoder,
    _fielddescriptor.type_int64: encoder.int64encoder,
    _fielddescriptor.type_uint64: encoder.uint64encoder,
    _fielddescriptor.type_int32: encoder.int32encoder,
    _fielddescriptor.type_fixed64: encoder.fixed64encoder,
    _fielddescriptor.type_fixed32: encoder.fixed32encoder,
    _fielddescriptor.type_bool: encoder.boolencoder,
    _fielddescriptor.type_string: encoder.stringencoder,
    _fielddescriptor.type_group: encoder.groupencoder,
    _fielddescriptor.type_message: encoder.messageencoder,
    _fielddescriptor.type_bytes: encoder.bytesencoder,
    _fielddescriptor.type_uint32: encoder.uint32encoder,
    _fielddescriptor.type_enum: encoder.enumencoder,
    _fielddescriptor.type_sfixed32: encoder.sfixed32encoder,
    _fielddescriptor.type_sfixed64: encoder.sfixed64encoder,
    _fielddescriptor.type_sint32: encoder.sint32encoder,
    _fielddescriptor.type_sint64: encoder.sint64encoder,
    }


# maps from field types to sizer constructors.
type_to_sizer = {
    _fielddescriptor.type_double: encoder.doublesizer,
    _fielddescriptor.type_float: encoder.floatsizer,
    _fielddescriptor.type_int64: encoder.int64sizer,
    _fielddescriptor.type_uint64: encoder.uint64sizer,
    _fielddescriptor.type_int32: encoder.int32sizer,
    _fielddescriptor.type_fixed64: encoder.fixed64sizer,
    _fielddescriptor.type_fixed32: encoder.fixed32sizer,
    _fielddescriptor.type_bool: encoder.boolsizer,
    _fielddescriptor.type_string: encoder.stringsizer,
    _fielddescriptor.type_group: encoder.groupsizer,
    _fielddescriptor.type_message: encoder.messagesizer,
    _fielddescriptor.type_bytes: encoder.bytessizer,
    _fielddescriptor.type_uint32: encoder.uint32sizer,
    _fielddescriptor.type_enum: encoder.enumsizer,
    _fielddescriptor.type_sfixed32: encoder.sfixed32sizer,
    _fielddescriptor.type_sfixed64: encoder.sfixed64sizer,
    _fielddescriptor.type_sint32: encoder.sint32sizer,
    _fielddescriptor.type_sint64: encoder.sint64sizer,
    }


# maps from field type to a decoder constructor.
type_to_decoder = {
    _fielddescriptor.type_double: decoder.doubledecoder,
    _fielddescriptor.type_float: decoder.floatdecoder,
    _fielddescriptor.type_int64: decoder.int64decoder,
    _fielddescriptor.type_uint64: decoder.uint64decoder,
    _fielddescriptor.type_int32: decoder.int32decoder,
    _fielddescriptor.type_fixed64: decoder.fixed64decoder,
    _fielddescriptor.type_fixed32: decoder.fixed32decoder,
    _fielddescriptor.type_bool: decoder.booldecoder,
    _fielddescriptor.type_string: decoder.stringdecoder,
    _fielddescriptor.type_group: decoder.groupdecoder,
    _fielddescriptor.type_message: decoder.messagedecoder,
    _fielddescriptor.type_bytes: decoder.bytesdecoder,
    _fielddescriptor.type_uint32: decoder.uint32decoder,
    _fielddescriptor.type_enum: decoder.enumdecoder,
    _fielddescriptor.type_sfixed32: decoder.sfixed32decoder,
    _fielddescriptor.type_sfixed64: decoder.sfixed64decoder,
    _fielddescriptor.type_sint32: decoder.sint32decoder,
    _fielddescriptor.type_sint64: decoder.sint64decoder,
    }

# maps from field type to expected wiretype.
field_type_to_wire_type = {
    _fielddescriptor.type_double: wire_format.wiretype_fixed64,
    _fielddescriptor.type_float: wire_format.wiretype_fixed32,
    _fielddescriptor.type_int64: wire_format.wiretype_varint,
    _fielddescriptor.type_uint64: wire_format.wiretype_varint,
    _fielddescriptor.type_int32: wire_format.wiretype_varint,
    _fielddescriptor.type_fixed64: wire_format.wiretype_fixed64,
    _fielddescriptor.type_fixed32: wire_format.wiretype_fixed32,
    _fielddescriptor.type_bool: wire_format.wiretype_varint,
    _fielddescriptor.type_string:
      wire_format.wiretype_length_delimited,
    _fielddescriptor.type_group: wire_format.wiretype_start_group,
    _fielddescriptor.type_message:
      wire_format.wiretype_length_delimited,
    _fielddescriptor.type_bytes:
      wire_format.wiretype_length_delimited,
    _fielddescriptor.type_uint32: wire_format.wiretype_varint,
    _fielddescriptor.type_enum: wire_format.wiretype_varint,
    _fielddescriptor.type_sfixed32: wire_format.wiretype_fixed32,
    _fielddescriptor.type_sfixed64: wire_format.wiretype_fixed64,
    _fielddescriptor.type_sint32: wire_format.wiretype_varint,
    _fielddescriptor.type_sint64: wire_format.wiretype_varint,
    }
