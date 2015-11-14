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

"""utilities for python proto2 tests.

this is intentionally modeled on c++ code in
//google/protobuf/test_util.*.
"""

__author__ = 'robinson@google.com (will robinson)'

import os.path

from google.protobuf import unittest_import_pb2
from google.protobuf import unittest_pb2


def setallnonlazyfields(message):
  """sets every non-lazy field in the message to a unique value.

  args:
    message: a unittest_pb2.testalltypes instance.
  """

  #
  # optional fields.
  #

  message.optional_int32    = 101
  message.optional_int64    = 102
  message.optional_uint32   = 103
  message.optional_uint64   = 104
  message.optional_sint32   = 105
  message.optional_sint64   = 106
  message.optional_fixed32  = 107
  message.optional_fixed64  = 108
  message.optional_sfixed32 = 109
  message.optional_sfixed64 = 110
  message.optional_float    = 111
  message.optional_double   = 112
  message.optional_bool     = true
  # todo(robinson): firmly spec out and test how
  # protos interact with unicode.  one specific example:
  # what happens if we change the literal below to
  # u'115'?  what *should* happen?  still some discussion
  # to finish with kenton about bytes vs. strings
  # and forcing everything to be utf8. :-/
  message.optional_string   = '115'
  message.optional_bytes    = '116'

  message.optionalgroup.a = 117
  message.optional_nested_message.bb = 118
  message.optional_foreign_message.c = 119
  message.optional_import_message.d = 120
  message.optional_public_import_message.e = 126

  message.optional_nested_enum = unittest_pb2.testalltypes.baz
  message.optional_foreign_enum = unittest_pb2.foreign_baz
  message.optional_import_enum = unittest_import_pb2.import_baz

  message.optional_string_piece = '124'
  message.optional_cord = '125'

  #
  # repeated fields.
  #

  message.repeated_int32.append(201)
  message.repeated_int64.append(202)
  message.repeated_uint32.append(203)
  message.repeated_uint64.append(204)
  message.repeated_sint32.append(205)
  message.repeated_sint64.append(206)
  message.repeated_fixed32.append(207)
  message.repeated_fixed64.append(208)
  message.repeated_sfixed32.append(209)
  message.repeated_sfixed64.append(210)
  message.repeated_float.append(211)
  message.repeated_double.append(212)
  message.repeated_bool.append(true)
  message.repeated_string.append('215')
  message.repeated_bytes.append('216')

  message.repeatedgroup.add().a = 217
  message.repeated_nested_message.add().bb = 218
  message.repeated_foreign_message.add().c = 219
  message.repeated_import_message.add().d = 220
  message.repeated_lazy_message.add().bb = 227

  message.repeated_nested_enum.append(unittest_pb2.testalltypes.bar)
  message.repeated_foreign_enum.append(unittest_pb2.foreign_bar)
  message.repeated_import_enum.append(unittest_import_pb2.import_bar)

  message.repeated_string_piece.append('224')
  message.repeated_cord.append('225')

  # add a second one of each field.
  message.repeated_int32.append(301)
  message.repeated_int64.append(302)
  message.repeated_uint32.append(303)
  message.repeated_uint64.append(304)
  message.repeated_sint32.append(305)
  message.repeated_sint64.append(306)
  message.repeated_fixed32.append(307)
  message.repeated_fixed64.append(308)
  message.repeated_sfixed32.append(309)
  message.repeated_sfixed64.append(310)
  message.repeated_float.append(311)
  message.repeated_double.append(312)
  message.repeated_bool.append(false)
  message.repeated_string.append('315')
  message.repeated_bytes.append('316')

  message.repeatedgroup.add().a = 317
  message.repeated_nested_message.add().bb = 318
  message.repeated_foreign_message.add().c = 319
  message.repeated_import_message.add().d = 320
  message.repeated_lazy_message.add().bb = 327

  message.repeated_nested_enum.append(unittest_pb2.testalltypes.baz)
  message.repeated_foreign_enum.append(unittest_pb2.foreign_baz)
  message.repeated_import_enum.append(unittest_import_pb2.import_baz)

  message.repeated_string_piece.append('324')
  message.repeated_cord.append('325')

  #
  # fields that have defaults.
  #

  message.default_int32 = 401
  message.default_int64 = 402
  message.default_uint32 = 403
  message.default_uint64 = 404
  message.default_sint32 = 405
  message.default_sint64 = 406
  message.default_fixed32 = 407
  message.default_fixed64 = 408
  message.default_sfixed32 = 409
  message.default_sfixed64 = 410
  message.default_float = 411
  message.default_double = 412
  message.default_bool = false
  message.default_string = '415'
  message.default_bytes = '416'

  message.default_nested_enum = unittest_pb2.testalltypes.foo
  message.default_foreign_enum = unittest_pb2.foreign_foo
  message.default_import_enum = unittest_import_pb2.import_foo

  message.default_string_piece = '424'
  message.default_cord = '425'


