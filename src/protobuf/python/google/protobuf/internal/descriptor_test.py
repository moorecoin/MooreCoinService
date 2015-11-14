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

"""unittest for google.protobuf.internal.descriptor."""

__author__ = 'robinson@google.com (will robinson)'

import unittest
from google.protobuf import unittest_custom_options_pb2
from google.protobuf import unittest_import_pb2
from google.protobuf import unittest_pb2
from google.protobuf import descriptor_pb2
from google.protobuf import descriptor
from google.protobuf import text_format


test_empty_message_descriptor_ascii = """
name: 'testemptymessage'
"""


class descriptortest(unittest.testcase):

  def setup(self):
    self.my_file = descriptor.filedescriptor(
        name='some/filename/some.proto',
        package='protobuf_unittest'
        )
    self.my_enum = descriptor.enumdescriptor(
        name='foreignenum',
        full_name='protobuf_unittest.foreignenum',
        filename=none,
        file=self.my_file,
        values=[
          descriptor.enumvaluedescriptor(name='foreign_foo', index=0, number=4),
          descriptor.enumvaluedescriptor(name='foreign_bar', index=1, number=5),
          descriptor.enumvaluedescriptor(name='foreign_baz', index=2, number=6),
        ])
    self.my_message = descriptor.descriptor(
        name='nestedmessage',
        full_name='protobuf_unittest.testalltypes.nestedmessage',
        filename=none,
        file=self.my_file,
        containing_type=none,
        fields=[
          descriptor.fielddescriptor(
            name='bb',
            full_name='protobuf_unittest.testalltypes.nestedmessage.bb',
            index=0, number=1,
            type=5, cpp_type=1, label=1,
            has_default_value=false, default_value=0,
            message_type=none, enum_type=none, containing_type=none,
            is_extension=false, extension_scope=none),
        ],
        nested_types=[],
        enum_types=[
          self.my_enum,
        ],
        extensions=[])
    self.my_method = descriptor.methoddescriptor(
        name='bar',
        full_name='protobuf_unittest.testservice.bar',
        index=0,
        containing_service=none,
        input_type=none,
        output_type=none)
    self.my_service = descriptor.servicedescriptor(
        name='testservicewithoptions',
        full_name='protobuf_unittest.testservicewithoptions',
        file=self.my_file,
        index=0,
        methods=[
            self.my_method
        ])

  def testenumvaluename(self):
    self.assertequal(self.my_message.enumvaluename('foreignenum', 4),
                     'foreign_foo')

    self.assertequal(
        self.my_message.enum_types_by_name[
            'foreignenum'].values_by_number[4].name,
        self.my_message.enumvaluename('foreignenum', 4))

  def testenumfixups(self):
    self.assertequal(self.my_enum, self.my_enum.values[0].type)

  def testcontainingtypefixups(self):
    self.assertequal(self.my_message, self.my_message.fields[0].containing_type)
    self.assertequal(self.my_message, self.my_enum.containing_type)

  def testcontainingservicefixups(self):
    self.assertequal(self.my_service, self.my_method.containing_service)

  def testgetoptions(self):
    self.assertequal(self.my_enum.getoptions(),
                     descriptor_pb2.enumoptions())
    self.assertequal(self.my_enum.values[0].getoptions(),
                     descriptor_pb2.enumvalueoptions())
    self.assertequal(self.my_message.getoptions(),
                     descriptor_pb2.messageoptions())
    self.assertequal(self.my_message.fields[0].getoptions(),
                     descriptor_pb2.fieldoptions())
    self.assertequal(self.my_method.getoptions(),
                     descriptor_pb2.methodoptions())
    self.assertequal(self.my_service.getoptions(),
                     descriptor_pb2.serviceoptions())

  def testsimplecustomoptions(self):
    file_descriptor = unittest_custom_options_pb2.descriptor
    message_descriptor =\
        unittest_custom_options_pb2.testmessagewithcustomoptions.descriptor
    field_descriptor = message_descriptor.fields_by_name["field1"]
    enum_descriptor = message_descriptor.enum_types_by_name["anenum"]
    enum_value_descriptor =\
        message_descriptor.enum_values_by_name["anenum_val2"]
    service_descriptor =\
        unittest_custom_options_pb2.testservicewithcustomoptions.descriptor
    method_descriptor = service_descriptor.findmethodbyname("foo")

    file_options = file_descriptor.getoptions()
    file_opt1 = unittest_custom_options_pb2.file_opt1
    self.assertequal(9876543210, file_options.extensions[file_opt1])
    message_options = message_descriptor.getoptions()
    message_opt1 = unittest_custom_options_pb2.message_opt1
    self.assertequal(-56, message_options.extensions[message_opt1])
    field_options = field_descriptor.getoptions()
    field_opt1 = unittest_custom_options_pb2.field_opt1
    self.assertequal(8765432109, field_options.extensions[field_opt1])
    field_opt2 = unittest_custom_options_pb2.field_opt2
    self.assertequal(42, field_options.extensions[field_opt2])
    enum_options = enum_descriptor.getoptions()
    enum_opt1 = unittest_custom_options_pb2.enum_opt1
    self.assertequal(-789, enum_options.extensions[enum_opt1])
    enum_value_options = enum_value_descriptor.getoptions()
    enum_value_opt1 = unittest_custom_options_pb2.enum_value_opt1
    self.assertequal(123, enum_value_options.extensions[enum_value_opt1])

    service_options = service_descriptor.getoptions()
    service_opt1 = unittest_custom_options_pb2.service_opt1
    self.assertequal(-9876543210, service_options.extensions[service_opt1])
    method_options = method_descriptor.getoptions()
    method_opt1 = unittest_custom_options_pb2.method_opt1
    self.assertequal(unittest_custom_options_pb2.methodopt1_val2,
                     method_options.extensions[method_opt1])

  def testdifferentcustomoptiontypes(self):
    kint32min = -2**31
    kint64min = -2**63
    kint32max = 2**31 - 1
    kint64max = 2**63 - 1
    kuint32max = 2**32 - 1
    kuint64max = 2**64 - 1

    message_descriptor =\
        unittest_custom_options_pb2.customoptionminintegervalues.descriptor
    message_options = message_descriptor.getoptions()
    self.assertequal(false, message_options.extensions[
        unittest_custom_options_pb2.bool_opt])
    self.assertequal(kint32min, message_options.extensions[
        unittest_custom_options_pb2.int32_opt])
    self.assertequal(kint64min, message_options.extensions[
        unittest_custom_options_pb2.int64_opt])
    self.assertequal(0, message_options.extensions[
        unittest_custom_options_pb2.uint32_opt])
    self.assertequal(0, message_options.extensions[
        unittest_custom_options_pb2.uint64_opt])
    self.assertequal(kint32min, message_options.extensions[
        unittest_custom_options_pb2.sint32_opt])
    self.assertequal(kint64min, message_options.extensions[
        unittest_custom_options_pb2.sint64_opt])
    self.assertequal(0, message_options.extensions[
        unittest_custom_options_pb2.fixed32_opt])
    self.assertequal(0, message_options.extensions[
        unittest_custom_options_pb2.fixed64_opt])
    self.assertequal(kint32min, message_options.extensions[
        unittest_custom_options_pb2.sfixed32_opt])
    self.assertequal(kint64min, message_options.extensions[
        unittest_custom_options_pb2.sfixed64_opt])

    message_descriptor =\
        unittest_custom_options_pb2.customoptionmaxintegervalues.descriptor
    message_options = message_descriptor.getoptions()
    self.assertequal(true, message_options.extensions[
        unittest_custom_options_pb2.bool_opt])
    self.assertequal(kint32max, message_options.extensions[
        unittest_custom_options_pb2.int32_opt])
    self.assertequal(kint64max, message_options.extensions[
        unittest_custom_options_pb2.int64_opt])
    self.assertequal(kuint32max, message_options.extensions[
        unittest_custom_options_pb2.uint32_opt])
    self.assertequal(kuint64max, message_options.extensions[
        unittest_custom_options_pb2.uint64_opt])
    self.assertequal(kint32max, message_options.extensions[
        unittest_custom_options_pb2.sint32_opt])
    self.assertequal(kint64max, message_options.extensions[
        unittest_custom_options_pb2.sint64_opt])
    self.assertequal(kuint32max, message_options.extensions[
        unittest_custom_options_pb2.fixed32_opt])
    self.assertequal(kuint64max, message_options.extensions[
        unittest_custom_options_pb2.fixed64_opt])
    self.assertequal(kint32max, message_options.extensions[
        unittest_custom_options_pb2.sfixed32_opt])
    self.assertequal(kint64max, message_options.extensions[
        unittest_custom_options_pb2.sfixed64_opt])

    message_descriptor =\
        unittest_custom_options_pb2.customoptionothervalues.descriptor
    message_options = message_descriptor.getoptions()
    self.assertequal(-100, message_options.extensions[
        unittest_custom_options_pb2.int32_opt])
    self.assertalmostequal(12.3456789, message_options.extensions[
        unittest_custom_options_pb2.float_opt], 6)
    self.assertalmostequal(1.234567890123456789, message_options.extensions[
        unittest_custom_options_pb2.double_opt])
    self.assertequal("hello, \"world\"", message_options.extensions[
        unittest_custom_options_pb2.string_opt])
    self.assertequal("hello\0world", message_options.extensions[
        unittest_custom_options_pb2.bytes_opt])
    dummy_enum = unittest_custom_options_pb2.dummymessagecontainingenum
    self.assertequal(
        dummy_enum.test_option_enum_type2,
        message_options.extensions[unittest_custom_options_pb2.enum_opt])

    message_descriptor =\
        unittest_custom_options_pb2.settingrealsfrompositiveints.descriptor
    message_options = message_descriptor.getoptions()
    self.assertalmostequal(12, message_options.extensions[
        unittest_custom_options_pb2.float_opt], 6)
    self.assertalmostequal(154, message_options.extensions[
        unittest_custom_options_pb2.double_opt])

    message_descriptor =\
        unittest_custom_options_pb2.settingrealsfromnegativeints.descriptor
    message_options = message_descriptor.getoptions()
    self.assertalmostequal(-12, message_options.extensions[
        unittest_custom_options_pb2.float_opt], 6)
    self.assertalmostequal(-154, message_options.extensions[
        unittest_custom_options_pb2.double_opt])

  def testcomplexextensionoptions(self):
    descriptor =\
        unittest_custom_options_pb2.variouscomplexoptions.descriptor
    options = descriptor.getoptions()
    self.assertequal(42, options.extensions[
        unittest_custom_options_pb2.complex_opt1].foo)
    self.assertequal(324, options.extensions[
        unittest_custom_options_pb2.complex_opt1].extensions[
            unittest_custom_options_pb2.quux])
    self.assertequal(876, options.extensions[
        unittest_custom_options_pb2.complex_opt1].extensions[
            unittest_custom_options_pb2.corge].qux)
    self.assertequal(987, options.extensions[
        unittest_custom_options_pb2.complex_opt2].baz)
    self.assertequal(654, options.extensions[
        unittest_custom_options_pb2.complex_opt2].extensions[
            unittest_custom_options_pb2.grault])
    self.assertequal(743, options.extensions[
        unittest_custom_options_pb2.complex_opt2].bar.foo)
    self.assertequal(1999, options.extensions[
        unittest_custom_options_pb2.complex_opt2].bar.extensions[
            unittest_custom_options_pb2.quux])
    self.assertequal(2008, options.extensions[
        unittest_custom_options_pb2.complex_opt2].bar.extensions[
            unittest_custom_options_pb2.corge].qux)
    self.assertequal(741, options.extensions[
        unittest_custom_options_pb2.complex_opt2].extensions[
            unittest_custom_options_pb2.garply].foo)
    self.assertequal(1998, options.extensions[
        unittest_custom_options_pb2.complex_opt2].extensions[
            unittest_custom_options_pb2.garply].extensions[
                unittest_custom_options_pb2.quux])
    self.assertequal(2121, options.extensions[
        unittest_custom_options_pb2.complex_opt2].extensions[
            unittest_custom_options_pb2.garply].extensions[
                unittest_custom_options_pb2.corge].qux)
    self.assertequal(1971, options.extensions[
        unittest_custom_options_pb2.complexoptiontype2
        .complexoptiontype4.complex_opt4].waldo)
    self.assertequal(321, options.extensions[
        unittest_custom_options_pb2.complex_opt2].fred.waldo)
    self.assertequal(9, options.extensions[
        unittest_custom_options_pb2.complex_opt3].qux)
    self.assertequal(22, options.extensions[
        unittest_custom_options_pb2.complex_opt3].complexoptiontype5.plugh)
    self.assertequal(24, options.extensions[
        unittest_custom_options_pb2.complexopt6].xyzzy)

  # check that aggregate options were parsed and saved correctly in
  # the appropriate descriptors.
  def testaggregateoptions(self):
    file_descriptor = unittest_custom_options_pb2.descriptor
    message_descriptor =\
        unittest_custom_options_pb2.aggregatemessage.descriptor
    field_descriptor = message_descriptor.fields_by_name["fieldname"]
    enum_descriptor = unittest_custom_options_pb2.aggregateenum.descriptor
    enum_value_descriptor = enum_descriptor.values_by_name["value"]
    service_descriptor =\
        unittest_custom_options_pb2.aggregateservice.descriptor
    method_descriptor = service_descriptor.findmethodbyname("method")

    # tests for the different types of data embedded in fileopt
    file_options = file_descriptor.getoptions().extensions[
        unittest_custom_options_pb2.fileopt]
    self.assertequal(100, file_options.i)
    self.assertequal("fileannotation", file_options.s)
    self.assertequal("nestedfileannotation", file_options.sub.s)
    self.assertequal("fileextensionannotation", file_options.file.extensions[
        unittest_custom_options_pb2.fileopt].s)
    self.assertequal("embeddedmessagesetelement", file_options.mset.extensions[
        unittest_custom_options_pb2.aggregatemessagesetelement
        .message_set_extension].s)

    # simple tests for all the other types of annotations
    self.assertequal(
        "messageannotation",
        message_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.msgopt].s)
    self.assertequal(
        "fieldannotation",
        field_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.fieldopt].s)
    self.assertequal(
        "enumannotation",
        enum_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.enumopt].s)
    self.assertequal(
        "enumvalueannotation",
        enum_value_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.enumvalopt].s)
    self.assertequal(
        "serviceannotation",
        service_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.serviceopt].s)
    self.assertequal(
        "methodannotation",
        method_descriptor.getoptions().extensions[
            unittest_custom_options_pb2.methodopt].s)

  def testnestedoptions(self):
    nested_message =\
        unittest_custom_options_pb2.nestedoptiontype.nestedmessage.descriptor
    self.assertequal(1001, nested_message.getoptions().extensions[
        unittest_custom_options_pb2.message_opt1])
    nested_field = nested_message.fields_by_name["nested_field"]
    self.assertequal(1002, nested_field.getoptions().extensions[
        unittest_custom_options_pb2.field_opt1])
    outer_message =\
        unittest_custom_options_pb2.nestedoptiontype.descriptor
    nested_enum = outer_message.enum_types_by_name["nestedenum"]
    self.assertequal(1003, nested_enum.getoptions().extensions[
        unittest_custom_options_pb2.enum_opt1])
    nested_enum_value = outer_message.enum_values_by_name["nested_enum_value"]
    self.assertequal(1004, nested_enum_value.getoptions().extensions[
        unittest_custom_options_pb2.enum_value_opt1])
    nested_extension = outer_message.extensions_by_name["nested_extension"]
    self.assertequal(1005, nested_extension.getoptions().extensions[
        unittest_custom_options_pb2.field_opt2])

  def testfiledescriptorreferences(self):
    self.assertequal(self.my_enum.file, self.my_file)
    self.assertequal(self.my_message.file, self.my_file)

  def testfiledescriptor(self):
    self.assertequal(self.my_file.name, 'some/filename/some.proto')
    self.assertequal(self.my_file.package, 'protobuf_unittest')


