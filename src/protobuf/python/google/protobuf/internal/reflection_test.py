#! /usr/bin/python
# -*- coding: utf-8 -*-
#
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

"""unittest for reflection.py, which also indirectly tests the output of the
pure-python protocol compiler.
"""

__author__ = 'robinson@google.com (will robinson)'

import gc
import operator
import struct

import unittest
from google.protobuf import unittest_import_pb2
from google.protobuf import unittest_mset_pb2
from google.protobuf import unittest_pb2
from google.protobuf import descriptor_pb2
from google.protobuf import descriptor
from google.protobuf import message
from google.protobuf import reflection
from google.protobuf.internal import api_implementation
from google.protobuf.internal import more_extensions_pb2
from google.protobuf.internal import more_messages_pb2
from google.protobuf.internal import wire_format
from google.protobuf.internal import test_util
from google.protobuf.internal import decoder


class _minidecoder(object):
  """decodes a stream of values from a string.

  once upon a time we actually had a class called decoder.decoder.  then we
  got rid of it during a redesign that made decoding much, much faster overall.
  but a couple tests in this file used it to check that the serialized form of
  a message was correct.  so, this class implements just the methods that were
  used by said tests, so that we don't have to rewrite the tests.
  """

  def __init__(self, bytes):
    self._bytes = bytes
    self._pos = 0

  def readvarint(self):
    result, self._pos = decoder._decodevarint(self._bytes, self._pos)
    return result

  readint32 = readvarint
  readint64 = readvarint
  readuint32 = readvarint
  readuint64 = readvarint

  def readsint64(self):
    return wire_format.zigzagdecode(self.readvarint())

  readsint32 = readsint64

  def readfieldnumberandwiretype(self):
    return wire_format.unpacktag(self.readvarint())

  def readfloat(self):
    result = struct.unpack("<f", self._bytes[self._pos:self._pos+4])[0]
    self._pos += 4
    return result

  def readdouble(self):
    result = struct.unpack("<d", self._bytes[self._pos:self._pos+8])[0]
    self._pos += 8
    return result

  def endofstream(self):
    return self._pos == len(self._bytes)


