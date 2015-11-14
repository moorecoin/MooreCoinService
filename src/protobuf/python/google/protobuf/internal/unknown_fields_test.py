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

"""test for preservation of unknown fields in the pure python implementation."""

__author__ = 'bohdank@google.com (bohdan koval)'

import unittest
from google.protobuf import unittest_mset_pb2
from google.protobuf import unittest_pb2
from google.protobuf.internal import encoder
from google.protobuf.internal import test_util
from google.protobuf.internal import type_checkers


class unknownfieldstest(unittest.testcase):

  def setup(self):
    self.descriptor = unittest_pb2.testalltypes.descriptor
    self.all_fields = unittest_pb2.testalltypes()
    test_util.setallfields(self.all_fields)
    self.all_fields_data = self.all_fields.serializetostring()
    self.empty_message = unittest_pb2.testemptymessage()
    self.empty_message.parsefromstring(self.all_fields_data)
    self.unknown_fields = self.empty_message._unknown_fields

  def getfield(self, name):
    field_descriptor = self.descriptor.fields_by_name[name]
    wire_type = type_checkers.field_type_to_wire_type[field_descriptor.type]
    field_tag = encoder.tagbytes(field_descriptor.number, wire_type)
    for tag_bytes, value in self.unknown_fields:
      if tag_bytes == field_tag:
        decoder = unittest_pb2.testalltypes._decoders_by_tag[tag_bytes]
        result_dict = {}
        decoder(value, 0, len(value), self.all_fields, result_dict)
        return result_dict[field_descriptor]

  def testvarint(self):
    value = self.getfield('optional_int32')
    self.assertequal(self.all_fields.optional_int32, value)

  def testfixed32(self):
    value = self.getfield('optional_fixed32')
    self.assertequal(self.all_fields.optional_fixed32, value)

  def testfixed64(self):
    value = self.getfield('optional_fixed64')
    self.assertequal(self.all_fields.optional_fixed64, value)

  def testlengthdelimited(self):
    value = self.getfield('optional_string')
    self.assertequal(self.all_fields.optional_string, value)

  def testgroup(self):
    value = self.getfield('optionalgroup')
    self.assertequal(self.all_fields.optionalgroup, value)

  def testserialize(self):
    data = self.empty_message.serializetostring()

    # don't use assertequal because we don't want to dump raw binary data to
    # stdout.
    self.asserttrue(data == self.all_fields_data)

  def testcopyfrom(self):
    message = unittest_pb2.testemptymessage()
    message.copyfrom(self.empty_message)
    self.assertequal(self.unknown_fields, message._unknown_fields)

  def testmergefrom(self):
    message = unittest_pb2.testalltypes()
    message.optional_int32 = 1
    message.optional_uint32 = 2
    source = unittest_pb2.testemptymessage()
    source.parsefromstring(message.serializetostring())

    message.clearfield('optional_int32')
    message.optional_int64 = 3
    message.optional_uint32 = 4
    destination = unittest_pb2.testemptymessage()
    destination.parsefromstring(message.serializetostring())
    unknown_fields = destination._unknown_fields[:]

    destination.mergefrom(source)
    self.assertequal(unknown_fields + source._unknown_fields,
                     destination._unknown_fields)

  def testclear(self):
    self.empty_message.clear()
    self.assertequal(0, len(self.empty_message._unknown_fields))

  def testbytesize(self):
    self.assertequal(self.all_fields.bytesize(), self.empty_message.bytesize())

  def testunknownextensions(self):
    message = unittest_pb2.testemptymessagewithextensions()
    message.parsefromstring(self.all_fields_data)
    self.assertequal(self.empty_message._unknown_fields,
                     message._unknown_fields)

  def testlistfields(self):
    # make sure listfields doesn't return unknown fields.
    self.assertequal(0, len(self.empty_message.listfields()))

  def testserializemessagesetwireformatunknownextension(self):
    # create a message using the message set wire format with an unknown
    # message.
    raw = unittest_mset_pb2.rawmessageset()

    # add an unknown extension.
    item = raw.item.add()
    item.type_id = 1545009
    message1 = unittest_mset_pb2.testmessagesetextension1()
    message1.i = 12345
    item.message = message1.serializetostring()

    serialized = raw.serializetostring()

    # parse message using the message set wire format.
    proto = unittest_mset_pb2.testmessageset()
    proto.mergefromstring(serialized)

    # verify that the unknown extension is serialized unchanged
    reserialized = proto.serializetostring()
    new_raw = unittest_mset_pb2.rawmessageset()
    new_raw.mergefromstring(reserialized)
    self.assertequal(raw, new_raw)

  def testequals(self):
    message = unittest_pb2.testemptymessage()
    message.parsefromstring(self.all_fields_data)
    self.assertequal(self.empty_message, message)

    self.all_fields.clearfield('optional_string')
    message.parsefromstring(self.all_fields.serializetostring())
    self.assertnotequal(self.empty_message, message)


if __name__ == '__main__':
  unittest.main()
