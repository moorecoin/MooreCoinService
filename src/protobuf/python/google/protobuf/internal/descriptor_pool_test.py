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

"""tests for google.protobuf.descriptor_pool."""

__author__ = 'matthewtoia@google.com (matt toia)'

import unittest
from google.protobuf import descriptor_pb2
from google.protobuf.internal import factory_test1_pb2
from google.protobuf.internal import factory_test2_pb2
from google.protobuf import descriptor
from google.protobuf import descriptor_database
from google.protobuf import descriptor_pool


class descriptorpooltest(unittest.testcase):

  def setup(self):
    self.pool = descriptor_pool.descriptorpool()
    self.factory_test1_fd = descriptor_pb2.filedescriptorproto.fromstring(
        factory_test1_pb2.descriptor.serialized_pb)
    self.factory_test2_fd = descriptor_pb2.filedescriptorproto.fromstring(
        factory_test2_pb2.descriptor.serialized_pb)
    self.pool.add(self.factory_test1_fd)
    self.pool.add(self.factory_test2_fd)

  def testfindfilebyname(self):
    name1 = 'net/proto2/python/internal/factory_test1.proto'
    file_desc1 = self.pool.findfilebyname(name1)
    self.assertisinstance(file_desc1, descriptor.filedescriptor)
    self.assertequals(name1, file_desc1.name)
    self.assertequals('net.proto2.python.internal', file_desc1.package)
    self.assertin('factory1message', file_desc1.message_types_by_name)

    name2 = 'net/proto2/python/internal/factory_test2.proto'
    file_desc2 = self.pool.findfilebyname(name2)
    self.assertisinstance(file_desc2, descriptor.filedescriptor)
    self.assertequals(name2, file_desc2.name)
    self.assertequals('net.proto2.python.internal', file_desc2.package)
    self.assertin('factory2message', file_desc2.message_types_by_name)

  def testfindfilebynamefailure(self):
    try:
      self.pool.findfilebyname('does not exist')
      self.fail('expected keyerror')
    except keyerror:
      pass

  def testfindfilecontainingsymbol(self):
    file_desc1 = self.pool.findfilecontainingsymbol(
        'net.proto2.python.internal.factory1message')
    self.assertisinstance(file_desc1, descriptor.filedescriptor)
    self.assertequals('net/proto2/python/internal/factory_test1.proto',
                      file_desc1.name)
    self.assertequals('net.proto2.python.internal', file_desc1.package)
    self.assertin('factory1message', file_desc1.message_types_by_name)

    file_desc2 = self.pool.findfilecontainingsymbol(
        'net.proto2.python.internal.factory2message')
    self.assertisinstance(file_desc2, descriptor.filedescriptor)
    self.assertequals('net/proto2/python/internal/factory_test2.proto',
                      file_desc2.name)
    self.assertequals('net.proto2.python.internal', file_desc2.package)
    self.assertin('factory2message', file_desc2.message_types_by_name)

  def testfindfilecontainingsymbolfailure(self):
    try:
      self.pool.findfilecontainingsymbol('does not exist')
      self.fail('expected keyerror')
    except keyerror:
      pass

  def testfindmessagetypebyname(self):
    msg1 = self.pool.findmessagetypebyname(
        'net.proto2.python.internal.factory1message')
    self.assertisinstance(msg1, descriptor.descriptor)
    self.assertequals('factory1message', msg1.name)
    self.assertequals('net.proto2.python.internal.factory1message',
                      msg1.full_name)
    self.assertequals(none, msg1.containing_type)

    nested_msg1 = msg1.nested_types[0]
    self.assertequals('nestedfactory1message', nested_msg1.name)
    self.assertequals(msg1, nested_msg1.containing_type)

    nested_enum1 = msg1.enum_types[0]
    self.assertequals('nestedfactory1enum', nested_enum1.name)
    self.assertequals(msg1, nested_enum1.containing_type)

    self.assertequals(nested_msg1, msg1.fields_by_name[
        'nested_factory_1_message'].message_type)
    self.assertequals(nested_enum1, msg1.fields_by_name[
        'nested_factory_1_enum'].enum_type)

    msg2 = self.pool.findmessagetypebyname(
        'net.proto2.python.internal.factory2message')
    self.assertisinstance(msg2, descriptor.descriptor)
    self.assertequals('factory2message', msg2.name)
    self.assertequals('net.proto2.python.internal.factory2message',
                      msg2.full_name)
    self.assertisnone(msg2.containing_type)

    nested_msg2 = msg2.nested_types[0]
    self.assertequals('nestedfactory2message', nested_msg2.name)
    self.assertequals(msg2, nested_msg2.containing_type)

    nested_enum2 = msg2.enum_types[0]
    self.assertequals('nestedfactory2enum', nested_enum2.name)
    self.assertequals(msg2, nested_enum2.containing_type)

    self.assertequals(nested_msg2, msg2.fields_by_name[
        'nested_factory_2_message'].message_type)
    self.assertequals(nested_enum2, msg2.fields_by_name[
        'nested_factory_2_enum'].enum_type)

    self.asserttrue(msg2.fields_by_name['int_with_default'].has_default)
    self.assertequals(
        1776, msg2.fields_by_name['int_with_default'].default_value)

    self.asserttrue(msg2.fields_by_name['double_with_default'].has_default)
    self.assertequals(
        9.99, msg2.fields_by_name['double_with_default'].default_value)

    self.asserttrue(msg2.fields_by_name['string_with_default'].has_default)
    self.assertequals(
        'hello world', msg2.fields_by_name['string_with_default'].default_value)

    self.asserttrue(msg2.fields_by_name['bool_with_default'].has_default)
    self.assertfalse(msg2.fields_by_name['bool_with_default'].default_value)

    self.asserttrue(msg2.fields_by_name['enum_with_default'].has_default)
    self.assertequals(
        1, msg2.fields_by_name['enum_with_default'].default_value)

    msg3 = self.pool.findmessagetypebyname(
        'net.proto2.python.internal.factory2message.nestedfactory2message')
    self.assertequals(nested_msg2, msg3)

  def testfindmessagetypebynamefailure(self):
    try:
      self.pool.findmessagetypebyname('does not exist')
      self.fail('expected keyerror')
    except keyerror:
      pass

  def testfindenumtypebyname(self):
    enum1 = self.pool.findenumtypebyname(
        'net.proto2.python.internal.factory1enum')
    self.assertisinstance(enum1, descriptor.enumdescriptor)
    self.assertequals(0, enum1.values_by_name['factory_1_value_0'].number)
    self.assertequals(1, enum1.values_by_name['factory_1_value_1'].number)

    nested_enum1 = self.pool.findenumtypebyname(
        'net.proto2.python.internal.factory1message.nestedfactory1enum')
    self.assertisinstance(nested_enum1, descriptor.enumdescriptor)
    self.assertequals(
        0, nested_enum1.values_by_name['nested_factory_1_value_0'].number)
    self.assertequals(
        1, nested_enum1.values_by_name['nested_factory_1_value_1'].number)

    enum2 = self.pool.findenumtypebyname(
        'net.proto2.python.internal.factory2enum')
    self.assertisinstance(enum2, descriptor.enumdescriptor)
    self.assertequals(0, enum2.values_by_name['factory_2_value_0'].number)
    self.assertequals(1, enum2.values_by_name['factory_2_value_1'].number)

    nested_enum2 = self.pool.findenumtypebyname(
        'net.proto2.python.internal.factory2message.nestedfactory2enum')
    self.assertisinstance(nested_enum2, descriptor.enumdescriptor)
    self.assertequals(
        0, nested_enum2.values_by_name['nested_factory_2_value_0'].number)
    self.assertequals(
        1, nested_enum2.values_by_name['nested_factory_2_value_1'].number)

  def testfindenumtypebynamefailure(self):
    try:
      self.pool.findenumtypebyname('does not exist')
      self.fail('expected keyerror')
    except keyerror:
      pass

  def testuserdefineddb(self):
    db = descriptor_database.descriptordatabase()
    self.pool = descriptor_pool.descriptorpool(db)
    db.add(self.factory_test1_fd)
    db.add(self.factory_test2_fd)
    self.testfindmessagetypebyname()

if __name__ == '__main__':
  unittest.main()
