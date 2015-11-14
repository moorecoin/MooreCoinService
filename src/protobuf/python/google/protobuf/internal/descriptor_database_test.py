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

"""tests for google.protobuf.descriptor_database."""

__author__ = 'matthewtoia@google.com (matt toia)'

import unittest
from google.protobuf import descriptor_pb2
from google.protobuf.internal import factory_test2_pb2
from google.protobuf import descriptor_database


class descriptordatabasetest(unittest.testcase):

  def testadd(self):
    db = descriptor_database.descriptordatabase()
    file_desc_proto = descriptor_pb2.filedescriptorproto.fromstring(
        factory_test2_pb2.descriptor.serialized_pb)
    db.add(file_desc_proto)

    self.assertequals(file_desc_proto, db.findfilebyname(
        'net/proto2/python/internal/factory_test2.proto'))
    self.assertequals(file_desc_proto, db.findfilecontainingsymbol(
        'net.proto2.python.internal.factory2message'))
    self.assertequals(file_desc_proto, db.findfilecontainingsymbol(
        'net.proto2.python.internal.factory2message.nestedfactory2message'))
    self.assertequals(file_desc_proto, db.findfilecontainingsymbol(
        'net.proto2.python.internal.factory2enum'))
    self.assertequals(file_desc_proto, db.findfilecontainingsymbol(
        'net.proto2.python.internal.factory2message.nestedfactory2enum'))

if __name__ == '__main__':
  unittest.main()