class reflectiontest(unittest.testcase):

  def assertlistsequal(self, values, others):
    self.assertequal(len(values), len(others))
    for i in range(len(values)):
      self.assertequal(values[i], others[i])

  def testscalarconstructor(self):
    # constructor with only scalar types should succeed.
    proto = unittest_pb2.testalltypes(
        optional_int32=24,
        optional_double=54.321,
        optional_string='optional_string')

    self.assertequal(24, proto.optional_int32)
    self.assertequal(54.321, proto.optional_double)
    self.assertequal('optional_string', proto.optional_string)

  def testrepeatedscalarconstructor(self):
    # constructor with only repeated scalar types should succeed.
    proto = unittest_pb2.testalltypes(
        repeated_int32=[1, 2, 3, 4],
        repeated_double=[1.23, 54.321],
        repeated_bool=[true, false, false],
        repeated_string=["optional_string"])

    self.assertequals([1, 2, 3, 4], list(proto.repeated_int32))
    self.assertequals([1.23, 54.321], list(proto.repeated_double))
    self.assertequals([true, false, false], list(proto.repeated_bool))
    self.assertequals(["optional_string"], list(proto.repeated_string))

  def testrepeatedcompositeconstructor(self):
    # constructor with only repeated composite types should succeed.
    proto = unittest_pb2.testalltypes(
        repeated_nested_message=[
            unittest_pb2.testalltypes.nestedmessage(
                bb=unittest_pb2.testalltypes.foo),
            unittest_pb2.testalltypes.nestedmessage(
                bb=unittest_pb2.testalltypes.bar)],
        repeated_foreign_message=[
            unittest_pb2.foreignmessage(c=-43),
            unittest_pb2.foreignmessage(c=45324),
            unittest_pb2.foreignmessage(c=12)],
        repeatedgroup=[
            unittest_pb2.testalltypes.repeatedgroup(),
            unittest_pb2.testalltypes.repeatedgroup(a=1),
            unittest_pb2.testalltypes.repeatedgroup(a=2)])

    self.assertequals(
        [unittest_pb2.testalltypes.nestedmessage(
            bb=unittest_pb2.testalltypes.foo),
         unittest_pb2.testalltypes.nestedmessage(
             bb=unittest_pb2.testalltypes.bar)],
        list(proto.repeated_nested_message))
    self.assertequals(
        [unittest_pb2.foreignmessage(c=-43),
         unittest_pb2.foreignmessage(c=45324),
         unittest_pb2.foreignmessage(c=12)],
        list(proto.repeated_foreign_message))
    self.assertequals(
        [unittest_pb2.testalltypes.repeatedgroup(),
         unittest_pb2.testalltypes.repeatedgroup(a=1),
         unittest_pb2.testalltypes.repeatedgroup(a=2)],
        list(proto.repeatedgroup))

  def testmixedconstructor(self):
    # constructor with only mixed types should succeed.
    proto = unittest_pb2.testalltypes(
        optional_int32=24,
        optional_string='optional_string',
        repeated_double=[1.23, 54.321],
        repeated_bool=[true, false, false],
        repeated_nested_message=[
            unittest_pb2.testalltypes.nestedmessage(
                bb=unittest_pb2.testalltypes.foo),
            unittest_pb2.testalltypes.nestedmessage(
                bb=unittest_pb2.testalltypes.bar)],
        repeated_foreign_message=[
            unittest_pb2.foreignmessage(c=-43),
            unittest_pb2.foreignmessage(c=45324),
            unittest_pb2.foreignmessage(c=12)])

    self.assertequal(24, proto.optional_int32)
    self.assertequal('optional_string', proto.optional_string)
    self.assertequals([1.23, 54.321], list(proto.repeated_double))
    self.assertequals([true, false, false], list(proto.repeated_bool))
    self.assertequals(
        [unittest_pb2.testalltypes.nestedmessage(
            bb=unittest_pb2.testalltypes.foo),
         unittest_pb2.testalltypes.nestedmessage(
             bb=unittest_pb2.testalltypes.bar)],
        list(proto.repeated_nested_message))
    self.assertequals(
        [unittest_pb2.foreignmessage(c=-43),
         unittest_pb2.foreignmessage(c=45324),
         unittest_pb2.foreignmessage(c=12)],
        list(proto.repeated_foreign_message))

  def testconstructortypeerror(self):
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, optional_int32="foo")
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, optional_string=1234)
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, optional_nested_message=1234)
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_int32=1234)
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_int32=["foo"])
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_string=1234)
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_string=[1234])
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_nested_message=1234)
    self.assertraises(
        typeerror, unittest_pb2.testalltypes, repeated_nested_message=[1234])

  def testconstructorinvalidatescachedbytesize(self):
    message = unittest_pb2.testalltypes(optional_int32 = 12)
    self.assertequals(2, message.bytesize())

    message = unittest_pb2.testalltypes(
        optional_nested_message = unittest_pb2.testalltypes.nestedmessage())
    self.assertequals(3, message.bytesize())

    message = unittest_pb2.testalltypes(repeated_int32 = [12])
    self.assertequals(3, message.bytesize())

    message = unittest_pb2.testalltypes(
        repeated_nested_message = [unittest_pb2.testalltypes.nestedmessage()])
    self.assertequals(3, message.bytesize())

  def testsimplehasbits(self):
    # test a scalar.
    proto = unittest_pb2.testalltypes()
    self.asserttrue(not proto.hasfield('optional_int32'))
    self.assertequal(0, proto.optional_int32)
    # hasfield() shouldn't be true if all we've done is
    # read the default value.
    self.asserttrue(not proto.hasfield('optional_int32'))
    proto.optional_int32 = 1
    # setting a value however *should* set the "has" bit.
    self.asserttrue(proto.hasfield('optional_int32'))
    proto.clearfield('optional_int32')
    # and clearing that value should unset the "has" bit.
    self.asserttrue(not proto.hasfield('optional_int32'))

  def testhasbitswithsinglynestedscalar(self):
    # helper used to test foreign messages and groups.
    #
    # composite_field_name should be the name of a non-repeated
    # composite (i.e., foreign or group) field in testalltypes,
    # and scalar_field_name should be the name of an integer-valued
    # scalar field within that composite.
    #
    # i never thought i'd miss c++ macros and templates so much. :(
    # this helper is semantically just:
    #
    #   assert proto.composite_field.scalar_field == 0
    #   assert not proto.composite_field.hasfield('scalar_field')
    #   assert not proto.hasfield('composite_field')
    #
    #   proto.composite_field.scalar_field = 10
    #   old_composite_field = proto.composite_field
    #
    #   assert proto.composite_field.scalar_field == 10
    #   assert proto.composite_field.hasfield('scalar_field')
    #   assert proto.hasfield('composite_field')
    #
    #   proto.clearfield('composite_field')
    #
    #   assert not proto.composite_field.hasfield('scalar_field')
    #   assert not proto.hasfield('composite_field')
    #   assert proto.composite_field.scalar_field == 0
    #
    #   # now ensure that clearfield('composite_field') disconnected
    #   # the old field object from the object tree...
    #   assert old_composite_field is not proto.composite_field
    #   old_composite_field.scalar_field = 20
    #   assert not proto.composite_field.hasfield('scalar_field')
    #   assert not proto.hasfield('composite_field')
    def testcompositehasbits(composite_field_name, scalar_field_name):
      proto = unittest_pb2.testalltypes()
      # first, check that we can get the scalar value, and see that it's the
      # default (0), but that proto.hasfield('omposite') and
      # proto.composite.hasfield('scalar') will still return false.
      composite_field = getattr(proto, composite_field_name)
      original_scalar_value = getattr(composite_field, scalar_field_name)
      self.assertequal(0, original_scalar_value)
      # assert that the composite object does not "have" the scalar.
      self.asserttrue(not composite_field.hasfield(scalar_field_name))
      # assert that proto does not "have" the composite field.
      self.asserttrue(not proto.hasfield(composite_field_name))

      # now set the scalar within the composite field.  ensure that the setting
      # is reflected, and that proto.hasfield('composite') and
      # proto.composite.hasfield('scalar') now both return true.
      new_val = 20
      setattr(composite_field, scalar_field_name, new_val)
      self.assertequal(new_val, getattr(composite_field, scalar_field_name))
      # hold on to a reference to the current composite_field object.
      old_composite_field = composite_field
      # assert that the has methods now return true.
      self.asserttrue(composite_field.hasfield(scalar_field_name))
      self.asserttrue(proto.hasfield(composite_field_name))

      # now call the clear method...
      proto.clearfield(composite_field_name)

      # ...and ensure that the "has" bits are all back to false...
      composite_field = getattr(proto, composite_field_name)
      self.asserttrue(not composite_field.hasfield(scalar_field_name))
      self.asserttrue(not proto.hasfield(composite_field_name))
      # ...and ensure that the scalar field has returned to its default.
      self.assertequal(0, getattr(composite_field, scalar_field_name))

      self.asserttrue(old_composite_field is not composite_field)
      setattr(old_composite_field, scalar_field_name, new_val)
      self.asserttrue(not composite_field.hasfield(scalar_field_name))
      self.asserttrue(not proto.hasfield(composite_field_name))
      self.assertequal(0, getattr(composite_field, scalar_field_name))

    # test simple, single-level nesting when we set a scalar.
    testcompositehasbits('optionalgroup', 'a')
    testcompositehasbits('optional_nested_message', 'bb')
    testcompositehasbits('optional_foreign_message', 'c')
    testcompositehasbits('optional_import_message', 'd')

  def testreferencestonestedmessage(self):
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    del proto
    # a previous version had a bug where this would raise an exception when
    # hitting a now-dead weak reference.
    nested.bb = 23

  def testdisconnectingnestedmessagebeforesettingfield(self):
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    proto.clearfield('optional_nested_message')  # should disconnect from parent
    self.asserttrue(nested is not proto.optional_nested_message)
    nested.bb = 23
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    self.assertequal(0, proto.optional_nested_message.bb)

  def testgetdefaultmessageafterdisconnectingdefaultmessage(self):
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    proto.clearfield('optional_nested_message')
    del proto
    del nested
    # force a garbage collect so that the underlying cmessages are freed along
    # with the messages they point to. this is to make sure we're not deleting
    # default message instances.
    gc.collect()
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message

  def testdisconnectingnestedmessageaftersettingfield(self):
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    nested.bb = 5
    self.asserttrue(proto.hasfield('optional_nested_message'))
    proto.clearfield('optional_nested_message')  # should disconnect from parent
    self.assertequal(5, nested.bb)
    self.assertequal(0, proto.optional_nested_message.bb)
    self.asserttrue(nested is not proto.optional_nested_message)
    nested.bb = 23
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    self.assertequal(0, proto.optional_nested_message.bb)

  def testdisconnectingnestedmessagebeforegettingfield(self):
    proto = unittest_pb2.testalltypes()
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    proto.clearfield('optional_nested_message')
    self.asserttrue(not proto.hasfield('optional_nested_message'))

  def testdisconnectingnestedmessageaftermerge(self):
    # this test exercises the code path that does not use releasemessage().
    # the underlying fear is that if we use releasemessage() incorrectly,
    # we will have memory leaks.  it's hard to check that that doesn't happen,
    # but at least we can exercise that code path to make sure it works.
    proto1 = unittest_pb2.testalltypes()
    proto2 = unittest_pb2.testalltypes()
    proto2.optional_nested_message.bb = 5
    proto1.mergefrom(proto2)
    self.asserttrue(proto1.hasfield('optional_nested_message'))
    proto1.clearfield('optional_nested_message')
    self.asserttrue(not proto1.hasfield('optional_nested_message'))

  def testdisconnectinglazynestedmessage(self):
    # this test exercises releasing a nested message that is lazy. this test
    # only exercises real code in the c++ implementation as python does not
    # support lazy parsing, but the current c++ implementation results in
    # memory corruption and a crash.
    if api_implementation.type() != 'python':
      return
    proto = unittest_pb2.testalltypes()
    proto.optional_lazy_message.bb = 5
    proto.clearfield('optional_lazy_message')
    del proto
    gc.collect()

  def testhasbitswhenmodifyingrepeatedfields(self):
    # test nesting when we add an element to a repeated field in a submessage.
    proto = unittest_pb2.testnestedmessagehasbits()
    proto.optional_nested_message.nestedmessage_repeated_int32.append(5)
    self.assertequal(
        [5], proto.optional_nested_message.nestedmessage_repeated_int32)
    self.asserttrue(proto.hasfield('optional_nested_message'))

    # do the same test, but with a repeated composite field within the
    # submessage.
    proto.clearfield('optional_nested_message')
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    proto.optional_nested_message.nestedmessage_repeated_foreignmessage.add()
    self.asserttrue(proto.hasfield('optional_nested_message'))

  def testhasbitsformanylevelsofnesting(self):
    # test nesting many levels deep.
    recursive_proto = unittest_pb2.testmutualrecursiona()
    self.asserttrue(not recursive_proto.hasfield('bb'))
    self.assertequal(0, recursive_proto.bb.a.bb.a.bb.optional_int32)
    self.asserttrue(not recursive_proto.hasfield('bb'))
    recursive_proto.bb.a.bb.a.bb.optional_int32 = 5
    self.assertequal(5, recursive_proto.bb.a.bb.a.bb.optional_int32)
    self.asserttrue(recursive_proto.hasfield('bb'))
    self.asserttrue(recursive_proto.bb.hasfield('a'))
    self.asserttrue(recursive_proto.bb.a.hasfield('bb'))
    self.asserttrue(recursive_proto.bb.a.bb.hasfield('a'))
    self.asserttrue(recursive_proto.bb.a.bb.a.hasfield('bb'))
    self.asserttrue(not recursive_proto.bb.a.bb.a.bb.hasfield('a'))
    self.asserttrue(recursive_proto.bb.a.bb.a.bb.hasfield('optional_int32'))

  def testsingularlistfields(self):
    proto = unittest_pb2.testalltypes()
    proto.optional_fixed32 = 1
    proto.optional_int32 = 5
    proto.optional_string = 'foo'
    # access sub-message but don't set it yet.
    nested_message = proto.optional_nested_message
    self.assertequal(
      [ (proto.descriptor.fields_by_name['optional_int32'  ], 5),
        (proto.descriptor.fields_by_name['optional_fixed32'], 1),
        (proto.descriptor.fields_by_name['optional_string' ], 'foo') ],
      proto.listfields())

    proto.optional_nested_message.bb = 123
    self.assertequal(
      [ (proto.descriptor.fields_by_name['optional_int32'  ], 5),
        (proto.descriptor.fields_by_name['optional_fixed32'], 1),
        (proto.descriptor.fields_by_name['optional_string' ], 'foo'),
        (proto.descriptor.fields_by_name['optional_nested_message' ],
             nested_message) ],
      proto.listfields())

  def testrepeatedlistfields(self):
    proto = unittest_pb2.testalltypes()
    proto.repeated_fixed32.append(1)
    proto.repeated_int32.append(5)
    proto.repeated_int32.append(11)
    proto.repeated_string.extend(['foo', 'bar'])
    proto.repeated_string.extend([])
    proto.repeated_string.append('baz')
    proto.repeated_string.extend(str(x) for x in xrange(2))
    proto.optional_int32 = 21
    proto.repeated_bool  # access but don't set anything; should not be listed.
    self.assertequal(
      [ (proto.descriptor.fields_by_name['optional_int32'  ], 21),
        (proto.descriptor.fields_by_name['repeated_int32'  ], [5, 11]),
        (proto.descriptor.fields_by_name['repeated_fixed32'], [1]),
        (proto.descriptor.fields_by_name['repeated_string' ],
          ['foo', 'bar', 'baz', '0', '1']) ],
      proto.listfields())

  def testsingularlistextensions(self):
    proto = unittest_pb2.testallextensions()
    proto.extensions[unittest_pb2.optional_fixed32_extension] = 1
    proto.extensions[unittest_pb2.optional_int32_extension  ] = 5
    proto.extensions[unittest_pb2.optional_string_extension ] = 'foo'
    self.assertequal(
      [ (unittest_pb2.optional_int32_extension  , 5),
        (unittest_pb2.optional_fixed32_extension, 1),
        (unittest_pb2.optional_string_extension , 'foo') ],
      proto.listfields())

  def testrepeatedlistextensions(self):
    proto = unittest_pb2.testallextensions()
    proto.extensions[unittest_pb2.repeated_fixed32_extension].append(1)
    proto.extensions[unittest_pb2.repeated_int32_extension  ].append(5)
    proto.extensions[unittest_pb2.repeated_int32_extension  ].append(11)
    proto.extensions[unittest_pb2.repeated_string_extension ].append('foo')
    proto.extensions[unittest_pb2.repeated_string_extension ].append('bar')
    proto.extensions[unittest_pb2.repeated_string_extension ].append('baz')
    proto.extensions[unittest_pb2.optional_int32_extension  ] = 21
    self.assertequal(
      [ (unittest_pb2.optional_int32_extension  , 21),
        (unittest_pb2.repeated_int32_extension  , [5, 11]),
        (unittest_pb2.repeated_fixed32_extension, [1]),
        (unittest_pb2.repeated_string_extension , ['foo', 'bar', 'baz']) ],
      proto.listfields())

  def testlistfieldsandextensions(self):
    proto = unittest_pb2.testfieldorderings()
    test_util.setallfieldsandextensions(proto)
    unittest_pb2.my_extension_int
    self.assertequal(
      [ (proto.descriptor.fields_by_name['my_int'   ], 1),
        (unittest_pb2.my_extension_int               , 23),
        (proto.descriptor.fields_by_name['my_string'], 'foo'),
        (unittest_pb2.my_extension_string            , 'bar'),
        (proto.descriptor.fields_by_name['my_float' ], 1.0) ],
      proto.listfields())

  def testdefaultvalues(self):
    proto = unittest_pb2.testalltypes()
    self.assertequal(0, proto.optional_int32)
    self.assertequal(0, proto.optional_int64)
    self.assertequal(0, proto.optional_uint32)
    self.assertequal(0, proto.optional_uint64)
    self.assertequal(0, proto.optional_sint32)
    self.assertequal(0, proto.optional_sint64)
    self.assertequal(0, proto.optional_fixed32)
    self.assertequal(0, proto.optional_fixed64)
    self.assertequal(0, proto.optional_sfixed32)
    self.assertequal(0, proto.optional_sfixed64)
    self.assertequal(0.0, proto.optional_float)
    self.assertequal(0.0, proto.optional_double)
    self.assertequal(false, proto.optional_bool)
    self.assertequal('', proto.optional_string)
    self.assertequal('', proto.optional_bytes)

    self.assertequal(41, proto.default_int32)
    self.assertequal(42, proto.default_int64)
    self.assertequal(43, proto.default_uint32)
    self.assertequal(44, proto.default_uint64)
    self.assertequal(-45, proto.default_sint32)
    self.assertequal(46, proto.default_sint64)
    self.assertequal(47, proto.default_fixed32)
    self.assertequal(48, proto.default_fixed64)
    self.assertequal(49, proto.default_sfixed32)
    self.assertequal(-50, proto.default_sfixed64)
    self.assertequal(51.5, proto.default_float)
    self.assertequal(52e3, proto.default_double)
    self.assertequal(true, proto.default_bool)
    self.assertequal('hello', proto.default_string)
    self.assertequal('world', proto.default_bytes)
    self.assertequal(unittest_pb2.testalltypes.bar, proto.default_nested_enum)
    self.assertequal(unittest_pb2.foreign_bar, proto.default_foreign_enum)
    self.assertequal(unittest_import_pb2.import_bar,
                     proto.default_import_enum)

    proto = unittest_pb2.testextremedefaultvalues()
    self.assertequal(u'\u1234', proto.utf8_string)

  def testhasfieldwithunknownfieldname(self):
    proto = unittest_pb2.testalltypes()
    self.assertraises(valueerror, proto.hasfield, 'nonexistent_field')

  def testclearfieldwithunknownfieldname(self):
    proto = unittest_pb2.testalltypes()
    self.assertraises(valueerror, proto.clearfield, 'nonexistent_field')

  def testdisallowedassignments(self):
    # it's illegal to assign values directly to repeated fields
    # or to nonrepeated composite fields.  ensure that this fails.
    proto = unittest_pb2.testalltypes()
    # repeated fields.
    self.assertraises(attributeerror, setattr, proto, 'repeated_int32', 10)
    # lists shouldn't work, either.
    self.assertraises(attributeerror, setattr, proto, 'repeated_int32', [10])
    # composite fields.
    self.assertraises(attributeerror, setattr, proto,
                      'optional_nested_message', 23)
    # assignment to a repeated nested message field without specifying
    # the index in the array of nested messages.
    self.assertraises(attributeerror, setattr, proto.repeated_nested_message,
                      'bb', 34)
    # assignment to an attribute of a repeated field.
    self.assertraises(attributeerror, setattr, proto.repeated_float,
                      'some_attribute', 34)
    # proto.nonexistent_field = 23 should fail as well.
    self.assertraises(attributeerror, setattr, proto, 'nonexistent_field', 23)

  def testsinglescalartypesafety(self):
    proto = unittest_pb2.testalltypes()
    self.assertraises(typeerror, setattr, proto, 'optional_int32', 1.1)
    self.assertraises(typeerror, setattr, proto, 'optional_int32', 'foo')
    self.assertraises(typeerror, setattr, proto, 'optional_string', 10)
    self.assertraises(typeerror, setattr, proto, 'optional_bytes', 10)

  def testsinglescalarboundschecking(self):
    def testminandmaxintegers(field_name, expected_min, expected_max):
      pb = unittest_pb2.testalltypes()
      setattr(pb, field_name, expected_min)
      self.assertequal(expected_min, getattr(pb, field_name))
      setattr(pb, field_name, expected_max)
      self.assertequal(expected_max, getattr(pb, field_name))
      self.assertraises(valueerror, setattr, pb, field_name, expected_min - 1)
      self.assertraises(valueerror, setattr, pb, field_name, expected_max + 1)

    testminandmaxintegers('optional_int32', -(1 << 31), (1 << 31) - 1)
    testminandmaxintegers('optional_uint32', 0, 0xffffffff)
    testminandmaxintegers('optional_int64', -(1 << 63), (1 << 63) - 1)
    testminandmaxintegers('optional_uint64', 0, 0xffffffffffffffff)

    pb = unittest_pb2.testalltypes()
    pb.optional_nested_enum = 1
    self.assertequal(1, pb.optional_nested_enum)

    # invalid enum values.
    pb.optional_nested_enum = 0
    self.assertequal(0, pb.optional_nested_enum)

    bytes_size_before = pb.bytesize()

    pb.optional_nested_enum = 4
    self.assertequal(4, pb.optional_nested_enum)

    pb.optional_nested_enum = 0
    self.assertequal(0, pb.optional_nested_enum)

    # make sure that setting the same enum field doesn't just add unknown
    # fields (but overwrites them).
    self.assertequal(bytes_size_before, pb.bytesize())

    # is the invalid value preserved after serialization?
    serialized = pb.serializetostring()
    pb2 = unittest_pb2.testalltypes()
    pb2.parsefromstring(serialized)
    self.assertequal(0, pb2.optional_nested_enum)
    self.assertequal(pb, pb2)

  def testrepeatedscalartypesafety(self):
    proto = unittest_pb2.testalltypes()
    self.assertraises(typeerror, proto.repeated_int32.append, 1.1)
    self.assertraises(typeerror, proto.repeated_int32.append, 'foo')
    self.assertraises(typeerror, proto.repeated_string, 10)
    self.assertraises(typeerror, proto.repeated_bytes, 10)

    proto.repeated_int32.append(10)
    proto.repeated_int32[0] = 23
    self.assertraises(indexerror, proto.repeated_int32.__setitem__, 500, 23)
    self.assertraises(typeerror, proto.repeated_int32.__setitem__, 0, 'abc')

    # repeated enums tests.
    #proto.repeated_nested_enum.append(0)

  def testsinglescalargettersandsetters(self):
    proto = unittest_pb2.testalltypes()
    self.assertequal(0, proto.optional_int32)
    proto.optional_int32 = 1
    self.assertequal(1, proto.optional_int32)

    proto.optional_uint64 = 0xffffffffffff
    self.assertequal(0xffffffffffff, proto.optional_uint64)
    proto.optional_uint64 = 0xffffffffffffffff
    self.assertequal(0xffffffffffffffff, proto.optional_uint64)
    # todo(robinson): test all other scalar field types.

  def testsinglescalarclearfield(self):
    proto = unittest_pb2.testalltypes()
    # should be allowed to clear something that's not there (a no-op).
    proto.clearfield('optional_int32')
    proto.optional_int32 = 1
    self.asserttrue(proto.hasfield('optional_int32'))
    proto.clearfield('optional_int32')
    self.assertequal(0, proto.optional_int32)
    self.asserttrue(not proto.hasfield('optional_int32'))
    # todo(robinson): test all other scalar field types.

  def testenums(self):
    proto = unittest_pb2.testalltypes()
    self.assertequal(1, proto.foo)
    self.assertequal(1, unittest_pb2.testalltypes.foo)
    self.assertequal(2, proto.bar)
    self.assertequal(2, unittest_pb2.testalltypes.bar)
    self.assertequal(3, proto.baz)
    self.assertequal(3, unittest_pb2.testalltypes.baz)

  def testenum_name(self):
    self.assertequal('foreign_foo',
                     unittest_pb2.foreignenum.name(unittest_pb2.foreign_foo))
    self.assertequal('foreign_bar',
                     unittest_pb2.foreignenum.name(unittest_pb2.foreign_bar))
    self.assertequal('foreign_baz',
                     unittest_pb2.foreignenum.name(unittest_pb2.foreign_baz))
    self.assertraises(valueerror,
                      unittest_pb2.foreignenum.name, 11312)

    proto = unittest_pb2.testalltypes()
    self.assertequal('foo',
                     proto.nestedenum.name(proto.foo))
    self.assertequal('foo',
                     unittest_pb2.testalltypes.nestedenum.name(proto.foo))
    self.assertequal('bar',
                     proto.nestedenum.name(proto.bar))
    self.assertequal('bar',
                     unittest_pb2.testalltypes.nestedenum.name(proto.bar))
    self.assertequal('baz',
                     proto.nestedenum.name(proto.baz))
    self.assertequal('baz',
                     unittest_pb2.testalltypes.nestedenum.name(proto.baz))
    self.assertraises(valueerror,
                      proto.nestedenum.name, 11312)
    self.assertraises(valueerror,
                      unittest_pb2.testalltypes.nestedenum.name, 11312)

  def testenum_value(self):
    self.assertequal(unittest_pb2.foreign_foo,
                     unittest_pb2.foreignenum.value('foreign_foo'))
    self.assertequal(unittest_pb2.foreign_bar,
                     unittest_pb2.foreignenum.value('foreign_bar'))
    self.assertequal(unittest_pb2.foreign_baz,
                     unittest_pb2.foreignenum.value('foreign_baz'))
    self.assertraises(valueerror,
                      unittest_pb2.foreignenum.value, 'fo')

    proto = unittest_pb2.testalltypes()
    self.assertequal(proto.foo,
                     proto.nestedenum.value('foo'))
    self.assertequal(proto.foo,
                     unittest_pb2.testalltypes.nestedenum.value('foo'))
    self.assertequal(proto.bar,
                     proto.nestedenum.value('bar'))
    self.assertequal(proto.bar,
                     unittest_pb2.testalltypes.nestedenum.value('bar'))
    self.assertequal(proto.baz,
                     proto.nestedenum.value('baz'))
    self.assertequal(proto.baz,
                     unittest_pb2.testalltypes.nestedenum.value('baz'))
    self.assertraises(valueerror,
                      proto.nestedenum.value, 'foo')
    self.assertraises(valueerror,
                      unittest_pb2.testalltypes.nestedenum.value, 'foo')

  def testenum_keysandvalues(self):
    self.assertequal(['foreign_foo', 'foreign_bar', 'foreign_baz'],
                     unittest_pb2.foreignenum.keys())
    self.assertequal([4, 5, 6],
                     unittest_pb2.foreignenum.values())
    self.assertequal([('foreign_foo', 4), ('foreign_bar', 5),
                      ('foreign_baz', 6)],
                     unittest_pb2.foreignenum.items())

    proto = unittest_pb2.testalltypes()
    self.assertequal(['foo', 'bar', 'baz'], proto.nestedenum.keys())
    self.assertequal([1, 2, 3], proto.nestedenum.values())
    self.assertequal([('foo', 1), ('bar', 2), ('baz', 3)],
                     proto.nestedenum.items())

  def testrepeatedscalars(self):
    proto = unittest_pb2.testalltypes()

    self.asserttrue(not proto.repeated_int32)
    self.assertequal(0, len(proto.repeated_int32))
    proto.repeated_int32.append(5)
    proto.repeated_int32.append(10)
    proto.repeated_int32.append(15)
    self.asserttrue(proto.repeated_int32)
    self.assertequal(3, len(proto.repeated_int32))

    self.assertequal([5, 10, 15], proto.repeated_int32)

    # test single retrieval.
    self.assertequal(5, proto.repeated_int32[0])
    self.assertequal(15, proto.repeated_int32[-1])
    # test out-of-bounds indices.
    self.assertraises(indexerror, proto.repeated_int32.__getitem__, 1234)
    self.assertraises(indexerror, proto.repeated_int32.__getitem__, -1234)
    # test incorrect types passed to __getitem__.
    self.assertraises(typeerror, proto.repeated_int32.__getitem__, 'foo')
    self.assertraises(typeerror, proto.repeated_int32.__getitem__, none)

    # test single assignment.
    proto.repeated_int32[1] = 20
    self.assertequal([5, 20, 15], proto.repeated_int32)

    # test insertion.
    proto.repeated_int32.insert(1, 25)
    self.assertequal([5, 25, 20, 15], proto.repeated_int32)

    # test slice retrieval.
    proto.repeated_int32.append(30)
    self.assertequal([25, 20, 15], proto.repeated_int32[1:4])
    self.assertequal([5, 25, 20, 15, 30], proto.repeated_int32[:])

    # test slice assignment with an iterator
    proto.repeated_int32[1:4] = (i for i in xrange(3))
    self.assertequal([5, 0, 1, 2, 30], proto.repeated_int32)

    # test slice assignment.
    proto.repeated_int32[1:4] = [35, 40, 45]
    self.assertequal([5, 35, 40, 45, 30], proto.repeated_int32)

    # test that we can use the field as an iterator.
    result = []
    for i in proto.repeated_int32:
      result.append(i)
    self.assertequal([5, 35, 40, 45, 30], result)

    # test single deletion.
    del proto.repeated_int32[2]
    self.assertequal([5, 35, 45, 30], proto.repeated_int32)

    # test slice deletion.
    del proto.repeated_int32[2:]
    self.assertequal([5, 35], proto.repeated_int32)

    # test extending.
    proto.repeated_int32.extend([3, 13])
    self.assertequal([5, 35, 3, 13], proto.repeated_int32)

    # test clearing.
    proto.clearfield('repeated_int32')
    self.asserttrue(not proto.repeated_int32)
    self.assertequal(0, len(proto.repeated_int32))

    proto.repeated_int32.append(1)
    self.assertequal(1, proto.repeated_int32[-1])
    # test assignment to a negative index.
    proto.repeated_int32[-1] = 2
    self.assertequal(2, proto.repeated_int32[-1])

    # test deletion at negative indices.
    proto.repeated_int32[:] = [0, 1, 2, 3]
    del proto.repeated_int32[-1]
    self.assertequal([0, 1, 2], proto.repeated_int32)

    del proto.repeated_int32[-2]
    self.assertequal([0, 2], proto.repeated_int32)

    self.assertraises(indexerror, proto.repeated_int32.__delitem__, -3)
    self.assertraises(indexerror, proto.repeated_int32.__delitem__, 300)

    del proto.repeated_int32[-2:-1]
    self.assertequal([2], proto.repeated_int32)

    del proto.repeated_int32[100:10000]
    self.assertequal([2], proto.repeated_int32)

  def testrepeatedscalarsremove(self):
    proto = unittest_pb2.testalltypes()

    self.asserttrue(not proto.repeated_int32)
    self.assertequal(0, len(proto.repeated_int32))
    proto.repeated_int32.append(5)
    proto.repeated_int32.append(10)
    proto.repeated_int32.append(5)
    proto.repeated_int32.append(5)

    self.assertequal(4, len(proto.repeated_int32))
    proto.repeated_int32.remove(5)
    self.assertequal(3, len(proto.repeated_int32))
    self.assertequal(10, proto.repeated_int32[0])
    self.assertequal(5, proto.repeated_int32[1])
    self.assertequal(5, proto.repeated_int32[2])

    proto.repeated_int32.remove(5)
    self.assertequal(2, len(proto.repeated_int32))
    self.assertequal(10, proto.repeated_int32[0])
    self.assertequal(5, proto.repeated_int32[1])

    proto.repeated_int32.remove(10)
    self.assertequal(1, len(proto.repeated_int32))
    self.assertequal(5, proto.repeated_int32[0])

    # remove a non-existent element.
    self.assertraises(valueerror, proto.repeated_int32.remove, 123)

  def testrepeatedcomposites(self):
    proto = unittest_pb2.testalltypes()
    self.asserttrue(not proto.repeated_nested_message)
    self.assertequal(0, len(proto.repeated_nested_message))
    m0 = proto.repeated_nested_message.add()
    m1 = proto.repeated_nested_message.add()
    self.asserttrue(proto.repeated_nested_message)
    self.assertequal(2, len(proto.repeated_nested_message))
    self.assertlistsequal([m0, m1], proto.repeated_nested_message)
    self.asserttrue(isinstance(m0, unittest_pb2.testalltypes.nestedmessage))

    # test out-of-bounds indices.
    self.assertraises(indexerror, proto.repeated_nested_message.__getitem__,
                      1234)
    self.assertraises(indexerror, proto.repeated_nested_message.__getitem__,
                      -1234)

    # test incorrect types passed to __getitem__.
    self.assertraises(typeerror, proto.repeated_nested_message.__getitem__,
                      'foo')
    self.assertraises(typeerror, proto.repeated_nested_message.__getitem__,
                      none)

    # test slice retrieval.
    m2 = proto.repeated_nested_message.add()
    m3 = proto.repeated_nested_message.add()
    m4 = proto.repeated_nested_message.add()
    self.assertlistsequal(
        [m1, m2, m3], proto.repeated_nested_message[1:4])
    self.assertlistsequal(
        [m0, m1, m2, m3, m4], proto.repeated_nested_message[:])
    self.assertlistsequal(
        [m0, m1], proto.repeated_nested_message[:2])
    self.assertlistsequal(
        [m2, m3, m4], proto.repeated_nested_message[2:])
    self.assertequal(
        m0, proto.repeated_nested_message[0])
    self.assertlistsequal(
        [m0], proto.repeated_nested_message[:1])

    # test that we can use the field as an iterator.
    result = []
    for i in proto.repeated_nested_message:
      result.append(i)
    self.assertlistsequal([m0, m1, m2, m3, m4], result)

    # test single deletion.
    del proto.repeated_nested_message[2]
    self.assertlistsequal([m0, m1, m3, m4], proto.repeated_nested_message)

    # test slice deletion.
    del proto.repeated_nested_message[2:]
    self.assertlistsequal([m0, m1], proto.repeated_nested_message)

    # test extending.
    n1 = unittest_pb2.testalltypes.nestedmessage(bb=1)
    n2 = unittest_pb2.testalltypes.nestedmessage(bb=2)
    proto.repeated_nested_message.extend([n1,n2])
    self.assertequal(4, len(proto.repeated_nested_message))
    self.assertequal(n1, proto.repeated_nested_message[2])
    self.assertequal(n2, proto.repeated_nested_message[3])

    # test clearing.
    proto.clearfield('repeated_nested_message')
    self.asserttrue(not proto.repeated_nested_message)
    self.assertequal(0, len(proto.repeated_nested_message))

    # test constructing an element while adding it.
    proto.repeated_nested_message.add(bb=23)
    self.assertequal(1, len(proto.repeated_nested_message))
    self.assertequal(23, proto.repeated_nested_message[0].bb)

  def testrepeatedcompositeremove(self):
    proto = unittest_pb2.testalltypes()

    self.assertequal(0, len(proto.repeated_nested_message))
    m0 = proto.repeated_nested_message.add()
    # need to set some differentiating variable so m0 != m1 != m2:
    m0.bb = len(proto.repeated_nested_message)
    m1 = proto.repeated_nested_message.add()
    m1.bb = len(proto.repeated_nested_message)
    self.asserttrue(m0 != m1)
    m2 = proto.repeated_nested_message.add()
    m2.bb = len(proto.repeated_nested_message)
    self.assertlistsequal([m0, m1, m2], proto.repeated_nested_message)

    self.assertequal(3, len(proto.repeated_nested_message))
    proto.repeated_nested_message.remove(m0)
    self.assertequal(2, len(proto.repeated_nested_message))
    self.assertequal(m1, proto.repeated_nested_message[0])
    self.assertequal(m2, proto.repeated_nested_message[1])

    # removing m0 again or removing none should raise error
    self.assertraises(valueerror, proto.repeated_nested_message.remove, m0)
    self.assertraises(valueerror, proto.repeated_nested_message.remove, none)
    self.assertequal(2, len(proto.repeated_nested_message))

    proto.repeated_nested_message.remove(m2)
    self.assertequal(1, len(proto.repeated_nested_message))
    self.assertequal(m1, proto.repeated_nested_message[0])

  def testhandwrittenreflection(self):
    # hand written extensions are only supported by the pure-python
    # implementation of the api.
    if api_implementation.type() != 'python':
      return

    fielddescriptor = descriptor.fielddescriptor
    foo_field_descriptor = fielddescriptor(
        name='foo_field', full_name='myproto.foo_field',
        index=0, number=1, type=fielddescriptor.type_int64,
        cpp_type=fielddescriptor.cpptype_int64,
        label=fielddescriptor.label_optional, default_value=0,
        containing_type=none, message_type=none, enum_type=none,
        is_extension=false, extension_scope=none,
        options=descriptor_pb2.fieldoptions())
    mydescriptor = descriptor.descriptor(
        name='myproto', full_name='myproto', filename='ignored',
        containing_type=none, nested_types=[], enum_types=[],
        fields=[foo_field_descriptor], extensions=[],
        options=descriptor_pb2.messageoptions())
    class myprotoclass(message.message):
      descriptor = mydescriptor
      __metaclass__ = reflection.generatedprotocolmessagetype
    myproto_instance = myprotoclass()
    self.assertequal(0, myproto_instance.foo_field)
    self.asserttrue(not myproto_instance.hasfield('foo_field'))
    myproto_instance.foo_field = 23
    self.assertequal(23, myproto_instance.foo_field)
    self.asserttrue(myproto_instance.hasfield('foo_field'))

  def testdescriptorprotosupport(self):
    # hand written descriptors/reflection are only supported by the pure-python
    # implementation of the api.
    if api_implementation.type() != 'python':
      return

    def adddescriptorfield(proto, field_name, field_type):
      adddescriptorfield.field_index += 1
      new_field = proto.field.add()
      new_field.name = field_name
      new_field.type = field_type
      new_field.number = adddescriptorfield.field_index
      new_field.label = descriptor_pb2.fielddescriptorproto.label_optional

    adddescriptorfield.field_index = 0

    desc_proto = descriptor_pb2.descriptorproto()
    desc_proto.name = 'car'
    fdp = descriptor_pb2.fielddescriptorproto
    adddescriptorfield(desc_proto, 'name', fdp.type_string)
    adddescriptorfield(desc_proto, 'year', fdp.type_int64)
    adddescriptorfield(desc_proto, 'automatic', fdp.type_bool)
    adddescriptorfield(desc_proto, 'price', fdp.type_double)
    # add a repeated field
    adddescriptorfield.field_index += 1
    new_field = desc_proto.field.add()
    new_field.name = 'owners'
    new_field.type = fdp.type_string
    new_field.number = adddescriptorfield.field_index
    new_field.label = descriptor_pb2.fielddescriptorproto.label_repeated

    desc = descriptor.makedescriptor(desc_proto)
    self.asserttrue(desc.fields_by_name.has_key('name'))
    self.asserttrue(desc.fields_by_name.has_key('year'))
    self.asserttrue(desc.fields_by_name.has_key('automatic'))
    self.asserttrue(desc.fields_by_name.has_key('price'))
    self.asserttrue(desc.fields_by_name.has_key('owners'))

    class carmessage(message.message):
      __metaclass__ = reflection.generatedprotocolmessagetype
      descriptor = desc

    prius = carmessage()
    prius.name = 'prius'
    prius.year = 2010
    prius.automatic = true
    prius.price = 25134.75
    prius.owners.extend(['bob', 'susan'])

    serialized_prius = prius.serializetostring()
    new_prius = reflection.parsemessage(desc, serialized_prius)
    self.asserttrue(new_prius is not prius)
    self.assertequal(prius, new_prius)

    # these are unnecessary assuming message equality works as advertised but
    # explicitly check to be safe since we're mucking about in metaclass foo
    self.assertequal(prius.name, new_prius.name)
    self.assertequal(prius.year, new_prius.year)
    self.assertequal(prius.automatic, new_prius.automatic)
    self.assertequal(prius.price, new_prius.price)
    self.assertequal(prius.owners, new_prius.owners)

  def testtoplevelextensionsforoptionalscalar(self):
    extendee_proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.optional_int32_extension
    self.asserttrue(not extendee_proto.hasextension(extension))
    self.assertequal(0, extendee_proto.extensions[extension])
    # as with normal scalar fields, just doing a read doesn't actually set the
    # "has" bit.
    self.asserttrue(not extendee_proto.hasextension(extension))
    # actually set the thing.
    extendee_proto.extensions[extension] = 23
    self.assertequal(23, extendee_proto.extensions[extension])
    self.asserttrue(extendee_proto.hasextension(extension))
    # ensure that clearing works as well.
    extendee_proto.clearextension(extension)
    self.assertequal(0, extendee_proto.extensions[extension])
    self.asserttrue(not extendee_proto.hasextension(extension))

  def testtoplevelextensionsforrepeatedscalar(self):
    extendee_proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.repeated_string_extension
    self.assertequal(0, len(extendee_proto.extensions[extension]))
    extendee_proto.extensions[extension].append('foo')
    self.assertequal(['foo'], extendee_proto.extensions[extension])
    string_list = extendee_proto.extensions[extension]
    extendee_proto.clearextension(extension)
    self.assertequal(0, len(extendee_proto.extensions[extension]))
    self.asserttrue(string_list is not extendee_proto.extensions[extension])
    # shouldn't be allowed to do extensions[extension] = 'a'
    self.assertraises(typeerror, operator.setitem, extendee_proto.extensions,
                      extension, 'a')

  def testtoplevelextensionsforoptionalmessage(self):
    extendee_proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.optional_foreign_message_extension
    self.asserttrue(not extendee_proto.hasextension(extension))
    self.assertequal(0, extendee_proto.extensions[extension].c)
    # as with normal (non-extension) fields, merely reading from the
    # thing shouldn't set the "has" bit.
    self.asserttrue(not extendee_proto.hasextension(extension))
    extendee_proto.extensions[extension].c = 23
    self.assertequal(23, extendee_proto.extensions[extension].c)
    self.asserttrue(extendee_proto.hasextension(extension))
    # save a reference here.
    foreign_message = extendee_proto.extensions[extension]
    extendee_proto.clearextension(extension)
    self.asserttrue(foreign_message is not extendee_proto.extensions[extension])
    # setting a field on foreign_message now shouldn't set
    # any "has" bits on extendee_proto.
    foreign_message.c = 42
    self.assertequal(42, foreign_message.c)
    self.asserttrue(foreign_message.hasfield('c'))
    self.asserttrue(not extendee_proto.hasextension(extension))
    # shouldn't be allowed to do extensions[extension] = 'a'
    self.assertraises(typeerror, operator.setitem, extendee_proto.extensions,
                      extension, 'a')

  def testtoplevelextensionsforrepeatedmessage(self):
    extendee_proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.repeatedgroup_extension
    self.assertequal(0, len(extendee_proto.extensions[extension]))
    group = extendee_proto.extensions[extension].add()
    group.a = 23
    self.assertequal(23, extendee_proto.extensions[extension][0].a)
    group.a = 42
    self.assertequal(42, extendee_proto.extensions[extension][0].a)
    group_list = extendee_proto.extensions[extension]
    extendee_proto.clearextension(extension)
    self.assertequal(0, len(extendee_proto.extensions[extension]))
    self.asserttrue(group_list is not extendee_proto.extensions[extension])
    # shouldn't be allowed to do extensions[extension] = 'a'
    self.assertraises(typeerror, operator.setitem, extendee_proto.extensions,
                      extension, 'a')

  def testnestedextensions(self):
    extendee_proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.testrequired.single

    # we just test the non-repeated case.
    self.asserttrue(not extendee_proto.hasextension(extension))
    required = extendee_proto.extensions[extension]
    self.assertequal(0, required.a)
    self.asserttrue(not extendee_proto.hasextension(extension))
    required.a = 23
    self.assertequal(23, extendee_proto.extensions[extension].a)
    self.asserttrue(extendee_proto.hasextension(extension))
    extendee_proto.clearextension(extension)
    self.asserttrue(required is not extendee_proto.extensions[extension])
    self.asserttrue(not extendee_proto.hasextension(extension))

  # if message a directly contains message b, and
  # a.hasfield('b') is currently false, then mutating any
  # extension in b should change a.hasfield('b') to true
  # (and so on up the object tree).
  def testhasbitsforancestorsofextendedmessage(self):
    # optional scalar extension.
    toplevel = more_extensions_pb2.toplevelmessage()
    self.asserttrue(not toplevel.hasfield('submessage'))
    self.assertequal(0, toplevel.submessage.extensions[
        more_extensions_pb2.optional_int_extension])
    self.asserttrue(not toplevel.hasfield('submessage'))
    toplevel.submessage.extensions[
        more_extensions_pb2.optional_int_extension] = 23
    self.assertequal(23, toplevel.submessage.extensions[
        more_extensions_pb2.optional_int_extension])
    self.asserttrue(toplevel.hasfield('submessage'))

    # repeated scalar extension.
    toplevel = more_extensions_pb2.toplevelmessage()
    self.asserttrue(not toplevel.hasfield('submessage'))
    self.assertequal([], toplevel.submessage.extensions[
        more_extensions_pb2.repeated_int_extension])
    self.asserttrue(not toplevel.hasfield('submessage'))
    toplevel.submessage.extensions[
        more_extensions_pb2.repeated_int_extension].append(23)
    self.assertequal([23], toplevel.submessage.extensions[
        more_extensions_pb2.repeated_int_extension])
    self.asserttrue(toplevel.hasfield('submessage'))

    # optional message extension.
    toplevel = more_extensions_pb2.toplevelmessage()
    self.asserttrue(not toplevel.hasfield('submessage'))
    self.assertequal(0, toplevel.submessage.extensions[
        more_extensions_pb2.optional_message_extension].foreign_message_int)
    self.asserttrue(not toplevel.hasfield('submessage'))
    toplevel.submessage.extensions[
        more_extensions_pb2.optional_message_extension].foreign_message_int = 23
    self.assertequal(23, toplevel.submessage.extensions[
        more_extensions_pb2.optional_message_extension].foreign_message_int)
    self.asserttrue(toplevel.hasfield('submessage'))

    # repeated message extension.
    toplevel = more_extensions_pb2.toplevelmessage()
    self.asserttrue(not toplevel.hasfield('submessage'))
    self.assertequal(0, len(toplevel.submessage.extensions[
        more_extensions_pb2.repeated_message_extension]))
    self.asserttrue(not toplevel.hasfield('submessage'))
    foreign = toplevel.submessage.extensions[
        more_extensions_pb2.repeated_message_extension].add()
    self.assertequal(foreign, toplevel.submessage.extensions[
        more_extensions_pb2.repeated_message_extension][0])
    self.asserttrue(toplevel.hasfield('submessage'))

  def testdisconnectionafterclearingemptymessage(self):
    toplevel = more_extensions_pb2.toplevelmessage()
    extendee_proto = toplevel.submessage
    extension = more_extensions_pb2.optional_message_extension
    extension_proto = extendee_proto.extensions[extension]
    extendee_proto.clearextension(extension)
    extension_proto.foreign_message_int = 23

    self.asserttrue(extension_proto is not extendee_proto.extensions[extension])

  def testextensionfailuremodes(self):
    extendee_proto = unittest_pb2.testallextensions()

    # try non-extension-handle arguments to hasextension,
    # clearextension(), and extensions[]...
    self.assertraises(keyerror, extendee_proto.hasextension, 1234)
    self.assertraises(keyerror, extendee_proto.clearextension, 1234)
    self.assertraises(keyerror, extendee_proto.extensions.__getitem__, 1234)
    self.assertraises(keyerror, extendee_proto.extensions.__setitem__, 1234, 5)

    # try something that *is* an extension handle, just not for
    # this message...
    unknown_handle = more_extensions_pb2.optional_int_extension
    self.assertraises(keyerror, extendee_proto.hasextension,
                      unknown_handle)
    self.assertraises(keyerror, extendee_proto.clearextension,
                      unknown_handle)
    self.assertraises(keyerror, extendee_proto.extensions.__getitem__,
                      unknown_handle)
    self.assertraises(keyerror, extendee_proto.extensions.__setitem__,
                      unknown_handle, 5)

    # try call hasextension() with a valid handle, but for a
    # *repeated* field.  (just as with non-extension repeated
    # fields, has*() isn't supported for extension repeated fields).
    self.assertraises(keyerror, extendee_proto.hasextension,
                      unittest_pb2.repeated_string_extension)

  def teststaticparsefrom(self):
    proto1 = unittest_pb2.testalltypes()
    test_util.setallfields(proto1)

    string1 = proto1.serializetostring()
    proto2 = unittest_pb2.testalltypes.fromstring(string1)

    # messages should be equal.
    self.assertequal(proto2, proto1)

  def testmergefromsingularfield(self):
    # test merge with just a singular field.
    proto1 = unittest_pb2.testalltypes()
    proto1.optional_int32 = 1

    proto2 = unittest_pb2.testalltypes()
    # this shouldn't get overwritten.
    proto2.optional_string = 'value'

    proto2.mergefrom(proto1)
    self.assertequal(1, proto2.optional_int32)
    self.assertequal('value', proto2.optional_string)

  def testmergefromrepeatedfield(self):
    # test merge with just a repeated field.
    proto1 = unittest_pb2.testalltypes()
    proto1.repeated_int32.append(1)
    proto1.repeated_int32.append(2)

    proto2 = unittest_pb2.testalltypes()
    proto2.repeated_int32.append(0)
    proto2.mergefrom(proto1)

    self.assertequal(0, proto2.repeated_int32[0])
    self.assertequal(1, proto2.repeated_int32[1])
    self.assertequal(2, proto2.repeated_int32[2])

  def testmergefromoptionalgroup(self):
    # test merge with an optional group.
    proto1 = unittest_pb2.testalltypes()
    proto1.optionalgroup.a = 12
    proto2 = unittest_pb2.testalltypes()
    proto2.mergefrom(proto1)
    self.assertequal(12, proto2.optionalgroup.a)

  def testmergefromrepeatednestedmessage(self):
    # test merge with a repeated nested message.
    proto1 = unittest_pb2.testalltypes()
    m = proto1.repeated_nested_message.add()
    m.bb = 123
    m = proto1.repeated_nested_message.add()
    m.bb = 321

    proto2 = unittest_pb2.testalltypes()
    m = proto2.repeated_nested_message.add()
    m.bb = 999
    proto2.mergefrom(proto1)
    self.assertequal(999, proto2.repeated_nested_message[0].bb)
    self.assertequal(123, proto2.repeated_nested_message[1].bb)
    self.assertequal(321, proto2.repeated_nested_message[2].bb)

    proto3 = unittest_pb2.testalltypes()
    proto3.repeated_nested_message.mergefrom(proto2.repeated_nested_message)
    self.assertequal(999, proto3.repeated_nested_message[0].bb)
    self.assertequal(123, proto3.repeated_nested_message[1].bb)
    self.assertequal(321, proto3.repeated_nested_message[2].bb)

  def testmergefromallfields(self):
    # with all fields set.
    proto1 = unittest_pb2.testalltypes()
    test_util.setallfields(proto1)
    proto2 = unittest_pb2.testalltypes()
    proto2.mergefrom(proto1)

    # messages should be equal.
    self.assertequal(proto2, proto1)

    # serialized string should be equal too.
    string1 = proto1.serializetostring()
    string2 = proto2.serializetostring()
    self.assertequal(string1, string2)

  def testmergefromextensionssingular(self):
    proto1 = unittest_pb2.testallextensions()
    proto1.extensions[unittest_pb2.optional_int32_extension] = 1

    proto2 = unittest_pb2.testallextensions()
    proto2.mergefrom(proto1)
    self.assertequal(
        1, proto2.extensions[unittest_pb2.optional_int32_extension])

  def testmergefromextensionsrepeated(self):
    proto1 = unittest_pb2.testallextensions()
    proto1.extensions[unittest_pb2.repeated_int32_extension].append(1)
    proto1.extensions[unittest_pb2.repeated_int32_extension].append(2)

    proto2 = unittest_pb2.testallextensions()
    proto2.extensions[unittest_pb2.repeated_int32_extension].append(0)
    proto2.mergefrom(proto1)
    self.assertequal(
        3, len(proto2.extensions[unittest_pb2.repeated_int32_extension]))
    self.assertequal(
        0, proto2.extensions[unittest_pb2.repeated_int32_extension][0])
    self.assertequal(
        1, proto2.extensions[unittest_pb2.repeated_int32_extension][1])
    self.assertequal(
        2, proto2.extensions[unittest_pb2.repeated_int32_extension][2])

  def testmergefromextensionsnestedmessage(self):
    proto1 = unittest_pb2.testallextensions()
    ext1 = proto1.extensions[
        unittest_pb2.repeated_nested_message_extension]
    m = ext1.add()
    m.bb = 222
    m = ext1.add()
    m.bb = 333

    proto2 = unittest_pb2.testallextensions()
    ext2 = proto2.extensions[
        unittest_pb2.repeated_nested_message_extension]
    m = ext2.add()
    m.bb = 111

    proto2.mergefrom(proto1)
    ext2 = proto2.extensions[
        unittest_pb2.repeated_nested_message_extension]
    self.assertequal(3, len(ext2))
    self.assertequal(111, ext2[0].bb)
    self.assertequal(222, ext2[1].bb)
    self.assertequal(333, ext2[2].bb)

  def testmergefrombug(self):
    message1 = unittest_pb2.testalltypes()
    message2 = unittest_pb2.testalltypes()

    # cause optional_nested_message to be instantiated within message1, even
    # though it is not considered to be "present".
    message1.optional_nested_message
    self.assertfalse(message1.hasfield('optional_nested_message'))

    # merge into message2.  this should not instantiate the field is message2.
    message2.mergefrom(message1)
    self.assertfalse(message2.hasfield('optional_nested_message'))

  def testcopyfromsingularfield(self):
    # test copy with just a singular field.
    proto1 = unittest_pb2.testalltypes()
    proto1.optional_int32 = 1
    proto1.optional_string = 'important-text'

    proto2 = unittest_pb2.testalltypes()
    proto2.optional_string = 'value'

    proto2.copyfrom(proto1)
    self.assertequal(1, proto2.optional_int32)
    self.assertequal('important-text', proto2.optional_string)

  def testcopyfromrepeatedfield(self):
    # test copy with a repeated field.
    proto1 = unittest_pb2.testalltypes()
    proto1.repeated_int32.append(1)
    proto1.repeated_int32.append(2)

    proto2 = unittest_pb2.testalltypes()
    proto2.repeated_int32.append(0)
    proto2.copyfrom(proto1)

    self.assertequal(1, proto2.repeated_int32[0])
    self.assertequal(2, proto2.repeated_int32[1])

  def testcopyfromallfields(self):
    # with all fields set.
    proto1 = unittest_pb2.testalltypes()
    test_util.setallfields(proto1)
    proto2 = unittest_pb2.testalltypes()
    proto2.copyfrom(proto1)

    # messages should be equal.
    self.assertequal(proto2, proto1)

    # serialized string should be equal too.
    string1 = proto1.serializetostring()
    string2 = proto2.serializetostring()
    self.assertequal(string1, string2)

  def testcopyfromself(self):
    proto1 = unittest_pb2.testalltypes()
    proto1.repeated_int32.append(1)
    proto1.optional_int32 = 2
    proto1.optional_string = 'important-text'

    proto1.copyfrom(proto1)
    self.assertequal(1, proto1.repeated_int32[0])
    self.assertequal(2, proto1.optional_int32)
    self.assertequal('important-text', proto1.optional_string)

  def testcopyfrombadtype(self):
    # the python implementation doesn't raise an exception in this
    # case. in theory it should.
    if api_implementation.type() == 'python':
      return
    proto1 = unittest_pb2.testalltypes()
    proto2 = unittest_pb2.testallextensions()
    self.assertraises(typeerror, proto1.copyfrom, proto2)

  def testclear(self):
    proto = unittest_pb2.testalltypes()
    # c++ implementation does not support lazy fields right now so leave it
    # out for now.
    if api_implementation.type() == 'python':
      test_util.setallfields(proto)
    else:
      test_util.setallnonlazyfields(proto)
    # clear the message.
    proto.clear()
    self.assertequals(proto.bytesize(), 0)
    empty_proto = unittest_pb2.testalltypes()
    self.assertequals(proto, empty_proto)

    # test if extensions which were set are cleared.
    proto = unittest_pb2.testallextensions()
    test_util.setallextensions(proto)
    # clear the message.
    proto.clear()
    self.assertequals(proto.bytesize(), 0)
    empty_proto = unittest_pb2.testallextensions()
    self.assertequals(proto, empty_proto)

  def testdisconnectingbeforeclear(self):
    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    proto.clear()
    self.asserttrue(nested is not proto.optional_nested_message)
    nested.bb = 23
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    self.assertequal(0, proto.optional_nested_message.bb)

    proto = unittest_pb2.testalltypes()
    nested = proto.optional_nested_message
    nested.bb = 5
    foreign = proto.optional_foreign_message
    foreign.c = 6

    proto.clear()
    self.asserttrue(nested is not proto.optional_nested_message)
    self.asserttrue(foreign is not proto.optional_foreign_message)
    self.assertequal(5, nested.bb)
    self.assertequal(6, foreign.c)
    nested.bb = 15
    foreign.c = 16
    self.asserttrue(not proto.hasfield('optional_nested_message'))
    self.assertequal(0, proto.optional_nested_message.bb)
    self.asserttrue(not proto.hasfield('optional_foreign_message'))
    self.assertequal(0, proto.optional_foreign_message.c)

  def assertinitialized(self, proto):
    self.asserttrue(proto.isinitialized())
    # neither method should raise an exception.
    proto.serializetostring()
    proto.serializepartialtostring()

  def assertnotinitialized(self, proto):
    self.assertfalse(proto.isinitialized())
    self.assertraises(message.encodeerror, proto.serializetostring)
    # "partial" serialization doesn't care if message is uninitialized.
    proto.serializepartialtostring()

  def testisinitialized(self):
    # trivial cases - all optional fields and extensions.
    proto = unittest_pb2.testalltypes()
    self.assertinitialized(proto)
    proto = unittest_pb2.testallextensions()
    self.assertinitialized(proto)

    # the case of uninitialized required fields.
    proto = unittest_pb2.testrequired()
    self.assertnotinitialized(proto)
    proto.a = proto.b = proto.c = 2
    self.assertinitialized(proto)

    # the case of uninitialized submessage.
    proto = unittest_pb2.testrequiredforeign()
    self.assertinitialized(proto)
    proto.optional_message.a = 1
    self.assertnotinitialized(proto)
    proto.optional_message.b = 0
    proto.optional_message.c = 0
    self.assertinitialized(proto)

    # uninitialized repeated submessage.
    message1 = proto.repeated_message.add()
    self.assertnotinitialized(proto)
    message1.a = message1.b = message1.c = 0
    self.assertinitialized(proto)

    # uninitialized repeated group in an extension.
    proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.testrequired.multi
    message1 = proto.extensions[extension].add()
    message2 = proto.extensions[extension].add()
    self.assertnotinitialized(proto)
    message1.a = 1
    message1.b = 1
    message1.c = 1
    self.assertnotinitialized(proto)
    message2.a = 2
    message2.b = 2
    message2.c = 2
    self.assertinitialized(proto)

    # uninitialized nonrepeated message in an extension.
    proto = unittest_pb2.testallextensions()
    extension = unittest_pb2.testrequired.single
    proto.extensions[extension].a = 1
    self.assertnotinitialized(proto)
    proto.extensions[extension].b = 2
    proto.extensions[extension].c = 3
    self.assertinitialized(proto)

    # try passing an errors list.
    errors = []
    proto = unittest_pb2.testrequired()
    self.assertfalse(proto.isinitialized(errors))
    self.assertequal(errors, ['a', 'b', 'c'])

  def teststringutf8encoding(self):
    proto = unittest_pb2.testalltypes()

    # assignment of a unicode object to a field of type 'bytes' is not allowed.
    self.assertraises(typeerror,
                      setattr, proto, 'optional_bytes', u'unicode object')

    # check that the default value is of python's 'unicode' type.
    self.assertequal(type(proto.optional_string), unicode)

    proto.optional_string = unicode('testing')
    self.assertequal(proto.optional_string, str('testing'))

    # assign a value of type 'str' which can be encoded in utf-8.
    proto.optional_string = str('testing')
    self.assertequal(proto.optional_string, unicode('testing'))

    if api_implementation.type() == 'python':
      # values of type 'str' are also accepted as long as they can be
      # encoded in utf-8.
      self.assertequal(type(proto.optional_string), str)

    # try to assign a 'str' value which contains bytes that aren't 7-bit ascii.
    self.assertraises(valueerror,
                      setattr, proto, 'optional_string', str('a\x80a'))
    # assign a 'str' object which contains a utf-8 encoded string.
    self.assertraises(valueerror,
                      setattr, proto, 'optional_string', '孝械褋褌')
    # no exception thrown.
    proto.optional_string = 'abc'

  def teststringutf8serialization(self):
    proto = unittest_mset_pb2.testmessageset()
    extension_message = unittest_mset_pb2.testmessagesetextension2
    extension = extension_message.message_set_extension

    test_utf8 = u'孝械褋褌'
    test_utf8_bytes = test_utf8.encode('utf-8')

    # 'test' in another language, using utf-8 charset.
    proto.extensions[extension].str = test_utf8

    # serialize using the messageset wire format (this is specified in the
    # .proto file).
    serialized = proto.serializetostring()

    # check byte size.
    self.assertequal(proto.bytesize(), len(serialized))

    raw = unittest_mset_pb2.rawmessageset()
    raw.mergefromstring(serialized)

    message2 = unittest_mset_pb2.testmessagesetextension2()

    self.assertequal(1, len(raw.item))
    # check that the type_id is the same as the tag id in the .proto file.
    self.assertequal(raw.item[0].type_id, 1547769)

    # check the actual bytes on the wire.
    self.asserttrue(
        raw.item[0].message.endswith(test_utf8_bytes))
    message2.mergefromstring(raw.item[0].message)

    self.assertequal(type(message2.str), unicode)
    self.assertequal(message2.str, test_utf8)

    # the pure python api throws an exception on mergefromstring(),
    # if any of the string fields of the message can't be utf-8 decoded.
    # the c++ implementation of the api has no way to check that on
    # mergefromstring and thus has no way to throw the exception.
    #
    # the pure python api always returns objects of type 'unicode' (utf-8
    # encoded), or 'str' (in 7 bit ascii).
    bytes = raw.item[0].message.replace(
        test_utf8_bytes, len(test_utf8_bytes) * '\xff')

    unicode_decode_failed = false
    try:
      message2.mergefromstring(bytes)
    except unicodedecodeerror as e:
      unicode_decode_failed = true
    string_field = message2.str
    self.asserttrue(unicode_decode_failed or type(string_field) == str)

  def testemptynestedmessage(self):
    proto = unittest_pb2.testalltypes()
    proto.optional_nested_message.mergefrom(
        unittest_pb2.testalltypes.nestedmessage())
    self.asserttrue(proto.hasfield('optional_nested_message'))

    proto = unittest_pb2.testalltypes()
    proto.optional_nested_message.copyfrom(
        unittest_pb2.testalltypes.nestedmessage())
    self.asserttrue(proto.hasfield('optional_nested_message'))

    proto = unittest_pb2.testalltypes()
    proto.optional_nested_message.mergefromstring('')
    self.asserttrue(proto.hasfield('optional_nested_message'))

    proto = unittest_pb2.testalltypes()
    proto.optional_nested_message.parsefromstring('')
    self.asserttrue(proto.hasfield('optional_nested_message'))

    serialized = proto.serializetostring()
    proto2 = unittest_pb2.testalltypes()
    proto2.mergefromstring(serialized)
    self.asserttrue(proto2.hasfield('optional_nested_message'))

  def testsetinparent(self):
    proto = unittest_pb2.testalltypes()
    self.assertfalse(proto.hasfield('optionalgroup'))
    proto.optionalgroup.setinparent()
    self.asserttrue(proto.hasfield('optionalgroup'))


