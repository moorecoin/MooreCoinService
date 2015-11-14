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

# todo(robinson): flesh this out considerably.  we focused on reflection_test.py
# first, since it's testing the subtler code, and since it provides decent
# indirect testing of the protocol compiler output.

"""unittest that directly tests the output of the pure-python protocol
compiler.  see //google/protobuf/reflection_test.py for a test which
further ensures that we can use python protocol message objects as we expect.
"""

__author__ = 'robinson@google.com (will robinson)'

import unittest
from google.protobuf.internal import test_bad_identifiers_pb2
from google.protobuf import unittest_custom_options_pb2
from google.protobuf import unittest_import_pb2
from google.protobuf import unittest_import_public_pb2
from google.protobuf import unittest_mset_pb2
from google.protobuf import unittest_pb2
from google.protobuf import unittest_no_generic_services_pb2
from google.protobuf import service

max_extension = 536870912


class generatortest(unittest.testcase):

  def testnestedmessagedescriptor(self):
    field_name = 'optional_nested_message'
    proto_type = unittest_pb2.testalltypes
    self.assertequal(
        proto_type.nestedmessage.descriptor,
        proto_type.descriptor.fields_by_name[field_name].message_type)

  def testenums(self):
    # we test only module-level enums here.
    # todo(robinson): examine descriptors directly to check
    # enum descriptor output.
    self.assertequal(4, unittest_pb2.foreign_foo)
    self.assertequal(5, unittest_pb2.foreign_bar)
    self.assertequal(6, unittest_pb2.foreign_baz)

    proto = unittest_pb2.testalltypes()
    self.assertequal(1, proto.foo)
    self.assertequal(1, unittest_pb2.testalltypes.foo)
    self.assertequal(2, proto.bar)
    self.assertequal(2, unittest_pb2.testalltypes.bar)
    self.assertequal(3, proto.baz)
    self.assertequal(3, unittest_pb2.testalltypes.baz)

  def testextremedefaultvalues(self):
    message = unittest_pb2.testextremedefaultvalues()

    # python pre-2.6 does not have isinf() or isnan() functions, so we have
    # to provide our own.
    def isnan(val):
      # nan is never equal to itself.
      return val != val
    def isinf(val):
      # infinity times zero equals nan.
      return not isnan(val) and isnan(val * 0)

    self.asserttrue(isinf(message.inf_double))
    self.asserttrue(message.inf_double > 0)
    self.asserttrue(isinf(message.neg_inf_double))
    self.asserttrue(message.neg_inf_double < 0)
    self.asserttrue(isnan(message.nan_double))

    self.asserttrue(isinf(message.inf_float))
    self.asserttrue(message.inf_float > 0)
    self.asserttrue(isinf(message.neg_inf_float))
    self.asserttrue(message.neg_inf_float < 0)
    self.asserttrue(isnan(message.nan_float))
    self.assertequal("? ? ?? ?? ??? ??/ ??-", message.cpp_trigraph)

  def testhasdefaultvalues(self):
    desc = unittest_pb2.testalltypes.descriptor

    expected_has_default_by_name = {
        'optional_int32': false,
        'repeated_int32': false,
        'optional_nested_message': false,
        'default_int32': true,
    }

    has_default_by_name = dict(
        [(f.name, f.has_default_value)
         for f in desc.fields
         if f.name in expected_has_default_by_name])
    self.assertequal(expected_has_default_by_name, has_default_by_name)

  def testcontainingtypebehaviorforextensions(self):
    self.assertequal(unittest_pb2.optional_int32_extension.containing_type,
                     unittest_pb2.testallextensions.descriptor)
    self.assertequal(unittest_pb2.testrequired.single.containing_type,
                     unittest_pb2.testallextensions.descriptor)

  def testextensionscope(self):
    self.assertequal(unittest_pb2.optional_int32_extension.extension_scope,
                     none)
    self.assertequal(unittest_pb2.testrequired.single.extension_scope,
                     unittest_pb2.testrequired.descriptor)

  def testisextension(self):
    self.asserttrue(unittest_pb2.optional_int32_extension.is_extension)
    self.asserttrue(unittest_pb2.testrequired.single.is_extension)

    message_descriptor = unittest_pb2.testrequired.descriptor
    non_extension_descriptor = message_descriptor.fields_by_name['a']
    self.asserttrue(not non_extension_descriptor.is_extension)

  def testoptions(self):
    proto = unittest_mset_pb2.testmessageset()
    self.asserttrue(proto.descriptor.getoptions().message_set_wire_format)

  def testmessagewithcustomoptions(self):
    proto = unittest_custom_options_pb2.testmessagewithcustomoptions()
    enum_options = proto.descriptor.enum_types_by_name['anenum'].getoptions()
    self.asserttrue(enum_options is not none)
    # todo(gps): we really should test for the presense of the enum_opt1
    # extension and for its value to be set to -789.

  def testnestedtypes(self):
    self.assertequals(
        set(unittest_pb2.testalltypes.descriptor.nested_types),
        set([
            unittest_pb2.testalltypes.nestedmessage.descriptor,
            unittest_pb2.testalltypes.optionalgroup.descriptor,
            unittest_pb2.testalltypes.repeatedgroup.descriptor,
        ]))
    self.assertequal(unittest_pb2.testemptymessage.descriptor.nested_types, [])
    self.assertequal(
        unittest_pb2.testalltypes.nestedmessage.descriptor.nested_types, [])

  def testcontainingtype(self):
    self.asserttrue(
        unittest_pb2.testemptymessage.descriptor.containing_type is none)
    self.asserttrue(
        unittest_pb2.testalltypes.descriptor.containing_type is none)
    self.assertequal(
        unittest_pb2.testalltypes.nestedmessage.descriptor.containing_type,
        unittest_pb2.testalltypes.descriptor)
    self.assertequal(
        unittest_pb2.testalltypes.nestedmessage.descriptor.containing_type,
        unittest_pb2.testalltypes.descriptor)
    self.assertequal(
        unittest_pb2.testalltypes.repeatedgroup.descriptor.containing_type,
        unittest_pb2.testalltypes.descriptor)

  def testcontainingtypeinenumdescriptor(self):
    self.asserttrue(unittest_pb2._foreignenum.containing_type is none)
    self.assertequal(unittest_pb2._testalltypes_nestedenum.containing_type,
                     unittest_pb2.testalltypes.descriptor)

  def testpackage(self):
    self.assertequal(
        unittest_pb2.testalltypes.descriptor.file.package,
        'protobuf_unittest')
    desc = unittest_pb2.testalltypes.nestedmessage.descriptor
    self.assertequal(desc.file.package, 'protobuf_unittest')
    self.assertequal(
        unittest_import_pb2.importmessage.descriptor.file.package,
        'protobuf_unittest_import')

    self.assertequal(
        unittest_pb2._foreignenum.file.package, 'protobuf_unittest')
    self.assertequal(
        unittest_pb2._testalltypes_nestedenum.file.package,
        'protobuf_unittest')
    self.assertequal(
        unittest_import_pb2._importenum.file.package,
        'protobuf_unittest_import')

  def testextensionrange(self):
    self.assertequal(
        unittest_pb2.testalltypes.descriptor.extension_ranges, [])
    self.assertequal(
        unittest_pb2.testallextensions.descriptor.extension_ranges,
        [(1, max_extension)])
    self.assertequal(
        unittest_pb2.testmultipleextensionranges.descriptor.extension_ranges,
        [(42, 43), (4143, 4244), (65536, max_extension)])

  def testfiledescriptor(self):
    self.assertequal(unittest_pb2.descriptor.name,
                     'google/protobuf/unittest.proto')
    self.assertequal(unittest_pb2.descriptor.package, 'protobuf_unittest')
    self.assertfalse(unittest_pb2.descriptor.serialized_pb is none)

  def testnogenericservices(self):
    self.asserttrue(hasattr(unittest_no_generic_services_pb2, "testmessage"))
    self.asserttrue(hasattr(unittest_no_generic_services_pb2, "foo"))
    self.asserttrue(hasattr(unittest_no_generic_services_pb2, "test_extension"))

    # make sure unittest_no_generic_services_pb2 has no services subclassing
    # proto2 service class.
    if hasattr(unittest_no_generic_services_pb2, "testservice"):
      self.assertfalse(issubclass(unittest_no_generic_services_pb2.testservice,
                                  service.service))

  def testmessagetypesbyname(self):
    file_type = unittest_pb2.descriptor
    self.assertequal(
        unittest_pb2._testalltypes,
        file_type.message_types_by_name[unittest_pb2._testalltypes.name])

    # nested messages shouldn't be included in the message_types_by_name
    # dictionary (like in the c++ api).
    self.assertfalse(
        unittest_pb2._testalltypes_nestedmessage.name in
        file_type.message_types_by_name)

  def testpublicimports(self):
    # test public imports as embedded message.
    all_type_proto = unittest_pb2.testalltypes()
    self.assertequal(0, all_type_proto.optional_public_import_message.e)

    # publicimportmessage is actually defined in unittest_import_public_pb2
    # module, and is public imported by unittest_import_pb2 module.
    public_import_proto = unittest_import_pb2.publicimportmessage()
    self.assertequal(0, public_import_proto.e)
    self.asserttrue(unittest_import_public_pb2.publicimportmessage is
                    unittest_import_pb2.publicimportmessage)

  def testbadidentifiers(self):
    # we're just testing that the code was imported without problems.
    message = test_bad_identifiers_pb2.testbadidentifiers()
    self.assertequal(message.extensions[test_bad_identifiers_pb2.message],
                     "foo")
    self.assertequal(message.extensions[test_bad_identifiers_pb2.descriptor],
                     "bar")
    self.assertequal(message.extensions[test_bad_identifiers_pb2.reflection],
                     "baz")
    self.assertequal(message.extensions[test_bad_identifiers_pb2.service],
                     "qux")

if __name__ == '__main__':
  unittest.main()