class descriptorcopytoprototest(unittest.testcase):
  """tests for copyto functions of descriptor."""

  def _assertprotoequal(self, actual_proto, expected_class, expected_ascii):
    expected_proto = expected_class()
    text_format.merge(expected_ascii, expected_proto)

    self.assertequal(
        actual_proto, expected_proto,
        'not equal,\nactual:\n%s\nexpected:\n%s\n'
        % (str(actual_proto), str(expected_proto)))

  def _internaltestcopytoproto(self, desc, expected_proto_class,
                               expected_proto_ascii):
    actual = expected_proto_class()
    desc.copytoproto(actual)
    self._assertprotoequal(
        actual, expected_proto_class, expected_proto_ascii)

  def testcopytoproto_emptymessage(self):
    self._internaltestcopytoproto(
        unittest_pb2.testemptymessage.descriptor,
        descriptor_pb2.descriptorproto,
        test_empty_message_descriptor_ascii)

  def testcopytoproto_nestedmessage(self):
    test_nested_message_ascii = """
      name: 'nestedmessage'
      field: <
        name: 'bb'
        number: 1
        label: 1  # optional
        type: 5  # type_int32
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testalltypes.nestedmessage.descriptor,
        descriptor_pb2.descriptorproto,
        test_nested_message_ascii)

  def testcopytoproto_foreignnestedmessage(self):
    test_foreign_nested_ascii = """
      name: 'testforeignnested'
      field: <
        name: 'foreign_nested'
        number: 1
        label: 1  # optional
        type: 11  # type_message
        type_name: '.protobuf_unittest.testalltypes.nestedmessage'
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testforeignnested.descriptor,
        descriptor_pb2.descriptorproto,
        test_foreign_nested_ascii)

  def testcopytoproto_foreignenum(self):
    test_foreign_enum_ascii = """
      name: 'foreignenum'
      value: <
        name: 'foreign_foo'
        number: 4
      >
      value: <
        name: 'foreign_bar'
        number: 5
      >
      value: <
        name: 'foreign_baz'
        number: 6
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2._foreignenum,
        descriptor_pb2.enumdescriptorproto,
        test_foreign_enum_ascii)

  def testcopytoproto_options(self):
    test_deprecated_fields_ascii = """
      name: 'testdeprecatedfields'
      field: <
        name: 'deprecated_int32'
        number: 1
        label: 1  # optional
        type: 5  # type_int32
        options: <
          deprecated: true
        >
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testdeprecatedfields.descriptor,
        descriptor_pb2.descriptorproto,
        test_deprecated_fields_ascii)

  def testcopytoproto_allextensions(self):
    test_empty_message_with_extensions_ascii = """
      name: 'testemptymessagewithextensions'
      extension_range: <
        start: 1
        end: 536870912
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testemptymessagewithextensions.descriptor,
        descriptor_pb2.descriptorproto,
        test_empty_message_with_extensions_ascii)

  def testcopytoproto_severalextensions(self):
    test_message_with_several_extensions_ascii = """
      name: 'testmultipleextensionranges'
      extension_range: <
        start: 42
        end: 43
      >
      extension_range: <
        start: 4143
        end: 4244
      >
      extension_range: <
        start: 65536
        end: 536870912
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testmultipleextensionranges.descriptor,
        descriptor_pb2.descriptorproto,
        test_message_with_several_extensions_ascii)

  def testcopytoproto_filedescriptor(self):
    unittest_import_file_descriptor_ascii = ("""
      name: 'google/protobuf/unittest_import.proto'
      package: 'protobuf_unittest_import'
      dependency: 'google/protobuf/unittest_import_public.proto'
      message_type: <
        name: 'importmessage'
        field: <
          name: 'd'
          number: 1
          label: 1  # optional
          type: 5  # type_int32
        >
      >
      """ +
      """enum_type: <
        name: 'importenum'
        value: <
          name: 'import_foo'
          number: 7
        >
        value: <
          name: 'import_bar'
          number: 8
        >
        value: <
          name: 'import_baz'
          number: 9
        >
      >
      options: <
        java_package: 'com.google.protobuf.test'
        optimize_for: 1  # speed
      >
      public_dependency: 0
      """)

    self._internaltestcopytoproto(
        unittest_import_pb2.descriptor,
        descriptor_pb2.filedescriptorproto,
        unittest_import_file_descriptor_ascii)

  def testcopytoproto_servicedescriptor(self):
    test_service_ascii = """
      name: 'testservice'
      method: <
        name: 'foo'
        input_type: '.protobuf_unittest.foorequest'
        output_type: '.protobuf_unittest.fooresponse'
      >
      method: <
        name: 'bar'
        input_type: '.protobuf_unittest.barrequest'
        output_type: '.protobuf_unittest.barresponse'
      >
      """

    self._internaltestcopytoproto(
        unittest_pb2.testservice.descriptor,
        descriptor_pb2.servicedescriptorproto,
        test_service_ascii)


class makedescriptortest(unittest.testcase):
  def testmakedescriptorwithunsignedintfield(self):
    file_descriptor_proto = descriptor_pb2.filedescriptorproto()
    file_descriptor_proto.name = 'foo'
    message_type = file_descriptor_proto.message_type.add()
    message_type.name = file_descriptor_proto.name
    field = message_type.field.add()
    field.number = 1
    field.name = 'uint64_field'
    field.label = descriptor.fielddescriptor.label_required
    field.type = descriptor.fielddescriptor.type_uint64
    result = descriptor.makedescriptor(message_type)
    self.assertequal(result.fields[0].cpp_type,
                     descriptor.fielddescriptor.cpptype_uint64)


if __name__ == '__main__':
  unittest.main()