#  since we had so many tests for protocol buffer equality, we broke these out
#  into separate testcase classes.


class testalltypesequalitytest(unittest.testcase):

  def setup(self):
    self.first_proto = unittest_pb2.testalltypes()
    self.second_proto = unittest_pb2.testalltypes()

  def testnothashable(self):
    self.assertraises(typeerror, hash, self.first_proto)

  def testselfequality(self):
    self.assertequal(self.first_proto, self.first_proto)

  def testemptyprotosequal(self):
    self.assertequal(self.first_proto, self.second_proto)


class fullprotosequalitytest(unittest.testcase):

  """equality tests using completely-full protos as a starting point."""

  def setup(self):
    self.first_proto = unittest_pb2.testalltypes()
    self.second_proto = unittest_pb2.testalltypes()
    test_util.setallfields(self.first_proto)
    test_util.setallfields(self.second_proto)

  def testnothashable(self):
    self.assertraises(typeerror, hash, self.first_proto)

  def testnonenotequal(self):
    self.assertnotequal(self.first_proto, none)
    self.assertnotequal(none, self.second_proto)

  def testnotequaltoothermessage(self):
    third_proto = unittest_pb2.testrequired()
    self.assertnotequal(self.first_proto, third_proto)
    self.assertnotequal(third_proto, self.second_proto)

  def testallfieldsfilledequality(self):
    self.assertequal(self.first_proto, self.second_proto)

  def testnonrepeatedscalar(self):
    # nonrepeated scalar field change should cause inequality.
    self.first_proto.optional_int32 += 1
    self.assertnotequal(self.first_proto, self.second_proto)
    # ...as should clearing a field.
    self.first_proto.clearfield('optional_int32')
    self.assertnotequal(self.first_proto, self.second_proto)

  def testnonrepeatedcomposite(self):
    # change a nonrepeated composite field.
    self.first_proto.optional_nested_message.bb += 1
    self.assertnotequal(self.first_proto, self.second_proto)
    self.first_proto.optional_nested_message.bb -= 1
    self.assertequal(self.first_proto, self.second_proto)
    # clear a field in the nested message.
    self.first_proto.optional_nested_message.clearfield('bb')
    self.assertnotequal(self.first_proto, self.second_proto)
    self.first_proto.optional_nested_message.bb = (
        self.second_proto.optional_nested_message.bb)
    self.assertequal(self.first_proto, self.second_proto)
    # remove the nested message entirely.
    self.first_proto.clearfield('optional_nested_message')
    self.assertnotequal(self.first_proto, self.second_proto)

  def testrepeatedscalar(self):
    # change a repeated scalar field.
    self.first_proto.repeated_int32.append(5)
    self.assertnotequal(self.first_proto, self.second_proto)
    self.first_proto.clearfield('repeated_int32')
    self.assertnotequal(self.first_proto, self.second_proto)

  def testrepeatedcomposite(self):
    # change value within a repeated composite field.
    self.first_proto.repeated_nested_message[0].bb += 1
    self.assertnotequal(self.first_proto, self.second_proto)
    self.first_proto.repeated_nested_message[0].bb -= 1
    self.assertequal(self.first_proto, self.second_proto)
    # add a value to a repeated composite field.
    self.first_proto.repeated_nested_message.add()
    self.assertnotequal(self.first_proto, self.second_proto)
    self.second_proto.repeated_nested_message.add()
    self.assertequal(self.first_proto, self.second_proto)

  def testnonrepeatedscalarhasbits(self):
    # ensure that we test "has" bits as well as value for
    # nonrepeated scalar field.
    self.first_proto.clearfield('optional_int32')
    self.second_proto.optional_int32 = 0
    self.assertnotequal(self.first_proto, self.second_proto)

  def testnonrepeatedcompositehasbits(self):
    # ensure that we test "has" bits as well as value for
    # nonrepeated composite field.
    self.first_proto.clearfield('optional_nested_message')
    self.second_proto.optional_nested_message.clearfield('bb')
    self.assertnotequal(self.first_proto, self.second_proto)
    self.first_proto.optional_nested_message.bb = 0
    self.first_proto.optional_nested_message.clearfield('bb')
    self.assertequal(self.first_proto, self.second_proto)


