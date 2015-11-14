#! /usr/bin/python
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

"""tests python protocol buffers against the golden message.

note that the golden messages exercise every known field type, thus this
test ends up exercising and verifying nearly all of the parsing and
serialization code in the whole library.

todo(kenton):  merge with wire_format_test?  it doesn't make a whole lot of
sense to call this a test of the "message" module, which only declares an
abstract interface.
"""

__author__ = 'gps@google.com (gregory p. smith)'

import copy
import math
import operator
import pickle

import unittest
from google.protobuf import unittest_import_pb2
from google.protobuf import unittest_pb2
from google.protobuf.internal import api_implementation
from google.protobuf.internal import test_util
from google.protobuf import message

# python pre-2.6 does not have isinf() or isnan() functions, so we have
# to provide our own.
def isnan(val):
  # nan is never equal to itself.
  return val != val
def isinf(val):
  # infinity times zero equals nan.
  return not isnan(val) and isnan(val * 0)
def isposinf(val):
  return isinf(val) and (val > 0)
def isneginf(val):
  return isinf(val) and (val < 0)

class messagetest(unittest.testcase):

  def testgoldenmessage(self):
    golden_data = test_util.goldenfile('golden_message').read()
    golden_message = unittest_pb2.testalltypes()
    golden_message.parsefromstring(golden_data)
    test_util.expectallfieldsset(self, golden_message)
    self.assertequal(golden_data, golden_message.serializetostring())
    golden_copy = copy.deepcopy(golden_message)
    self.assertequal(golden_data, golden_copy.serializetostring())

  def testgoldenextensions(self):
    golden_data = test_util.goldenfile('golden_message').read()
    golden_message = unittest_pb2.testallextensions()
    golden_message.parsefromstring(golden_data)
    all_set = unittest_pb2.testallextensions()
    test_util.setallextensions(all_set)
    self.assertequals(all_set, golden_message)
    self.assertequal(golden_data, golden_message.serializetostring())
    golden_copy = copy.deepcopy(golden_message)
    self.assertequal(golden_data, golden_copy.serializetostring())

  def testgoldenpackedmessage(self):
    golden_data = test_util.goldenfile('golden_packed_fields_message').read()
    golden_message = unittest_pb2.testpackedtypes()
    golden_message.parsefromstring(golden_data)
    all_set = unittest_pb2.testpackedtypes()
    test_util.setallpackedfields(all_set)
    self.assertequals(all_set, golden_message)
    self.assertequal(golden_data, all_set.serializetostring())
    golden_copy = copy.deepcopy(golden_message)
    self.assertequal(golden_data, golden_copy.serializetostring())

  def testgoldenpackedextensions(self):
    golden_data = test_util.goldenfile('golden_packed_fields_message').read()
    golden_message = unittest_pb2.testpackedextensions()
    golden_message.parsefromstring(golden_data)
    all_set = unittest_pb2.testpackedextensions()
    test_util.setallpackedextensions(all_set)
    self.assertequals(all_set, golden_message)
    self.assertequal(golden_data, all_set.serializetostring())
    golden_copy = copy.deepcopy(golden_message)
    self.assertequal(golden_data, golden_copy.serializetostring())

  def testpicklesupport(self):
    golden_data = test_util.goldenfile('golden_message').read()
    golden_message = unittest_pb2.testalltypes()
    golden_message.parsefromstring(golden_data)
    pickled_message = pickle.dumps(golden_message)

    unpickled_message = pickle.loads(pickled_message)
    self.assertequals(unpickled_message, golden_message)

  def testpickleincompleteproto(self):
    golden_message = unittest_pb2.testrequired(a=1)
    pickled_message = pickle.dumps(golden_message)

    unpickled_message = pickle.loads(pickled_message)
    self.assertequals(unpickled_message, golden_message)
    self.assertequals(unpickled_message.a, 1)
    # this is still an incomplete proto - so serializing should fail
    self.assertraises(message.encodeerror, unpickled_message.serializetostring)

  def testpositiveinfinity(self):
    golden_data = ('\x5d\x00\x00\x80\x7f'
                   '\x61\x00\x00\x00\x00\x00\x00\xf0\x7f'
                   '\xcd\x02\x00\x00\x80\x7f'
                   '\xd1\x02\x00\x00\x00\x00\x00\x00\xf0\x7f')
    golden_message = unittest_pb2.testalltypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isposinf(golden_message.optional_float))
    self.asserttrue(isposinf(golden_message.optional_double))
    self.asserttrue(isposinf(golden_message.repeated_float[0]))
    self.asserttrue(isposinf(golden_message.repeated_double[0]))
    self.assertequal(golden_data, golden_message.serializetostring())

  def testnegativeinfinity(self):
    golden_data = ('\x5d\x00\x00\x80\xff'
                   '\x61\x00\x00\x00\x00\x00\x00\xf0\xff'
                   '\xcd\x02\x00\x00\x80\xff'
                   '\xd1\x02\x00\x00\x00\x00\x00\x00\xf0\xff')
    golden_message = unittest_pb2.testalltypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isneginf(golden_message.optional_float))
    self.asserttrue(isneginf(golden_message.optional_double))
    self.asserttrue(isneginf(golden_message.repeated_float[0]))
    self.asserttrue(isneginf(golden_message.repeated_double[0]))
    self.assertequal(golden_data, golden_message.serializetostring())

  def testnotanumber(self):
    golden_data = ('\x5d\x00\x00\xc0\x7f'
                   '\x61\x00\x00\x00\x00\x00\x00\xf8\x7f'
                   '\xcd\x02\x00\x00\xc0\x7f'
                   '\xd1\x02\x00\x00\x00\x00\x00\x00\xf8\x7f')
    golden_message = unittest_pb2.testalltypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isnan(golden_message.optional_float))
    self.asserttrue(isnan(golden_message.optional_double))
    self.asserttrue(isnan(golden_message.repeated_float[0]))
    self.asserttrue(isnan(golden_message.repeated_double[0]))

    # the protocol buffer may serialize to any one of multiple different
    # representations of a nan.  rather than verify a specific representation,
    # verify the serialized string can be converted into a correctly
    # behaving protocol buffer.
    serialized = golden_message.serializetostring()
    message = unittest_pb2.testalltypes()
    message.parsefromstring(serialized)
    self.asserttrue(isnan(message.optional_float))
    self.asserttrue(isnan(message.optional_double))
    self.asserttrue(isnan(message.repeated_float[0]))
    self.asserttrue(isnan(message.repeated_double[0]))

  def testpositiveinfinitypacked(self):
    golden_data = ('\xa2\x06\x04\x00\x00\x80\x7f'
                   '\xaa\x06\x08\x00\x00\x00\x00\x00\x00\xf0\x7f')
    golden_message = unittest_pb2.testpackedtypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isposinf(golden_message.packed_float[0]))
    self.asserttrue(isposinf(golden_message.packed_double[0]))
    self.assertequal(golden_data, golden_message.serializetostring())

  def testnegativeinfinitypacked(self):
    golden_data = ('\xa2\x06\x04\x00\x00\x80\xff'
                   '\xaa\x06\x08\x00\x00\x00\x00\x00\x00\xf0\xff')
    golden_message = unittest_pb2.testpackedtypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isneginf(golden_message.packed_float[0]))
    self.asserttrue(isneginf(golden_message.packed_double[0]))
    self.assertequal(golden_data, golden_message.serializetostring())

  def testnotanumberpacked(self):
    golden_data = ('\xa2\x06\x04\x00\x00\xc0\x7f'
                   '\xaa\x06\x08\x00\x00\x00\x00\x00\x00\xf8\x7f')
    golden_message = unittest_pb2.testpackedtypes()
    golden_message.parsefromstring(golden_data)
    self.asserttrue(isnan(golden_message.packed_float[0]))
    self.asserttrue(isnan(golden_message.packed_double[0]))

    serialized = golden_message.serializetostring()
    message = unittest_pb2.testpackedtypes()
    message.parsefromstring(serialized)
    self.asserttrue(isnan(message.packed_float[0]))
    self.asserttrue(isnan(message.packed_double[0]))

  def testextremefloatvalues(self):
    message = unittest_pb2.testalltypes()

    # most positive exponent, no significand bits set.
    kmostposexponentnosigbits = math.pow(2, 127)
    message.optional_float = kmostposexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == kmostposexponentnosigbits)

    # most positive exponent, one significand bit set.
    kmostposexponentonesigbit = 1.5 * math.pow(2, 127)
    message.optional_float = kmostposexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == kmostposexponentonesigbit)

    # repeat last two cases with values of same magnitude, but negative.
    message.optional_float = -kmostposexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == -kmostposexponentnosigbits)

    message.optional_float = -kmostposexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == -kmostposexponentonesigbit)

    # most negative exponent, no significand bits set.
    kmostnegexponentnosigbits = math.pow(2, -127)
    message.optional_float = kmostnegexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == kmostnegexponentnosigbits)

    # most negative exponent, one significand bit set.
    kmostnegexponentonesigbit = 1.5 * math.pow(2, -127)
    message.optional_float = kmostnegexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == kmostnegexponentonesigbit)

    # repeat last two cases with values of the same magnitude, but negative.
    message.optional_float = -kmostnegexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == -kmostnegexponentnosigbits)

    message.optional_float = -kmostnegexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_float == -kmostnegexponentonesigbit)

  def testextremedoublevalues(self):
    message = unittest_pb2.testalltypes()

    # most positive exponent, no significand bits set.
    kmostposexponentnosigbits = math.pow(2, 1023)
    message.optional_double = kmostposexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == kmostposexponentnosigbits)

    # most positive exponent, one significand bit set.
    kmostposexponentonesigbit = 1.5 * math.pow(2, 1023)
    message.optional_double = kmostposexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == kmostposexponentonesigbit)

    # repeat last two cases with values of same magnitude, but negative.
    message.optional_double = -kmostposexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == -kmostposexponentnosigbits)

    message.optional_double = -kmostposexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == -kmostposexponentonesigbit)

    # most negative exponent, no significand bits set.
    kmostnegexponentnosigbits = math.pow(2, -1023)
    message.optional_double = kmostnegexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == kmostnegexponentnosigbits)

    # most negative exponent, one significand bit set.
    kmostnegexponentonesigbit = 1.5 * math.pow(2, -1023)
    message.optional_double = kmostnegexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == kmostnegexponentonesigbit)

    # repeat last two cases with values of the same magnitude, but negative.
    message.optional_double = -kmostnegexponentnosigbits
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == -kmostnegexponentnosigbits)

    message.optional_double = -kmostnegexponentonesigbit
    message.parsefromstring(message.serializetostring())
    self.asserttrue(message.optional_double == -kmostnegexponentonesigbit)

  def testsortingrepeatedscalarfieldsdefaultcomparator(self):
    """check some different types with the default comparator."""
    message = unittest_pb2.testalltypes()

    # todo(mattp): would testing more scalar types strengthen test?
    message.repeated_int32.append(1)
    message.repeated_int32.append(3)
    message.repeated_int32.append(2)
    message.repeated_int32.sort()
    self.assertequal(message.repeated_int32[0], 1)
    self.assertequal(message.repeated_int32[1], 2)
    self.assertequal(message.repeated_int32[2], 3)

    message.repeated_float.append(1.1)
    message.repeated_float.append(1.3)
    message.repeated_float.append(1.2)
    message.repeated_float.sort()
    self.assertalmostequal(message.repeated_float[0], 1.1)
    self.assertalmostequal(message.repeated_float[1], 1.2)
    self.assertalmostequal(message.repeated_float[2], 1.3)

    message.repeated_string.append('a')
    message.repeated_string.append('c')
    message.repeated_string.append('b')
    message.repeated_string.sort()
    self.assertequal(message.repeated_string[0], 'a')
    self.assertequal(message.repeated_string[1], 'b')
    self.assertequal(message.repeated_string[2], 'c')

    message.repeated_bytes.append('a')
    message.repeated_bytes.append('c')
    message.repeated_bytes.append('b')
    message.repeated_bytes.sort()
    self.assertequal(message.repeated_bytes[0], 'a')
    self.assertequal(message.repeated_bytes[1], 'b')
    self.assertequal(message.repeated_bytes[2], 'c')

  def testsortingrepeatedscalarfieldscustomcomparator(self):
    """check some different types with custom comparator."""
    message = unittest_pb2.testalltypes()

    message.repeated_int32.append(-3)
    message.repeated_int32.append(-2)
    message.repeated_int32.append(-1)
    message.repeated_int32.sort(lambda x,y: cmp(abs(x), abs(y)))
    self.assertequal(message.repeated_int32[0], -1)
    self.assertequal(message.repeated_int32[1], -2)
    self.assertequal(message.repeated_int32[2], -3)

    message.repeated_string.append('aaa')
    message.repeated_string.append('bb')
    message.repeated_string.append('c')
    message.repeated_string.sort(lambda x,y: cmp(len(x), len(y)))
    self.assertequal(message.repeated_string[0], 'c')
    self.assertequal(message.repeated_string[1], 'bb')
    self.assertequal(message.repeated_string[2], 'aaa')

  def testsortingrepeatedcompositefieldscustomcomparator(self):
    """check passing a custom comparator to sort a repeated composite field."""
    message = unittest_pb2.testalltypes()

    message.repeated_nested_message.add().bb = 1
    message.repeated_nested_message.add().bb = 3
    message.repeated_nested_message.add().bb = 2
    message.repeated_nested_message.add().bb = 6
    message.repeated_nested_message.add().bb = 5
    message.repeated_nested_message.add().bb = 4
    message.repeated_nested_message.sort(lambda x,y: cmp(x.bb, y.bb))
    self.assertequal(message.repeated_nested_message[0].bb, 1)
    self.assertequal(message.repeated_nested_message[1].bb, 2)
    self.assertequal(message.repeated_nested_message[2].bb, 3)
    self.assertequal(message.repeated_nested_message[3].bb, 4)
    self.assertequal(message.repeated_nested_message[4].bb, 5)
    self.assertequal(message.repeated_nested_message[5].bb, 6)

  def testrepeatedcompositefieldsortarguments(self):
    """check sorting a repeated composite field using list.sort() arguments."""
    message = unittest_pb2.testalltypes()

    get_bb = operator.attrgetter('bb')
    cmp_bb = lambda a, b: cmp(a.bb, b.bb)
    message.repeated_nested_message.add().bb = 1
    message.repeated_nested_message.add().bb = 3
    message.repeated_nested_message.add().bb = 2
    message.repeated_nested_message.add().bb = 6
    message.repeated_nested_message.add().bb = 5
    message.repeated_nested_message.add().bb = 4
    message.repeated_nested_message.sort(key=get_bb)
    self.assertequal([k.bb for k in message.repeated_nested_message],
                     [1, 2, 3, 4, 5, 6])
    message.repeated_nested_message.sort(key=get_bb, reverse=true)
    self.assertequal([k.bb for k in message.repeated_nested_message],
                     [6, 5, 4, 3, 2, 1])
    message.repeated_nested_message.sort(sort_function=cmp_bb)
    self.assertequal([k.bb for k in message.repeated_nested_message],
                     [1, 2, 3, 4, 5, 6])
    message.repeated_nested_message.sort(cmp=cmp_bb, reverse=true)
    self.assertequal([k.bb for k in message.repeated_nested_message],
                     [6, 5, 4, 3, 2, 1])

  def testrepeatedscalarfieldsortarguments(self):
    """check sorting a scalar field using list.sort() arguments."""
    message = unittest_pb2.testalltypes()

    abs_cmp = lambda a, b: cmp(abs(a), abs(b))
    message.repeated_int32.append(-3)
    message.repeated_int32.append(-2)
    message.repeated_int32.append(-1)
    message.repeated_int32.sort(key=abs)
    self.assertequal(list(message.repeated_int32), [-1, -2, -3])
    message.repeated_int32.sort(key=abs, reverse=true)
    self.assertequal(list(message.repeated_int32), [-3, -2, -1])
    message.repeated_int32.sort(sort_function=abs_cmp)
    self.assertequal(list(message.repeated_int32), [-1, -2, -3])
    message.repeated_int32.sort(cmp=abs_cmp, reverse=true)
    self.assertequal(list(message.repeated_int32), [-3, -2, -1])

    len_cmp = lambda a, b: cmp(len(a), len(b))
    message.repeated_string.append('aaa')
    message.repeated_string.append('bb')
    message.repeated_string.append('c')
    message.repeated_string.sort(key=len)
    self.assertequal(list(message.repeated_string), ['c', 'bb', 'aaa'])
    message.repeated_string.sort(key=len, reverse=true)
    self.assertequal(list(message.repeated_string), ['aaa', 'bb', 'c'])
    message.repeated_string.sort(sort_function=len_cmp)
    self.assertequal(list(message.repeated_string), ['c', 'bb', 'aaa'])
    message.repeated_string.sort(cmp=len_cmp, reverse=true)
    self.assertequal(list(message.repeated_string), ['aaa', 'bb', 'c'])

  def testparsingmerge(self):
    """check the merge behavior when a required or optional field appears
    multiple times in the input."""
    messages = [
        unittest_pb2.testalltypes(),
        unittest_pb2.testalltypes(),
        unittest_pb2.testalltypes() ]
    messages[0].optional_int32 = 1
    messages[1].optional_int64 = 2
    messages[2].optional_int32 = 3
    messages[2].optional_string = 'hello'

    merged_message = unittest_pb2.testalltypes()
    merged_message.optional_int32 = 3
    merged_message.optional_int64 = 2
    merged_message.optional_string = 'hello'

    generator = unittest_pb2.testparsingmerge.repeatedfieldsgenerator()
    generator.field1.extend(messages)
    generator.field2.extend(messages)
    generator.field3.extend(messages)
    generator.ext1.extend(messages)
    generator.ext2.extend(messages)
    generator.group1.add().field1.mergefrom(messages[0])
    generator.group1.add().field1.mergefrom(messages[1])
    generator.group1.add().field1.mergefrom(messages[2])
    generator.group2.add().field1.mergefrom(messages[0])
    generator.group2.add().field1.mergefrom(messages[1])
    generator.group2.add().field1.mergefrom(messages[2])

    data = generator.serializetostring()
    parsing_merge = unittest_pb2.testparsingmerge()
    parsing_merge.parsefromstring(data)

    # required and optional fields should be merged.
    self.assertequal(parsing_merge.required_all_types, merged_message)
    self.assertequal(parsing_merge.optional_all_types, merged_message)
    self.assertequal(parsing_merge.optionalgroup.optional_group_all_types,
                     merged_message)
    self.assertequal(parsing_merge.extensions[
                     unittest_pb2.testparsingmerge.optional_ext],
                     merged_message)

    # repeated fields should not be merged.
    self.assertequal(len(parsing_merge.repeated_all_types), 3)
    self.assertequal(len(parsing_merge.repeatedgroup), 3)
    self.assertequal(len(parsing_merge.extensions[
        unittest_pb2.testparsingmerge.repeated_ext]), 3)


  def testsortemptyrepeatedcompositecontainer(self):
    """exercise a scenario that has led to segfaults in the past.
    """
    m = unittest_pb2.testalltypes()
    m.repeated_nested_message.sort()


if __name__ == '__main__':
  unittest.main()
