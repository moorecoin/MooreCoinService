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

"""tests for google.protobuf.message_factory."""

__author__ = 'matthewtoia@google.com (matt toia)'

import unittest
from google.protobuf import descriptor_pb2
from google.protobuf.internal import factory_test1_pb2
from google.protobuf.internal import factory_test2_pb2
from google.protobuf import descriptor_database
from google.protobuf import descriptor_pool
from google.protobuf import message_factory


class messagefactorytest(unittest.testcase):

  def setup(self):
    self.factory_test1_fd = descriptor_pb2.filedescriptorproto.fromstring(
        factory_test1_pb2.descriptor.serialized_pb)
    self.factory_test2_fd = descriptor_pb2.filedescriptorproto.fromstring(
        factory_test2_pb2.descriptor.serialized_pb)

  def _exercisedynamicclass(self, cls):
    msg = cls()
    msg.mandatory = 42
    msg.nested_factory_2_enum = 0
    msg.nested_factory_2_message.value = 'nested message value'
    msg.factory_1_message.factory_1_enum = 1
    msg.factory_1_message.nested_factory_1_enum = 0
    msg.factory_1_message.nested_factory_1_message.value = (
        'nested message value')
    msg.factory_1_message.scalar_value = 22
    msg.factory_1_message.list_value.extend(['one', 'two', 'three'])
    msg.factory_1_message.list_value.append('four')
    msg.factory_1_enum = 1
    msg.nested_factory_1_enum = 0
    msg.nested_factory_1_message.value = 'nested message value'
    msg.circular_message.mandatory = 1
    msg.circular_message.circular_message.mandatory = 2
    msg.circular_message.scalar_value = 'one deep'
    msg.scalar_value = 'zero deep'
    msg.list_value.extend(['four', 'three', 'two'])
    msg.list_value.append('one')
    msg.grouped.add()
    msg.grouped[0].part_1 = 'hello'
    msg.grouped[0].part_2 = 'world'
    msg.grouped.add(part_1='testing', part_2='123')
    msg.loop.loop.mandatory = 2
    msg.loop.loop.loop.loop.mandatory = 4
    serialized = msg.serializetostring()
    converted = factory_test2_pb2.factory2message.fromstring(serialized)
    reserialized = converted.serializetostring()
    self.assertequals(serialized, reserialized)
    result = cls.fromstring(reserialized)
    self.assertequals(msg, result)

  def testgetprototype(self):
    db = descriptor_database.descriptordatabase()
    pool = descriptor_pool.descriptorpool(db)
    db.add(self.factory_test1_fd)
    db.add(self.factory_test2_fd)
    factory = message_factory.messagefactory()
    cls = factory.getprototype(pool.findmessagetypebyname(
        'net.proto2.python.internal.factory2message'))
    self.assertisnot(cls, factory_test2_pb2.factory2message)
    self._exercisedynamicclass(cls)
    cls2 = factory.getprototype(pool.findmessagetypebyname(
        'net.proto2.python.internal.factory2message'))
    self.assertis(cls, cls2)

  def testgetmessages(self):
    messages = message_factory.getmessages([self.factory_test2_fd,
                                            self.factory_test1_fd])
    self.assertcontainssubset(
        ['net.proto2.python.internal.factory2message',
         'net.proto2.python.internal.factory1message'],
        messages.keys())
    self._exercisedynamicclass(
        messages['net.proto2.python.internal.factory2message'])

if __name__ == '__main__':
  unittest.main()