class extensionequalitytest(unittest.testcase):

  def testextensionequality(self):
    first_proto = unittest_pb2.testallextensions()
    second_proto = unittest_pb2.testallextensions()
    self.assertequal(first_proto, second_proto)
    test_util.setallextensions(first_proto)
    self.assertnotequal(first_proto, second_proto)
    test_util.setallextensions(second_proto)
    self.assertequal(first_proto, second_proto)

    # ensure that we check value equality.
    first_proto.extensions[unittest_pb2.optional_int32_extension] += 1
    self.assertnotequal(first_proto, second_proto)
    first_proto.extensions[unittest_pb2.optional_int32_extension] -= 1
    self.assertequal(first_proto, second_proto)

    # ensure that we also look at "has" bits.
    first_proto.clearextension(unittest_pb2.optional_int32_extension)
    second_proto.extensions[unittest_pb2.optional_int32_extension] = 0
    self.assertnotequal(first_proto, second_proto)
    first_proto.extensions[unittest_pb2.optional_int32_extension] = 0
    self.assertequal(first_proto, second_proto)

    # ensure that differences in cached values
    # don't matter if "has" bits are both false.
    first_proto = unittest_pb2.testallextensions()
    second_proto = unittest_pb2.testallextensions()
    self.assertequal(
        0, first_proto.extensions[unittest_pb2.optional_int32_extension])
    self.assertequal(first_proto, second_proto)