def setallfields(message):
  setallnonlazyfields(message)
  message.optional_lazy_message.bb = 127


def setallextensions(message):
  """sets every extension in the message to a unique value.

  args:
    message: a unittest_pb2.testallextensions instance.
  """

  extensions = message.extensions
  pb2 = unittest_pb2
  import_pb2 = unittest_import_pb2

  #
  # optional fields.
  #

  extensions[pb2.optional_int32_extension] = 101
  extensions[pb2.optional_int64_extension] = 102
  extensions[pb2.optional_uint32_extension] = 103
  extensions[pb2.optional_uint64_extension] = 104
  extensions[pb2.optional_sint32_extension] = 105
  extensions[pb2.optional_sint64_extension] = 106
  extensions[pb2.optional_fixed32_extension] = 107
  extensions[pb2.optional_fixed64_extension] = 108
  extensions[pb2.optional_sfixed32_extension] = 109
  extensions[pb2.optional_sfixed64_extension] = 110
  extensions[pb2.optional_float_extension] = 111
  extensions[pb2.optional_double_extension] = 112
  extensions[pb2.optional_bool_extension] = true
  extensions[pb2.optional_string_extension] = '115'
  extensions[pb2.optional_bytes_extension] = '116'

  extensions[pb2.optionalgroup_extension].a = 117
  extensions[pb2.optional_nested_message_extension].bb = 118
  extensions[pb2.optional_foreign_message_extension].c = 119
  extensions[pb2.optional_import_message_extension].d = 120
  extensions[pb2.optional_public_import_message_extension].e = 126
  extensions[pb2.optional_lazy_message_extension].bb = 127

  extensions[pb2.optional_nested_enum_extension] = pb2.testalltypes.baz
  extensions[pb2.optional_nested_enum_extension] = pb2.testalltypes.baz
  extensions[pb2.optional_foreign_enum_extension] = pb2.foreign_baz
  extensions[pb2.optional_import_enum_extension] = import_pb2.import_baz

  extensions[pb2.optional_string_piece_extension] = '124'
  extensions[pb2.optional_cord_extension] = '125'

  #
  # repeated fields.
  #

  extensions[pb2.repeated_int32_extension].append(201)
  extensions[pb2.repeated_int64_extension].append(202)
  extensions[pb2.repeated_uint32_extension].append(203)
  extensions[pb2.repeated_uint64_extension].append(204)
  extensions[pb2.repeated_sint32_extension].append(205)
  extensions[pb2.repeated_sint64_extension].append(206)
  extensions[pb2.repeated_fixed32_extension].append(207)
  extensions[pb2.repeated_fixed64_extension].append(208)
  extensions[pb2.repeated_sfixed32_extension].append(209)
  extensions[pb2.repeated_sfixed64_extension].append(210)
  extensions[pb2.repeated_float_extension].append(211)
  extensions[pb2.repeated_double_extension].append(212)
  extensions[pb2.repeated_bool_extension].append(true)
  extensions[pb2.repeated_string_extension].append('215')
  extensions[pb2.repeated_bytes_extension].append('216')

  extensions[pb2.repeatedgroup_extension].add().a = 217
  extensions[pb2.repeated_nested_message_extension].add().bb = 218
  extensions[pb2.repeated_foreign_message_extension].add().c = 219
  extensions[pb2.repeated_import_message_extension].add().d = 220
  extensions[pb2.repeated_lazy_message_extension].add().bb = 227

  extensions[pb2.repeated_nested_enum_extension].append(pb2.testalltypes.bar)
  extensions[pb2.repeated_foreign_enum_extension].append(pb2.foreign_bar)
  extensions[pb2.repeated_import_enum_extension].append(import_pb2.import_bar)

  extensions[pb2.repeated_string_piece_extension].append('224')
  extensions[pb2.repeated_cord_extension].append('225')

  # append a second one of each field.
  extensions[pb2.repeated_int32_extension].append(301)
  extensions[pb2.repeated_int64_extension].append(302)
  extensions[pb2.repeated_uint32_extension].append(303)
  extensions[pb2.repeated_uint64_extension].append(304)
  extensions[pb2.repeated_sint32_extension].append(305)
  extensions[pb2.repeated_sint64_extension].append(306)
  extensions[pb2.repeated_fixed32_extension].append(307)
  extensions[pb2.repeated_fixed64_extension].append(308)
  extensions[pb2.repeated_sfixed32_extension].append(309)
  extensions[pb2.repeated_sfixed64_extension].append(310)
  extensions[pb2.repeated_float_extension].append(311)
  extensions[pb2.repeated_double_extension].append(312)
  extensions[pb2.repeated_bool_extension].append(false)
  extensions[pb2.repeated_string_extension].append('315')
  extensions[pb2.repeated_bytes_extension].append('316')

  extensions[pb2.repeatedgroup_extension].add().a = 317
  extensions[pb2.repeated_nested_message_extension].add().bb = 318
  extensions[pb2.repeated_foreign_message_extension].add().c = 319
  extensions[pb2.repeated_import_message_extension].add().d = 320
  extensions[pb2.repeated_lazy_message_extension].add().bb = 327

  extensions[pb2.repeated_nested_enum_extension].append(pb2.testalltypes.baz)
  extensions[pb2.repeated_foreign_enum_extension].append(pb2.foreign_baz)
  extensions[pb2.repeated_import_enum_extension].append(import_pb2.import_baz)

  extensions[pb2.repeated_string_piece_extension].append('324')
  extensions[pb2.repeated_cord_extension].append('325')

  #
  # fields with defaults.
  #

  extensions[pb2.default_int32_extension] = 401
  extensions[pb2.default_int64_extension] = 402
  extensions[pb2.default_uint32_extension] = 403
  extensions[pb2.default_uint64_extension] = 404
  extensions[pb2.default_sint32_extension] = 405
  extensions[pb2.default_sint64_extension] = 406
  extensions[pb2.default_fixed32_extension] = 407
  extensions[pb2.default_fixed64_extension] = 408
  extensions[pb2.default_sfixed32_extension] = 409
  extensions[pb2.default_sfixed64_extension] = 410
  extensions[pb2.default_float_extension] = 411
  extensions[pb2.default_double_extension] = 412
  extensions[pb2.default_bool_extension] = false
  extensions[pb2.default_string_extension] = '415'
  extensions[pb2.default_bytes_extension] = '416'

  extensions[pb2.default_nested_enum_extension] = pb2.testalltypes.foo
  extensions[pb2.default_foreign_enum_extension] = pb2.foreign_foo
  extensions[pb2.default_import_enum_extension] = import_pb2.import_foo

  extensions[pb2.default_string_piece_extension] = '424'
  extensions[pb2.default_cord_extension] = '425'


