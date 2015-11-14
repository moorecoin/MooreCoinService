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

"""unittest for reflection.py, which tests the generated c++ implementation."""

__author__ = 'jasonh@google.com (jason hsueh)'

import os
os.environ['protocol_buffers_python_implementation'] = 'cpp'

import unittest
from google.protobuf.internal import api_implementation
from google.protobuf.internal import more_extensions_dynamic_pb2
from google.protobuf.internal import more_extensions_pb2
from google.protobuf.internal.reflection_test import *


class reflectioncpptest(unittest.testcase):
  def testimplementationsetting(self):
    self.assertequal('cpp', api_implementation.type())

  def testextensionofgeneratedtypeindynamicfile(self):
    """tests that a file built dynamically can extend a generated c++ type.

    the c++ implementation uses a descriptorpool that has the generated
    descriptorpool as an underlay. typically, a type can only find
    extensions in its own pool. with the python c-extension, the generated c++
    extendee may be available, but not the extension. this tests that the
    c-extension implements the correct special handling to make such extensions
    available.
    """
    pb1 = more_extensions_pb2.extendedmessage()
    # test that basic accessors work.
    self.assertfalse(
        pb1.hasextension(more_extensions_dynamic_pb2.dynamic_int32_extension))
    self.assertfalse(
        pb1.hasextension(more_extensions_dynamic_pb2.dynamic_message_extension))
    pb1.extensions[more_extensions_dynamic_pb2.dynamic_int32_extension] = 17
    pb1.extensions[more_extensions_dynamic_pb2.dynamic_message_extension].a = 24
    self.asserttrue(
        pb1.hasextension(more_extensions_dynamic_pb2.dynamic_int32_extension))
    self.asserttrue(
        pb1.hasextension(more_extensions_dynamic_pb2.dynamic_message_extension))

    # now serialize the data and parse to a new message.
    pb2 = more_extensions_pb2.extendedmessage()
    pb2.mergefromstring(pb1.serializetostring())

    self.asserttrue(
        pb2.hasextension(more_extensions_dynamic_pb2.dynamic_int32_extension))
    self.asserttrue(
        pb2.hasextension(more_extensions_dynamic_pb2.dynamic_message_extension))
    self.assertequal(
        17, pb2.extensions[more_extensions_dynamic_pb2.dynamic_int32_extension])
    self.assertequal(
        24,
        pb2.extensions[more_extensions_dynamic_pb2.dynamic_message_extension].a)


if __name__ == '__main__':
  unittest.main()