class mutualrecursionequalitytest(unittest.testcase):

  def testequalitywithmutualrecursion(self):
    first_proto = unittest_pb2.testmutualrecursiona()
    second_proto = unittest_pb2.testmutualrecursiona()
    self.assertequal(first_proto, second_proto)
    first_proto.bb.a.bb.optional_int32 = 23
    self.assertnotequal(first_proto, second_proto)
    second_proto.bb.a.bb.optional_int32 = 23
    self.assertequal(first_proto, second_proto)


class bytesizetest(unittest.testcase):

  def setup(self):
    self.proto = unittest_pb2.testalltypes()
    self.extended_proto = more_extensions_pb2.extendedmessage()
    self.packed_proto = unittest_pb2.testpackedtypes()
    self.packed_extended_proto = unittest_pb2.testpackedextensions()

  def size(self):
    return self.proto.bytesize()

  def testemptymessage(self):
    self.assertequal(0, self.proto.bytesize())

  def testsizedonkwargs(self):
    # use a separate message to ensure testing right after creation.
    proto = unittest_pb2.testalltypes()
    self.assertequal(0, proto.bytesize())
    proto_kwargs = unittest_pb2.testalltypes(optional_int64 = 1)
    # one byte for the tag, one to encode varint 1.
    self.assertequal(2, proto_kwargs.bytesize())

  def testvarints(self):
    def test(i, expected_varint_size):
      self.proto.clear()
      self.proto.optional_int64 = i
      # add one to the varint size for the tag info
      # for tag 1.
      self.assertequal(expected_varint_size + 1, self.size())
    test(0, 1)
    test(1, 1)
    for i, num_bytes in zip(range(7, 63, 7), range(1, 10000)):
      test((1 << i) - 1, num_bytes)
    test(-1, 10)
    test(-2, 10)
    test(-(1 << 63), 10)

  def teststrings(self):
    self.proto.optional_string = ''
    # need one byte for tag info (tag #14), and one byte for length.
    self.assertequal(2, self.size())

    self.proto.optional_string = 'abc'
    # need one byte for tag info (tag #14), and one byte for length.
    self.assertequal(2 + len(self.proto.optional_string), self.size())

    self.proto.optional_string = 'x' * 128
    # need one byte for tag info (tag #14), and two bytes for length.
    self.assertequal(3 + len(self.proto.optional_string), self.size())

  def testothernumerics(self):
    self.proto.optional_fixed32 = 1234
    # one byte for tag and 4 bytes for fixed32.
    self.assertequal(5, self.size())
    self.proto = unittest_pb2.testalltypes()

    self.proto.optional_fixed64 = 1234
    # one byte for tag and 8 bytes for fixed64.
    self.assertequal(9, self.size())
    self.proto = unittest_pb2.testalltypes()

    self.proto.optional_float = 1.234
    # one byte for tag and 4 bytes for float.
    self.assertequal(5, self.size())
    self.proto = unittest_pb2.testalltypes()

    self.proto.optional_double = 1.234
    # one byte for tag and 8 bytes for float.
    self.assertequal(9, self.size())
    self.proto = unittest_pb2.testalltypes()

    self.proto.optional_sint32 = 64
    # one byte for tag and 2 bytes for zig-zag-encoded 64.
    self.assertequal(3, self.size())
    self.proto = unittest_pb2.testalltypes()

  def testcomposites(self):
    # 3 bytes.
    self.proto.optional_nested_message.bb = (1 << 14)
    # plus one byte for bb tag.
    # plus 1 byte for optional_nested_message serialized size.
    # plus two bytes for optional_nested_message tag.
    self.assertequal(3 + 1 + 1 + 2, self.size())

  def testgroups(self):
    # 4 bytes.
    self.proto.optionalgroup.a = (1 << 21)
    # plus two bytes for |a| tag.
    # plus 2 * two bytes for start_group and end_group tags.
    self.assertequal(4 + 2 + 2*2, self.size())

  def testrepeatedscalars(self):
    self.proto.repeated_int32.append(10)  # 1 byte.
    self.proto.repeated_int32.append(128)  # 2 bytes.
    # also need 2 bytes for each entry for tag.
    self.assertequal(1 + 2 + 2*2, self.size())

  def testrepeatedscalarsextend(self):
    self.proto.repeated_int32.extend([10, 128])  # 3 bytes.
    # also need 2 bytes for each entry for tag.
    self.assertequal(1 + 2 + 2*2, self.size())

  def testrepeatedscalarsremove(self):
    self.proto.repeated_int32.append(10)  # 1 byte.
    self.proto.repeated_int32.append(128)  # 2 bytes.
    # also need 2 bytes for each entry for tag.
    self.assertequal(1 + 2 + 2*2, self.size())
    self.proto.repeated_int32.remove(128)
    self.assertequal(1 + 2, self.size())

  def testrepeatedcomposites(self):
    # empty message.  2 bytes tag plus 1 byte length.
    foreign_message_0 = self.proto.repeated_nested_message.add()
    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    foreign_message_1 = self.proto.repeated_nested_message.add()
    foreign_message_1.bb = 7
    self.assertequal(2 + 1 + 2 + 1 + 1 + 1, self.size())

  def testrepeatedcompositesdelete(self):
    # empty message.  2 bytes tag plus 1 byte length.
    foreign_message_0 = self.proto.repeated_nested_message.add()
    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    foreign_message_1 = self.proto.repeated_nested_message.add()
    foreign_message_1.bb = 9
    self.assertequal(2 + 1 + 2 + 1 + 1 + 1, self.size())

    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    del self.proto.repeated_nested_message[0]
    self.assertequal(2 + 1 + 1 + 1, self.size())

    # now add a new message.
    foreign_message_2 = self.proto.repeated_nested_message.add()
    foreign_message_2.bb = 12

    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    self.assertequal(2 + 1 + 1 + 1 + 2 + 1 + 1 + 1, self.size())

    # 2 bytes tag plus 1 byte length plus 1 byte bb tag 1 byte int.
    del self.proto.repeated_nested_message[1]
    self.assertequal(2 + 1 + 1 + 1, self.size())

    del self.proto.repeated_nested_message[0]
    self.assertequal(0, self.size())

  def testrepeatedgroups(self):
    # 2-byte start_group plus 2-byte end_group.
    group_0 = self.proto.repeatedgroup.add()
    # 2-byte start_group plus 2-byte |a| tag + 1-byte |a|
    # plus 2-byte end_group.
    group_1 = self.proto.repeatedgroup.add()
    group_1.a =  7
    self.assertequal(2 + 2 + 2 + 2 + 1 + 2, self.size())

  def testextensions(self):
    proto = unittest_pb2.testallextensions()
    self.assertequal(0, proto.bytesize())
    extension = unittest_pb2.optional_int32_extension  # field #1, 1 byte.
    proto.extensions[extension] = 23
    # 1 byte for tag, 1 byte for value.
    self.assertequal(2, proto.bytesize())

  def testcacheinvalidationfornonrepeatedscalar(self):
    # test non-extension.
    self.proto.optional_int32 = 1
    self.assertequal(2, self.proto.bytesize())
    self.proto.optional_int32 = 128
    self.assertequal(3, self.proto.bytesize())
    self.proto.clearfield('optional_int32')
    self.assertequal(0, self.proto.bytesize())

    # test within extension.
    extension = more_extensions_pb2.optional_int_extension
    self.extended_proto.extensions[extension] = 1
    self.assertequal(2, self.extended_proto.bytesize())
    self.extended_proto.extensions[extension] = 128
    self.assertequal(3, self.extended_proto.bytesize())
    self.extended_proto.clearextension(extension)
    self.assertequal(0, self.extended_proto.bytesize())

  def testcacheinvalidationforrepeatedscalar(self):
    # test non-extension.
    self.proto.repeated_int32.append(1)
    self.assertequal(3, self.proto.bytesize())
    self.proto.repeated_int32.append(1)
    self.assertequal(6, self.proto.bytesize())
    self.proto.repeated_int32[1] = 128
    self.assertequal(7, self.proto.bytesize())
    self.proto.clearfield('repeated_int32')
    self.assertequal(0, self.proto.bytesize())

    # test within extension.
    extension = more_extensions_pb2.repeated_int_extension
    repeated = self.extended_proto.extensions[extension]
    repeated.append(1)
    self.assertequal(2, self.extended_proto.bytesize())
    repeated.append(1)
    self.assertequal(4, self.extended_proto.bytesize())
    repeated[1] = 128
    self.assertequal(5, self.extended_proto.bytesize())
    self.extended_proto.clearextension(extension)
    self.assertequal(0, self.extended_proto.bytesize())

  def testcacheinvalidationfornonrepeatedmessage(self):
    # test non-extension.
    self.proto.optional_foreign_message.c = 1
    self.assertequal(5, self.proto.bytesize())
    self.proto.optional_foreign_message.c = 128
    self.assertequal(6, self.proto.bytesize())
    self.proto.optional_foreign_message.clearfield('c')
    self.assertequal(3, self.proto.bytesize())
    self.proto.clearfield('optional_foreign_message')
    self.assertequal(0, self.proto.bytesize())

    if api_implementation.type() == 'python':
      # this is only possible in pure-python implementation of the api.
      child = self.proto.optional_foreign_message
      self.proto.clearfield('optional_foreign_message')
      child.c = 128
      self.assertequal(0, self.proto.bytesize())

    # test within extension.
    extension = more_extensions_pb2.optional_message_extension
    child = self.extended_proto.extensions[extension]
    self.assertequal(0, self.extended_proto.bytesize())
    child.foreign_message_int = 1
    self.assertequal(4, self.extended_proto.bytesize())
    child.foreign_message_int = 128
    self.assertequal(5, self.extended_proto.bytesize())
    self.extended_proto.clearextension(extension)
    self.assertequal(0, self.extended_proto.bytesize())

  def testcacheinvalidationforrepeatedmessage(self):
    # test non-extension.
    child0 = self.proto.repeated_foreign_message.add()
    self.assertequal(3, self.proto.bytesize())
    self.proto.repeated_foreign_message.add()
    self.assertequal(6, self.proto.bytesize())
    child0.c = 1
    self.assertequal(8, self.proto.bytesize())
    self.proto.clearfield('repeated_foreign_message')
    self.assertequal(0, self.proto.bytesize())

    # test within extension.
    extension = more_extensions_pb2.repeated_message_extension
    child_list = self.extended_proto.extensions[extension]
    child0 = child_list.add()
    self.assertequal(2, self.extended_proto.bytesize())
    child_list.add()
    self.assertequal(4, self.extended_proto.bytesize())
    child0.foreign_message_int = 1
    self.assertequal(6, self.extended_proto.bytesize())
    child0.clearfield('foreign_message_int')
    self.assertequal(4, self.extended_proto.bytesize())
    self.extended_proto.clearextension(extension)
    self.assertequal(0, self.extended_proto.bytesize())

  def testpackedrepeatedscalars(self):
    self.assertequal(0, self.packed_proto.bytesize())

    self.packed_proto.packed_int32.append(10)   # 1 byte.
    self.packed_proto.packed_int32.append(128)  # 2 bytes.
    # the tag is 2 bytes (the field number is 90), and the varint
    # storing the length is 1 byte.
    int_size = 1 + 2 + 3
    self.assertequal(int_size, self.packed_proto.bytesize())

    self.packed_proto.packed_double.append(4.2)   # 8 bytes
    self.packed_proto.packed_double.append(3.25)  # 8 bytes
    # 2 more tag bytes, 1 more length byte.
    double_size = 8 + 8 + 3
    self.assertequal(int_size+double_size, self.packed_proto.bytesize())

    self.packed_proto.clearfield('packed_int32')
    self.assertequal(double_size, self.packed_proto.bytesize())

  def testpackedextensions(self):
    self.assertequal(0, self.packed_extended_proto.bytesize())
    extension = self.packed_extended_proto.extensions[
        unittest_pb2.packed_fixed32_extension]
    extension.extend([1, 2, 3, 4])   # 16 bytes
    # tag is 3 bytes.
    self.assertequal(19, self.packed_extended_proto.bytesize())