def setallfieldsandextensions(message):
  """sets every field and extension in the message to a unique value.

  args:
    message: a unittest_pb2.testallextensions message.
  """
  message.my_int = 1
  message.my_string = 'foo'
  message.my_float = 1.0
  message.extensions[unittest_pb2.my_extension_int] = 23
  message.extensions[unittest_pb2.my_extension_string] = 'bar'


def expectallfieldsandextensionsinorder(serialized):
  """ensures that serialized is the serialization we expect for a message
  filled with setallfieldsandextensions().  (specifically, ensures that the
  serialization is in canonical, tag-number order).
  """
  my_extension_int = unittest_pb2.my_extension_int
  my_extension_string = unittest_pb2.my_extension_string
  expected_strings = []
  message = unittest_pb2.testfieldorderings()
  message.my_int = 1  # field 1.
  expected_strings.append(message.serializetostring())
  message.clear()
  message.extensions[my_extension_int] = 23  # field 5.
  expected_strings.append(message.serializetostring())
  message.clear()
  message.my_string = 'foo'  # field 11.
  expected_strings.append(message.serializetostring())
  message.clear()
  message.extensions[my_extension_string] = 'bar'  # field 50.
  expected_strings.append(message.serializetostring())
  message.clear()
  message.my_float = 1.0
  expected_strings.append(message.serializetostring())
  message.clear()
  expected = ''.join(expected_strings)

  if expected != serialized:
    raise valueerror('expected %r, found %r' % (expected, serialized))


