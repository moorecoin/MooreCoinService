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

"""test for google.protobuf.internal.wire_format."""

__author__ = 'robinson@google.com (will robinson)'

import unittest
from google.protobuf import message
from google.protobuf.internal import wire_format


class wireformattest(unittest.testcase):

  def testpacktag(self):
    field_number = 0xabc
    tag_type = 2
    self.assertequal((field_number << 3) | tag_type,
                     wire_format.packtag(field_number, tag_type))
    packtag = wire_format.packtag
    # number too high.
    self.assertraises(message.encodeerror, packtag, field_number, 6)
    # number too low.
    self.assertraises(message.encodeerror, packtag, field_number, -1)

  def testunpacktag(self):
    # test field numbers that will require various varint sizes.
    for expected_field_number in (1, 15, 16, 2047, 2048):
      for expected_wire_type in range(6):  # highest-numbered wiretype is 5.
        field_number, wire_type = wire_format.unpacktag(
            wire_format.packtag(expected_field_number, expected_wire_type))
        self.assertequal(expected_field_number, field_number)
        self.assertequal(expected_wire_type, wire_type)

    self.assertraises(typeerror, wire_format.unpacktag, none)
    self.assertraises(typeerror, wire_format.unpacktag, 'abc')
    self.assertraises(typeerror, wire_format.unpacktag, 0.0)
    self.assertraises(typeerror, wire_format.unpacktag, object())

  def testzigzagencode(self):
    z = wire_format.zigzagencode
    self.assertequal(0, z(0))
    self.assertequal(1, z(-1))
    self.assertequal(2, z(1))
    self.assertequal(3, z(-2))
    self.assertequal(4, z(2))
    self.assertequal(0xfffffffe, z(0x7fffffff))
    self.assertequal(0xffffffff, z(-0x80000000))
    self.assertequal(0xfffffffffffffffe, z(0x7fffffffffffffff))
    self.assertequal(0xffffffffffffffff, z(-0x8000000000000000))

    self.assertraises(typeerror, z, none)
    self.assertraises(typeerror, z, 'abcd')
    self.assertraises(typeerror, z, 0.0)
    self.assertraises(typeerror, z, object())

  def testzigzagdecode(self):
    z = wire_format.zigzagdecode
    self.assertequal(0, z(0))
    self.assertequal(-1, z(1))
    self.assertequal(1, z(2))
    self.assertequal(-2, z(3))
    self.assertequal(2, z(4))
    self.assertequal(0x7fffffff, z(0xfffffffe))
    self.assertequal(-0x80000000, z(0xffffffff))
    self.assertequal(0x7fffffffffffffff, z(0xfffffffffffffffe))
    self.assertequal(-0x8000000000000000, z(0xffffffffffffffff))

    self.assertraises(typeerror, z, none)
    self.assertraises(typeerror, z, 'abcd')
    self.assertraises(typeerror, z, 0.0)
    self.assertraises(typeerror, z, object())

  def numericbytesizetesthelper(self, byte_size_fn, value, expected_value_size):
    # use field numbers that cause various byte sizes for the tag information.
    for field_number, tag_bytes in ((15, 1), (16, 2), (2047, 2), (2048, 3)):
      expected_size = expected_value_size + tag_bytes
      actual_size = byte_size_fn(field_number, value)
      self.assertequal(expected_size, actual_size,
                       'byte_size_fn: %s, field_number: %d, value: %r\n'
                       'expected: %d, actual: %d'% (
          byte_size_fn, field_number, value, expected_size, actual_size))

  def testbytesizefunctions(self):
    # test all numeric *bytesize() functions.
    numeric_args = [
        # int32bytesize().
        [wire_format.int32bytesize, 0, 1],
        [wire_format.int32bytesize, 127, 1],
        [wire_format.int32bytesize, 128, 2],
        [wire_format.int32bytesize, -1, 10],
        # int64bytesize().
        [wire_format.int64bytesize, 0, 1],
        [wire_format.int64bytesize, 127, 1],
        [wire_format.int64bytesize, 128, 2],
        [wire_format.int64bytesize, -1, 10],
        # uint32bytesize().
        [wire_format.uint32bytesize, 0, 1],
        [wire_format.uint32bytesize, 127, 1],
        [wire_format.uint32bytesize, 128, 2],
        [wire_format.uint32bytesize, wire_format.uint32_max, 5],
        # uint64bytesize().
        [wire_format.uint64bytesize, 0, 1],
        [wire_format.uint64bytesize, 127, 1],
        [wire_format.uint64bytesize, 128, 2],
        [wire_format.uint64bytesize, wire_format.uint64_max, 10],
        # sint32bytesize().
        [wire_format.sint32bytesize, 0, 1],
        [wire_format.sint32bytesize, -1, 1],
        [wire_format.sint32bytesize, 1, 1],
        [wire_format.sint32bytesize, -63, 1],
        [wire_format.sint32bytesize, 63, 1],
        [wire_format.sint32bytesize, -64, 1],
        [wire_format.sint32bytesize, 64, 2],
        # sint64bytesize().
        [wire_format.sint64bytesize, 0, 1],
        [wire_format.sint64bytesize, -1, 1],
        [wire_format.sint64bytesize, 1, 1],
        [wire_format.sint64bytesize, -63, 1],
        [wire_format.sint64bytesize, 63, 1],
        [wire_format.sint64bytesize, -64, 1],
        [wire_format.sint64bytesize, 64, 2],
        # fixed32bytesize().
        [wire_format.fixed32bytesize, 0, 4],
        [wire_format.fixed32bytesize, wire_format.uint32_max, 4],
        # fixed64bytesize().
        [wire_format.fixed64bytesize, 0, 8],
        [wire_format.fixed64bytesize, wire_format.uint64_max, 8],
        # sfixed32bytesize().
        [wire_format.sfixed32bytesize, 0, 4],
        [wire_format.sfixed32bytesize, wire_format.int32_min, 4],
        [wire_format.sfixed32bytesize, wire_format.int32_max, 4],
        # sfixed64bytesize().
        [wire_format.sfixed64bytesize, 0, 8],
        [wire_format.sfixed64bytesize, wire_format.int64_min, 8],
        [wire_format.sfixed64bytesize, wire_format.int64_max, 8],
        # floatbytesize().
        [wire_format.floatbytesize, 0.0, 4],
        [wire_format.floatbytesize, 1000000000.0, 4],
        [wire_format.floatbytesize, -1000000000.0, 4],
        # doublebytesize().
        [wire_format.doublebytesize, 0.0, 8],
        [wire_format.doublebytesize, 1000000000.0, 8],
        [wire_format.doublebytesize, -1000000000.0, 8],
        # boolbytesize().
        [wire_format.boolbytesize, false, 1],
        [wire_format.boolbytesize, true, 1],
        # enumbytesize().
        [wire_format.enumbytesize, 0, 1],
        [wire_format.enumbytesize, 127, 1],
        [wire_format.enumbytesize, 128, 2],
        [wire_format.enumbytesize, wire_format.uint32_max, 5],
        ]
    for args in numeric_args:
      self.numericbytesizetesthelper(*args)

    # test strings and bytes.
    for byte_size_fn in (wire_format.stringbytesize, wire_format.bytesbytesize):
      # 1 byte for tag, 1 byte for length, 3 bytes for contents.
      self.assertequal(5, byte_size_fn(10, 'abc'))
      # 2 bytes for tag, 1 byte for length, 3 bytes for contents.
      self.assertequal(6, byte_size_fn(16, 'abc'))
      # 2 bytes for tag, 2 bytes for length, 128 bytes for contents.
      self.assertequal(132, byte_size_fn(16, 'a' * 128))

    # test utf-8 string byte size calculation.
    # 1 byte for tag, 1 byte for length, 8 bytes for content.
    self.assertequal(10, wire_format.stringbytesize(
        5, unicode('\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82', 'utf-8')))

    class mockmessage(object):
      def __init__(self, byte_size):
        self.byte_size = byte_size
      def bytesize(self):
        return self.byte_size

    message_byte_size = 10
    mock_message = mockmessage(byte_size=message_byte_size)
    # test groups.
    # (2 * 1) bytes for begin and end tags, plus message_byte_size.
    self.assertequal(2 + message_byte_size,
                     wire_format.groupbytesize(1, mock_message))
    # (2 * 2) bytes for begin and end tags, plus message_byte_size.
    self.assertequal(4 + message_byte_size,
                     wire_format.groupbytesize(16, mock_message))

    # test messages.
    # 1 byte for tag, plus 1 byte for length, plus contents.
    self.assertequal(2 + mock_message.byte_size,
                     wire_format.messagebytesize(1, mock_message))
    # 2 bytes for tag, plus 1 byte for length, plus contents.
    self.assertequal(3 + mock_message.byte_size,
                     wire_format.messagebytesize(16, mock_message))
    # 2 bytes for tag, plus 2 bytes for length, plus contents.
    mock_message.byte_size = 128
    self.assertequal(4 + mock_message.byte_size,
                     wire_format.messagebytesize(16, mock_message))


    # test message set item byte size.
    # 4 bytes for tags, plus 1 byte for length, plus 1 byte for type_id,
    # plus contents.
    mock_message.byte_size = 10
    self.assertequal(mock_message.byte_size + 6,
                     wire_format.messagesetitembytesize(1, mock_message))

    # 4 bytes for tags, plus 2 bytes for length, plus 1 byte for type_id,
    # plus contents.
    mock_message.byte_size = 128
    self.assertequal(mock_message.byte_size + 7,
                     wire_format.messagesetitembytesize(1, mock_message))

    # 4 bytes for tags, plus 2 bytes for length, plus 2 byte for type_id,
    # plus contents.
    self.assertequal(mock_message.byte_size + 8,
                     wire_format.messagesetitembytesize(128, mock_message))

    # too-long varint.
    self.assertraises(message.encodeerror,
                      wire_format.uint64bytesize, 1, 1 << 128)


if __name__ == '__main__':
  unittest.main()