# issues to be sure to cover include:
#   * handling of unrecognized tags ("uninterpreted_bytes").
#   * handling of messagesets.
#   * consistent ordering of tags in the wire format,
#     including ordering between extensions and non-extension
#     fields.
#   * consistent serialization of negative numbers, especially
#     negative int32s.
#   * handling of empty submessages (with and without "has"
#     bits set).

class serializationtest(unittest.testcase):

  def testserializeemtpymessage(self):
    first_proto = unittest_pb2.testalltypes()
    second_proto = unittest_pb2.testalltypes()
    serialized = first_proto.serializetostring()
    self.assertequal(first_proto.bytesize(), len(serialized))
    second_proto.mergefromstring(serialized)
    self.assertequal(first_proto, second_proto)

  def testserializeallfields(self):
    first_proto = unittest_pb2.testalltypes()
    second_proto = unittest_pb2.testalltypes()
    test_util.setallfields(first_proto)
    serialized = first_proto.serializetostring()
    self.assertequal(first_proto.bytesize(), len(serialized))
    second_proto.mergefromstring(serialized)
    self.assertequal(first_proto, second_proto)

  def testserializeallextensions(self):
    first_proto = unittest_pb2.testallextensions()
    second_proto = unittest_pb2.testallextensions()
    test_util.setallextensions(first_proto)
    serialized = first_proto.serializetostring()
    second_proto.mergefromstring(serialized)
    self.assertequal(first_proto, second_proto)

  def testserializenegativevalues(self):
    first_proto = unittest_pb2.testalltypes()

    first_proto.optional_int32 = -1
    first_proto.optional_int64 = -(2 << 40)
    first_proto.optional_sint32 = -3
    first_proto.optional_sint64 = -(4 << 40)
    first_proto.optional_sfixed32 = -5
    first_proto.optional_sfixed64 = -(6 << 40)

    second_proto = unittest_pb2.testalltypes.fromstring(
        first_proto.serializetostring())

    self.assertequal(first_proto, second_proto)

  def testparsetruncated(self):
    # this test is only applicable for the python implementation of the api.
    if api_implementation.type() != 'python':
      return

    first_proto = unittest_pb2.testalltypes()
    test_util.setallfields(first_proto)
    serialized = first_proto.serializetostring()

    for truncation_point in xrange(len(serialized) + 1):
      try:
        second_proto = unittest_pb2.testalltypes()
        unknown_fields = unittest_pb2.testemptymessage()
        pos = second_proto._internalparse(serialized, 0, truncation_point)
        # if we didn't raise an error then we read exactly the amount expected.
        self.assertequal(truncation_point, pos)

        # parsing to unknown fields should not throw if parsing to known fields
        # did not.
        try:
          pos2 = unknown_fields._internalparse(serialized, 0, truncation_point)
          self.assertequal(truncation_point, pos2)
        except message.decodeerror:
          self.fail('parsing unknown fields failed when parsing known fields '
                    'did not.')
      except message.decodeerror:
        # parsing unknown fields should also fail.
        self.assertraises(message.decodeerror, unknown_fields._internalparse,
                          serialized, 0, truncation_point)

  def testcanonicalserializationorder(self):
    proto = more_messages_pb2.outoforderfields()
    # these are also their tag numbers.  even though we're setting these in
    # reverse-tag order and they're listed in reverse tag-order in the .proto
    # file, they should nonetheless be serialized in tag order.
    proto.optional_sint32 = 5
    proto.extensions[more_messages_pb2.optional_uint64] = 4
    proto.optional_uint32 = 3
    proto.extensions[more_messages_pb2.optional_int64] = 2
    proto.optional_int32 = 1
    serialized = proto.serializetostring()
    self.assertequal(proto.bytesize(), len(serialized))
    d = _minidecoder(serialized)
    readtag = d.readfieldnumberandwiretype
    self.assertequal((1, wire_format.wiretype_varint), readtag())
    self.assertequal(1, d.readint32())
    self.assertequal((2, wire_format.wiretype_varint), readtag())
    self.assertequal(2, d.readint64())
    self.assertequal((3, wire_format.wiretype_varint), readtag())
    self.assertequal(3, d.readuint32())
    self.assertequal((4, wire_format.wiretype_varint), readtag())
    self.assertequal(4, d.readuint64())
    self.assertequal((5, wire_format.wiretype_varint), readtag())
    self.assertequal(5, d.readsint32())

  def testcanonicalserializationordersameascpp(self):
    # copy of the same test we use for c++.
    proto = unittest_pb2.testfieldorderings()
    test_util.setallfieldsandextensions(proto)
    serialized = proto.serializetostring()
    test_util.expectallfieldsandextensionsinorder(serialized)

  def testmergefromstringwhenfieldsalreadyset(self):
    first_proto = unittest_pb2.testalltypes()
    first_proto.repeated_string.append('foobar')
    first_proto.optional_int32 = 23
    first_proto.optional_nested_message.bb = 42
    serialized = first_proto.serializetostring()

    second_proto = unittest_pb2.testalltypes()
    second_proto.repeated_string.append('baz')
    second_proto.optional_int32 = 100
    second_proto.optional_nested_message.bb = 999

    second_proto.mergefromstring(serialized)
    # ensure that we append to repeated fields.
    self.assertequal(['baz', 'foobar'], list(second_proto.repeated_string))
    # ensure that we overwrite nonrepeatd scalars.
    self.assertequal(23, second_proto.optional_int32)
    # ensure that we recursively call mergefromstring() on
    # submessages.
    self.assertequal(42, second_proto.optional_nested_message.bb)

  def testmessagesetwireformat(self):
    proto = unittest_mset_pb2.testmessageset()
    extension_message1 = unittest_mset_pb2.testmessagesetextension1
    extension_message2 = unittest_mset_pb2.testmessagesetextension2
    extension1 = extension_message1.message_set_extension
    extension2 = extension_message2.message_set_extension
    proto.extensions[extension1].i = 123
    proto.extensions[extension2].str = 'foo'

    # serialize using the messageset wire format (this is specified in the
    # .proto file).
    serialized = proto.serializetostring()

    raw = unittest_mset_pb2.rawmessageset()
    self.assertequal(false,
                     raw.descriptor.getoptions().message_set_wire_format)
    raw.mergefromstring(serialized)
    self.assertequal(2, len(raw.item))

    message1 = unittest_mset_pb2.testmessagesetextension1()
    message1.mergefromstring(raw.item[0].message)
    self.assertequal(123, message1.i)

    message2 = unittest_mset_pb2.testmessagesetextension2()
    message2.mergefromstring(raw.item[1].message)
    self.assertequal('foo', message2.str)

    # deserialize using the messageset wire format.
    proto2 = unittest_mset_pb2.testmessageset()
    proto2.mergefromstring(serialized)
    self.assertequal(123, proto2.extensions[extension1].i)
    self.assertequal('foo', proto2.extensions[extension2].str)

    # check byte size.
    self.assertequal(proto2.bytesize(), len(serialized))
    self.assertequal(proto.bytesize(), len(serialized))

  def testmessagesetwireformatunknownextension(self):
    # create a message using the message set wire format with an unknown
    # message.
    raw = unittest_mset_pb2.rawmessageset()

    # add an item.
    item = raw.item.add()
    item.type_id = 1545008
    extension_message1 = unittest_mset_pb2.testmessagesetextension1
    message1 = unittest_mset_pb2.testmessagesetextension1()
    message1.i = 12345
    item.message = message1.serializetostring()

    # add a second, unknown extension.
    item = raw.item.add()
    item.type_id = 1545009
    extension_message1 = unittest_mset_pb2.testmessagesetextension1
    message1 = unittest_mset_pb2.testmessagesetextension1()
    message1.i = 12346
    item.message = message1.serializetostring()

    # add another unknown extension.
    item = raw.item.add()
    item.type_id = 1545010
    message1 = unittest_mset_pb2.testmessagesetextension2()
    message1.str = 'foo'
    item.message = message1.serializetostring()

    serialized = raw.serializetostring()

    # parse message using the message set wire format.
    proto = unittest_mset_pb2.testmessageset()
    proto.mergefromstring(serialized)

    # check that the message parsed well.
    extension_message1 = unittest_mset_pb2.testmessagesetextension1
    extension1 = extension_message1.message_set_extension
    self.assertequals(12345, proto.extensions[extension1].i)

  def testunknownfields(self):
    proto = unittest_pb2.testalltypes()
    test_util.setallfields(proto)

    serialized = proto.serializetostring()

    # the empty message should be parsable with all of the fields
    # unknown.
    proto2 = unittest_pb2.testemptymessage()

    # parsing this message should succeed.
    proto2.mergefromstring(serialized)

    # now test with a int64 field set.
    proto = unittest_pb2.testalltypes()
    proto.optional_int64 = 0x0fffffffffffffff
    serialized = proto.serializetostring()
    # the empty message should be parsable with all of the fields
    # unknown.
    proto2 = unittest_pb2.testemptymessage()
    # parsing this message should succeed.
    proto2.mergefromstring(serialized)

  def _checkraises(self, exc_class, callable_obj, exception):
    """this method checks if the excpetion type and message are as expected."""
    try:
      callable_obj()
    except exc_class as ex:
      # check if the exception message is the right one.
      self.assertequal(exception, str(ex))
      return
    else:
      raise self.failureexception('%s not raised' % str(exc_class))

  def testserializeuninitialized(self):
    proto = unittest_pb2.testrequired()
    self._checkraises(
        message.encodeerror,
        proto.serializetostring,
        'message protobuf_unittest.testrequired is missing required fields: '
        'a,b,c')
    # shouldn't raise exceptions.
    partial = proto.serializepartialtostring()

    proto2 = unittest_pb2.testrequired()
    self.assertfalse(proto2.hasfield('a'))
    # proto2 parsefromstring does not check that required fields are set.
    proto2.parsefromstring(partial)
    self.assertfalse(proto2.hasfield('a'))

    proto.a = 1
    self._checkraises(
        message.encodeerror,
        proto.serializetostring,
        'message protobuf_unittest.testrequired is missing required fields: b,c')
    # shouldn't raise exceptions.
    partial = proto.serializepartialtostring()

    proto.b = 2
    self._checkraises(
        message.encodeerror,
        proto.serializetostring,
        'message protobuf_unittest.testrequired is missing required fields: c')
    # shouldn't raise exceptions.
    partial = proto.serializepartialtostring()

    proto.c = 3
    serialized = proto.serializetostring()
    # shouldn't raise exceptions.
    partial = proto.serializepartialtostring()

    proto2 = unittest_pb2.testrequired()
    proto2.mergefromstring(serialized)
    self.assertequal(1, proto2.a)
    self.assertequal(2, proto2.b)
    self.assertequal(3, proto2.c)
    proto2.parsefromstring(partial)
    self.assertequal(1, proto2.a)
    self.assertequal(2, proto2.b)
    self.assertequal(3, proto2.c)

  def testserializeuninitializedsubmessage(self):
    proto = unittest_pb2.testrequiredforeign()

    # sub-message doesn't exist yet, so this succeeds.
    proto.serializetostring()

    proto.optional_message.a = 1
    self._checkraises(
        message.encodeerror,
        proto.serializetostring,
        'message protobuf_unittest.testrequiredforeign '
        'is missing required fields: '
        'optional_message.b,optional_message.c')

    proto.optional_message.b = 2
    proto.optional_message.c = 3
    proto.serializetostring()

    proto.repeated_message.add().a = 1
    proto.repeated_message.add().b = 2
    self._checkraises(
        message.encodeerror,
        proto.serializetostring,
        'message protobuf_unittest.testrequiredforeign is missing required fields: '
        'repeated_message[0].b,repeated_message[0].c,'
        'repeated_message[1].a,repeated_message[1].c')

    proto.repeated_message[0].b = 2
    proto.repeated_message[0].c = 3
    proto.repeated_message[1].a = 1
    proto.repeated_message[1].c = 3
    proto.serializetostring()

  def testserializeallpackedfields(self):
    first_proto = unittest_pb2.testpackedtypes()
    second_proto = unittest_pb2.testpackedtypes()
    test_util.setallpackedfields(first_proto)
    serialized = first_proto.serializetostring()
    self.assertequal(first_proto.bytesize(), len(serialized))
    bytes_read = second_proto.mergefromstring(serialized)
    self.assertequal(second_proto.bytesize(), bytes_read)
    self.assertequal(first_proto, second_proto)

  def testserializeallpackedextensions(self):
    first_proto = unittest_pb2.testpackedextensions()
    second_proto = unittest_pb2.testpackedextensions()
    test_util.setallpackedextensions(first_proto)
    serialized = first_proto.serializetostring()
    bytes_read = second_proto.mergefromstring(serialized)
    self.assertequal(second_proto.bytesize(), bytes_read)
    self.assertequal(first_proto, second_proto)

  def testmergepackedfromstringwhensomefieldsalreadyset(self):
    first_proto = unittest_pb2.testpackedtypes()
    first_proto.packed_int32.extend([1, 2])
    first_proto.packed_double.append(3.0)
    serialized = first_proto.serializetostring()

    second_proto = unittest_pb2.testpackedtypes()
    second_proto.packed_int32.append(3)
    second_proto.packed_double.extend([1.0, 2.0])
    second_proto.packed_sint32.append(4)

    second_proto.mergefromstring(serialized)
    self.assertequal([3, 1, 2], second_proto.packed_int32)
    self.assertequal([1.0, 2.0, 3.0], second_proto.packed_double)
    self.assertequal([4], second_proto.packed_sint32)

  def testpackedfieldswireformat(self):
    proto = unittest_pb2.testpackedtypes()
    proto.packed_int32.extend([1, 2, 150, 3])  # 1 + 1 + 2 + 1 bytes
    proto.packed_double.extend([1.0, 1000.0])  # 8 + 8 bytes
    proto.packed_float.append(2.0)             # 4 bytes, will be before double
    serialized = proto.serializetostring()
    self.assertequal(proto.bytesize(), len(serialized))
    d = _minidecoder(serialized)
    readtag = d.readfieldnumberandwiretype
    self.assertequal((90, wire_format.wiretype_length_delimited), readtag())
    self.assertequal(1+1+1+2, d.readint32())
    self.assertequal(1, d.readint32())
    self.assertequal(2, d.readint32())
    self.assertequal(150, d.readint32())
    self.assertequal(3, d.readint32())
    self.assertequal((100, wire_format.wiretype_length_delimited), readtag())
    self.assertequal(4, d.readint32())
    self.assertequal(2.0, d.readfloat())
    self.assertequal((101, wire_format.wiretype_length_delimited), readtag())
    self.assertequal(8+8, d.readint32())
    self.assertequal(1.0, d.readdouble())
    self.assertequal(1000.0, d.readdouble())
    self.asserttrue(d.endofstream())

  def testparsepackedfromunpacked(self):
    unpacked = unittest_pb2.testunpackedtypes()
    test_util.setallunpackedfields(unpacked)
    packed = unittest_pb2.testpackedtypes()
    packed.mergefromstring(unpacked.serializetostring())
    expected = unittest_pb2.testpackedtypes()
    test_util.setallpackedfields(expected)
    self.assertequal(expected, packed)

  def testparseunpackedfrompacked(self):
    packed = unittest_pb2.testpackedtypes()
    test_util.setallpackedfields(packed)
    unpacked = unittest_pb2.testunpackedtypes()
    unpacked.mergefromstring(packed.serializetostring())
    expected = unittest_pb2.testunpackedtypes()
    test_util.setallunpackedfields(expected)
    self.assertequal(expected, unpacked)

  def testfieldnumbers(self):
    proto = unittest_pb2.testalltypes()
    self.assertequal(unittest_pb2.testalltypes.nestedmessage.bb_field_number, 1)
    self.assertequal(unittest_pb2.testalltypes.optional_int32_field_number, 1)
    self.assertequal(unittest_pb2.testalltypes.optionalgroup_field_number, 16)
    self.assertequal(
      unittest_pb2.testalltypes.optional_nested_message_field_number, 18)
    self.assertequal(
      unittest_pb2.testalltypes.optional_nested_enum_field_number, 21)
    self.assertequal(unittest_pb2.testalltypes.repeated_int32_field_number, 31)
    self.assertequal(unittest_pb2.testalltypes.repeatedgroup_field_number, 46)
    self.assertequal(
      unittest_pb2.testalltypes.repeated_nested_message_field_number, 48)
    self.assertequal(
      unittest_pb2.testalltypes.repeated_nested_enum_field_number, 51)

  def testextensionfieldnumbers(self):
    self.assertequal(unittest_pb2.testrequired.single.number, 1000)
    self.assertequal(unittest_pb2.testrequired.single_field_number, 1000)
    self.assertequal(unittest_pb2.testrequired.multi.number, 1001)
    self.assertequal(unittest_pb2.testrequired.multi_field_number, 1001)
    self.assertequal(unittest_pb2.optional_int32_extension.number, 1)
    self.assertequal(unittest_pb2.optional_int32_extension_field_number, 1)
    self.assertequal(unittest_pb2.optionalgroup_extension.number, 16)
    self.assertequal(unittest_pb2.optionalgroup_extension_field_number, 16)
    self.assertequal(unittest_pb2.optional_nested_message_extension.number, 18)
    self.assertequal(
      unittest_pb2.optional_nested_message_extension_field_number, 18)
    self.assertequal(unittest_pb2.optional_nested_enum_extension.number, 21)
    self.assertequal(unittest_pb2.optional_nested_enum_extension_field_number,
      21)
    self.assertequal(unittest_pb2.repeated_int32_extension.number, 31)
    self.assertequal(unittest_pb2.repeated_int32_extension_field_number, 31)
    self.assertequal(unittest_pb2.repeatedgroup_extension.number, 46)
    self.assertequal(unittest_pb2.repeatedgroup_extension_field_number, 46)
    self.assertequal(unittest_pb2.repeated_nested_message_extension.number, 48)
    self.assertequal(
      unittest_pb2.repeated_nested_message_extension_field_number, 48)
    self.assertequal(unittest_pb2.repeated_nested_enum_extension.number, 51)
    self.assertequal(unittest_pb2.repeated_nested_enum_extension_field_number,
      51)

  def testinitkwargs(self):
    proto = unittest_pb2.testalltypes(
        optional_int32=1,
        optional_string='foo',
        optional_bool=true,
        optional_bytes='bar',
        optional_nested_message=unittest_pb2.testalltypes.nestedmessage(bb=1),
        optional_foreign_message=unittest_pb2.foreignmessage(c=1),
        optional_nested_enum=unittest_pb2.testalltypes.foo,
        optional_foreign_enum=unittest_pb2.foreign_foo,
        repeated_int32=[1, 2, 3])
    self.asserttrue(proto.isinitialized())
    self.asserttrue(proto.hasfield('optional_int32'))
    self.asserttrue(proto.hasfield('optional_string'))
    self.asserttrue(proto.hasfield('optional_bool'))
    self.asserttrue(proto.hasfield('optional_bytes'))
    self.asserttrue(proto.hasfield('optional_nested_message'))
    self.asserttrue(proto.hasfield('optional_foreign_message'))
    self.asserttrue(proto.hasfield('optional_nested_enum'))
    self.asserttrue(proto.hasfield('optional_foreign_enum'))
    self.assertequal(1, proto.optional_int32)
    self.assertequal('foo', proto.optional_string)
    self.assertequal(true, proto.optional_bool)
    self.assertequal('bar', proto.optional_bytes)
    self.assertequal(1, proto.optional_nested_message.bb)
    self.assertequal(1, proto.optional_foreign_message.c)
    self.assertequal(unittest_pb2.testalltypes.foo,
                     proto.optional_nested_enum)
    self.assertequal(unittest_pb2.foreign_foo, proto.optional_foreign_enum)
    self.assertequal([1, 2, 3], proto.repeated_int32)

  def testinitargsunknownfieldname(self):
    def initalizeemptymessagewithextrakeywordarg():
      unused_proto = unittest_pb2.testemptymessage(unknown='unknown')
    self._checkraises(valueerror,
                      initalizeemptymessagewithextrakeywordarg,
                      'protocol message has no "unknown" field.')

  def testinitrequiredkwargs(self):
    proto = unittest_pb2.testrequired(a=1, b=1, c=1)
    self.asserttrue(proto.isinitialized())
    self.asserttrue(proto.hasfield('a'))
    self.asserttrue(proto.hasfield('b'))
    self.asserttrue(proto.hasfield('c'))
    self.asserttrue(not proto.hasfield('dummy2'))
    self.assertequal(1, proto.a)
    self.assertequal(1, proto.b)
    self.assertequal(1, proto.c)

  def testinitrequiredforeignkwargs(self):
    proto = unittest_pb2.testrequiredforeign(
        optional_message=unittest_pb2.testrequired(a=1, b=1, c=1))
    self.asserttrue(proto.isinitialized())
    self.asserttrue(proto.hasfield('optional_message'))
    self.asserttrue(proto.optional_message.isinitialized())
    self.asserttrue(proto.optional_message.hasfield('a'))
    self.asserttrue(proto.optional_message.hasfield('b'))
    self.asserttrue(proto.optional_message.hasfield('c'))
    self.asserttrue(not proto.optional_message.hasfield('dummy2'))
    self.assertequal(unittest_pb2.testrequired(a=1, b=1, c=1),
                     proto.optional_message)
    self.assertequal(1, proto.optional_message.a)
    self.assertequal(1, proto.optional_message.b)
    self.assertequal(1, proto.optional_message.c)

  def testinitrepeatedkwargs(self):
    proto = unittest_pb2.testalltypes(repeated_int32=[1, 2, 3])
    self.asserttrue(proto.isinitialized())
    self.assertequal(1, proto.repeated_int32[0])
    self.assertequal(2, proto.repeated_int32[1])
    self.assertequal(3, proto.repeated_int32[2])


class optionstest(unittest.testcase):

  def testmessageoptions(self):
    proto = unittest_mset_pb2.testmessageset()
    self.assertequal(true,
                     proto.descriptor.getoptions().message_set_wire_format)
    proto = unittest_pb2.testalltypes()
    self.assertequal(false,
                     proto.descriptor.getoptions().message_set_wire_format)

  def testpackedoptions(self):
    proto = unittest_pb2.testalltypes()
    proto.optional_int32 = 1
    proto.optional_double = 3.0
    for field_descriptor, _ in proto.listfields():
      self.assertequal(false, field_descriptor.getoptions().packed)

    proto = unittest_pb2.testpackedtypes()
    proto.packed_int32.append(1)
    proto.packed_double.append(3.0)
    for field_descriptor, _ in proto.listfields():
      self.assertequal(true, field_descriptor.getoptions().packed)
      self.assertequal(reflection._fielddescriptor.label_repeated,
                       field_descriptor.label)



if __name__ == '__main__':
  unittest.main()