def expectallfieldsset(test_case, message):
  """check all fields for correct values have after set*fields() is called."""
  test_case.asserttrue(message.hasfield('optional_int32'))
  test_case.asserttrue(message.hasfield('optional_int64'))
  test_case.asserttrue(message.hasfield('optional_uint32'))
  test_case.asserttrue(message.hasfield('optional_uint64'))
  test_case.asserttrue(message.hasfield('optional_sint32'))
  test_case.asserttrue(message.hasfield('optional_sint64'))
  test_case.asserttrue(message.hasfield('optional_fixed32'))
  test_case.asserttrue(message.hasfield('optional_fixed64'))
  test_case.asserttrue(message.hasfield('optional_sfixed32'))
  test_case.asserttrue(message.hasfield('optional_sfixed64'))
  test_case.asserttrue(message.hasfield('optional_float'))
  test_case.asserttrue(message.hasfield('optional_double'))
  test_case.asserttrue(message.hasfield('optional_bool'))
  test_case.asserttrue(message.hasfield('optional_string'))
  test_case.asserttrue(message.hasfield('optional_bytes'))

  test_case.asserttrue(message.hasfield('optionalgroup'))
  test_case.asserttrue(message.hasfield('optional_nested_message'))
  test_case.asserttrue(message.hasfield('optional_foreign_message'))
  test_case.asserttrue(message.hasfield('optional_import_message'))

  test_case.asserttrue(message.optionalgroup.hasfield('a'))
  test_case.asserttrue(message.optional_nested_message.hasfield('bb'))
  test_case.asserttrue(message.optional_foreign_message.hasfield('c'))
  test_case.asserttrue(message.optional_import_message.hasfield('d'))

  test_case.asserttrue(message.hasfield('optional_nested_enum'))
  test_case.asserttrue(message.hasfield('optional_foreign_enum'))
  test_case.asserttrue(message.hasfield('optional_import_enum'))

  test_case.asserttrue(message.hasfield('optional_string_piece'))
  test_case.asserttrue(message.hasfield('optional_cord'))

  test_case.assertequal(101, message.optional_int32)
  test_case.assertequal(102, message.optional_int64)
  test_case.assertequal(103, message.optional_uint32)
  test_case.assertequal(104, message.optional_uint64)
  test_case.assertequal(105, message.optional_sint32)
  test_case.assertequal(106, message.optional_sint64)
  test_case.assertequal(107, message.optional_fixed32)
  test_case.assertequal(108, message.optional_fixed64)
  test_case.assertequal(109, message.optional_sfixed32)
  test_case.assertequal(110, message.optional_sfixed64)
  test_case.assertequal(111, message.optional_float)
  test_case.assertequal(112, message.optional_double)
  test_case.assertequal(true, message.optional_bool)
  test_case.assertequal('115', message.optional_string)
  test_case.assertequal('116', message.optional_bytes)

  test_case.assertequal(117, message.optionalgroup.a)
  test_case.assertequal(118, message.optional_nested_message.bb)
  test_case.assertequal(119, message.optional_foreign_message.c)
  test_case.assertequal(120, message.optional_import_message.d)
  test_case.assertequal(126, message.optional_public_import_message.e)
  test_case.assertequal(127, message.optional_lazy_message.bb)

  test_case.assertequal(unittest_pb2.testalltypes.baz,
                        message.optional_nested_enum)
  test_case.assertequal(unittest_pb2.foreign_baz,
                        message.optional_foreign_enum)
  test_case.assertequal(unittest_import_pb2.import_baz,
                        message.optional_import_enum)

  # -----------------------------------------------------------------

  test_case.assertequal(2, len(message.repeated_int32))
  test_case.assertequal(2, len(message.repeated_int64))
  test_case.assertequal(2, len(message.repeated_uint32))
  test_case.assertequal(2, len(message.repeated_uint64))
  test_case.assertequal(2, len(message.repeated_sint32))
  test_case.assertequal(2, len(message.repeated_sint64))
  test_case.assertequal(2, len(message.repeated_fixed32))
  test_case.assertequal(2, len(message.repeated_fixed64))
  test_case.assertequal(2, len(message.repeated_sfixed32))
  test_case.assertequal(2, len(message.repeated_sfixed64))
  test_case.assertequal(2, len(message.repeated_float))
  test_case.assertequal(2, len(message.repeated_double))
  test_case.assertequal(2, len(message.repeated_bool))
  test_case.assertequal(2, len(message.repeated_string))
  test_case.assertequal(2, len(message.repeated_bytes))

  test_case.assertequal(2, len(message.repeatedgroup))
  test_case.assertequal(2, len(message.repeated_nested_message))
  test_case.assertequal(2, len(message.repeated_foreign_message))
  test_case.assertequal(2, len(message.repeated_import_message))
  test_case.assertequal(2, len(message.repeated_nested_enum))
  test_case.assertequal(2, len(message.repeated_foreign_enum))
  test_case.assertequal(2, len(message.repeated_import_enum))

  test_case.assertequal(2, len(message.repeated_string_piece))
  test_case.assertequal(2, len(message.repeated_cord))

  test_case.assertequal(201, message.repeated_int32[0])
  test_case.assertequal(202, message.repeated_int64[0])
  test_case.assertequal(203, message.repeated_uint32[0])
  test_case.assertequal(204, message.repeated_uint64[0])
  test_case.assertequal(205, message.repeated_sint32[0])
  test_case.assertequal(206, message.repeated_sint64[0])
  test_case.assertequal(207, message.repeated_fixed32[0])
  test_case.assertequal(208, message.repeated_fixed64[0])
  test_case.assertequal(209, message.repeated_sfixed32[0])
  test_case.assertequal(210, message.repeated_sfixed64[0])
  test_case.assertequal(211, message.repeated_float[0])
  test_case.assertequal(212, message.repeated_double[0])
  test_case.assertequal(true, message.repeated_bool[0])
  test_case.assertequal('215', message.repeated_string[0])
  test_case.assertequal('216', message.repeated_bytes[0])

  test_case.assertequal(217, message.repeatedgroup[0].a)
  test_case.assertequal(218, message.repeated_nested_message[0].bb)
  test_case.assertequal(219, message.repeated_foreign_message[0].c)
  test_case.assertequal(220, message.repeated_import_message[0].d)
  test_case.assertequal(227, message.repeated_lazy_message[0].bb)

  test_case.assertequal(unittest_pb2.testalltypes.bar,
                        message.repeated_nested_enum[0])
  test_case.assertequal(unittest_pb2.foreign_bar,
                        message.repeated_foreign_enum[0])
  test_case.assertequal(unittest_import_pb2.import_bar,
                        message.repeated_import_enum[0])

  test_case.assertequal(301, message.repeated_int32[1])
  test_case.assertequal(302, message.repeated_int64[1])
  test_case.assertequal(303, message.repeated_uint32[1])
  test_case.assertequal(304, message.repeated_uint64[1])
  test_case.assertequal(305, message.repeated_sint32[1])
  test_case.assertequal(306, message.repeated_sint64[1])
  test_case.assertequal(307, message.repeated_fixed32[1])
  test_case.assertequal(308, message.repeated_fixed64[1])
  test_case.assertequal(309, message.repeated_sfixed32[1])
  test_case.assertequal(310, message.repeated_sfixed64[1])
  test_case.assertequal(311, message.repeated_float[1])
  test_case.assertequal(312, message.repeated_double[1])
  test_case.assertequal(false, message.repeated_bool[1])
  test_case.assertequal('315', message.repeated_string[1])
  test_case.assertequal('316', message.repeated_bytes[1])

  test_case.assertequal(317, message.repeatedgroup[1].a)
  test_case.assertequal(318, message.repeated_nested_message[1].bb)
  test_case.assertequal(319, message.repeated_foreign_message[1].c)
  test_case.assertequal(320, message.repeated_import_message[1].d)
  test_case.assertequal(327, message.repeated_lazy_message[1].bb)

  test_case.assertequal(unittest_pb2.testalltypes.baz,
                        message.repeated_nested_enum[1])
  test_case.assertequal(unittest_pb2.foreign_baz,
                        message.repeated_foreign_enum[1])
  test_case.assertequal(unittest_import_pb2.import_baz,
                        message.repeated_import_enum[1])

  # -----------------------------------------------------------------

  test_case.asserttrue(message.hasfield('default_int32'))
  test_case.asserttrue(message.hasfield('default_int64'))
  test_case.asserttrue(message.hasfield('default_uint32'))
  test_case.asserttrue(message.hasfield('default_uint64'))
  test_case.asserttrue(message.hasfield('default_sint32'))
  test_case.asserttrue(message.hasfield('default_sint64'))
  test_case.asserttrue(message.hasfield('default_fixed32'))
  test_case.asserttrue(message.hasfield('default_fixed64'))
  test_case.asserttrue(message.hasfield('default_sfixed32'))
  test_case.asserttrue(message.hasfield('default_sfixed64'))
  test_case.asserttrue(message.hasfield('default_float'))
  test_case.asserttrue(message.hasfield('default_double'))
  test_case.asserttrue(message.hasfield('default_bool'))
  test_case.asserttrue(message.hasfield('default_string'))
  test_case.asserttrue(message.hasfield('default_bytes'))

  test_case.asserttrue(message.hasfield('default_nested_enum'))
  test_case.asserttrue(message.hasfield('default_foreign_enum'))
  test_case.asserttrue(message.hasfield('default_import_enum'))

  test_case.assertequal(401, message.default_int32)
  test_case.assertequal(402, message.default_int64)
  test_case.assertequal(403, message.default_uint32)
  test_case.assertequal(404, message.default_uint64)
  test_case.assertequal(405, message.default_sint32)
  test_case.assertequal(406, message.default_sint64)
  test_case.assertequal(407, message.default_fixed32)
  test_case.assertequal(408, message.default_fixed64)
  test_case.assertequal(409, message.default_sfixed32)
  test_case.assertequal(410, message.default_sfixed64)
  test_case.assertequal(411, message.default_float)
  test_case.assertequal(412, message.default_double)
  test_case.assertequal(false, message.default_bool)
  test_case.assertequal('415', message.default_string)
  test_case.assertequal('416', message.default_bytes)

  test_case.assertequal(unittest_pb2.testalltypes.foo,
                        message.default_nested_enum)
  test_case.assertequal(unittest_pb2.foreign_foo,
                        message.default_foreign_enum)
  test_case.assertequal(unittest_import_pb2.import_foo,
                        message.default_import_enum)

def goldenfile(filename):
  """finds the given golden file and returns a file object representing it."""

  # search up the directory tree looking for the c++ protobuf source code.
  path = '.'
  while os.path.exists(path):
    if os.path.exists(os.path.join(path, 'src/google/protobuf')):
      # found it.  load the golden file from the testdata directory.
      full_path = os.path.join(path, 'src/google/protobuf/testdata', filename)
      return open(full_path, 'rb')
    path = os.path.join(path, '..')

  raise runtimeerror(
    'could not find golden files.  this test must be run from within the '
    'protobuf source package so that it can read test data files from the '
    'c++ source tree.')


def setallpackedfields(message):
  """sets every field in the message to a unique value.

  args:
    message: a unittest_pb2.testpackedtypes instance.
  """
  message.packed_int32.extend([601, 701])
  message.packed_int64.extend([602, 702])
  message.packed_uint32.extend([603, 703])
  message.packed_uint64.extend([604, 704])
  message.packed_sint32.extend([605, 705])
  message.packed_sint64.extend([606, 706])
  message.packed_fixed32.extend([607, 707])
  message.packed_fixed64.extend([608, 708])
  message.packed_sfixed32.extend([609, 709])
  message.packed_sfixed64.extend([610, 710])
  message.packed_float.extend([611.0, 711.0])
  message.packed_double.extend([612.0, 712.0])
  message.packed_bool.extend([true, false])
  message.packed_enum.extend([unittest_pb2.foreign_bar,
                              unittest_pb2.foreign_baz])


def setallpackedextensions(message):
  """sets every extension in the message to a unique value.

  args:
    message: a unittest_pb2.testpackedextensions instance.
  """
  extensions = message.extensions
  pb2 = unittest_pb2

  extensions[pb2.packed_int32_extension].extend([601, 701])
  extensions[pb2.packed_int64_extension].extend([602, 702])
  extensions[pb2.packed_uint32_extension].extend([603, 703])
  extensions[pb2.packed_uint64_extension].extend([604, 704])
  extensions[pb2.packed_sint32_extension].extend([605, 705])
  extensions[pb2.packed_sint64_extension].extend([606, 706])
  extensions[pb2.packed_fixed32_extension].extend([607, 707])
  extensions[pb2.packed_fixed64_extension].extend([608, 708])
  extensions[pb2.packed_sfixed32_extension].extend([609, 709])
  extensions[pb2.packed_sfixed64_extension].extend([610, 710])
  extensions[pb2.packed_float_extension].extend([611.0, 711.0])
  extensions[pb2.packed_double_extension].extend([612.0, 712.0])
  extensions[pb2.packed_bool_extension].extend([true, false])
  extensions[pb2.packed_enum_extension].extend([unittest_pb2.foreign_bar,
                                                unittest_pb2.foreign_baz])


def setallunpackedfields(message):
  """sets every field in the message to a unique value.

  args:
    message: a unittest_pb2.testunpackedtypes instance.
  """
  message.unpacked_int32.extend([601, 701])
  message.unpacked_int64.extend([602, 702])
  message.unpacked_uint32.extend([603, 703])
  message.unpacked_uint64.extend([604, 704])
  message.unpacked_sint32.extend([605, 705])
  message.unpacked_sint64.extend([606, 706])
  message.unpacked_fixed32.extend([607, 707])
  message.unpacked_fixed64.extend([608, 708])
  message.unpacked_sfixed32.extend([609, 709])
  message.unpacked_sfixed64.extend([610, 710])
  message.unpacked_float.extend([611.0, 711.0])
  message.unpacked_double.extend([612.0, 712.0])
  message.unpacked_bool.extend([true, false])
  message.unpacked_enum.extend([unittest_pb2.foreign_bar,
                                unittest_pb2.foreign_baz])
