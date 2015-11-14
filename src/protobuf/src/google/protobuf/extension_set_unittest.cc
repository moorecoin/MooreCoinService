// protocol buffers - google's data interchange format
// copyright 2008 google inc.  all rights reserved.
// http://code.google.com/p/protobuf/
//
// redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * neither the name of google inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// this software is provided by the copyright holders and contributors
// "as is" and any express or implied warranties, including, but not
// limited to, the implied warranties of merchantability and fitness for
// a particular purpose are disclaimed. in no event shall the copyright
// owner or contributors be liable for any direct, indirect, incidental,
// special, exemplary, or consequential damages (including, but not
// limited to, procurement of substitute goods or services; loss of use,
// data, or profits; or business interruption) however caused and on any
// theory of liability, whether in contract, strict liability, or tort
// (including negligence or otherwise) arising in any way out of the use
// of this software, even if advised of the possibility of such damage.

// author: kenton@google.com (kenton varda)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <google/protobuf/extension_set.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {

namespace protobuf {
namespace internal {
namespace {

// this test closely mirrors google/protobuf/compiler/cpp/unittest.cc
// except that it uses extensions rather than regular fields.

test(extensionsettest, defaults) {
  // check that all default values are set correctly in the initial message.
  unittest::testallextensions message;

  testutil::expectextensionsclear(message);

  // messages should return pointers to default instances until first use.
  // (this is not checked by expectclear() since it is not actually true after
  // the fields have been set and then cleared.)
  expect_eq(&unittest::optionalgroup_extension::default_instance(),
            &message.getextension(unittest::optionalgroup_extension));
  expect_eq(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.getextension(unittest::optional_nested_message_extension));
  expect_eq(&unittest::foreignmessage::default_instance(),
            &message.getextension(
              unittest::optional_foreign_message_extension));
  expect_eq(&unittest_import::importmessage::default_instance(),
            &message.getextension(unittest::optional_import_message_extension));
}

test(extensionsettest, accessors) {
  // set every field to a unique value then go back and check all those
  // values.
  unittest::testallextensions message;

  testutil::setallextensions(&message);
  testutil::expectallextensionsset(message);

  testutil::modifyrepeatedextensions(&message);
  testutil::expectrepeatedextensionsmodified(message);
}

test(extensionsettest, clear) {
  // set every field to a unique value, clear the message, then check that
  // it is cleared.
  unittest::testallextensions message;

  testutil::setallextensions(&message);
  message.clear();
  testutil::expectextensionsclear(message);

  // unlike with the defaults test, we do not expect that requesting embedded
  // messages will return a pointer to the default instance.  instead, they
  // should return the objects that were created when mutable_blah() was
  // called.
  expect_ne(&unittest::optionalgroup_extension::default_instance(),
            &message.getextension(unittest::optionalgroup_extension));
  expect_ne(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.getextension(unittest::optional_nested_message_extension));
  expect_ne(&unittest::foreignmessage::default_instance(),
            &message.getextension(
              unittest::optional_foreign_message_extension));
  expect_ne(&unittest_import::importmessage::default_instance(),
            &message.getextension(unittest::optional_import_message_extension));

  // make sure setting stuff again after clearing works.  (this takes slightly
  // different code paths since the objects are reused.)
  testutil::setallextensions(&message);
  testutil::expectallextensionsset(message);
}

test(extensionsettest, clearonefield) {
  // set every field to a unique value, then clear one value and insure that
  // only that one value is cleared.
  unittest::testallextensions message;

  testutil::setallextensions(&message);
  int64 original_value =
    message.getextension(unittest::optional_int64_extension);

  // clear the field and make sure it shows up as cleared.
  message.clearextension(unittest::optional_int64_extension);
  expect_false(message.hasextension(unittest::optional_int64_extension));
  expect_eq(0, message.getextension(unittest::optional_int64_extension));

  // other adjacent fields should not be cleared.
  expect_true(message.hasextension(unittest::optional_int32_extension));
  expect_true(message.hasextension(unittest::optional_uint32_extension));

  // make sure if we set it again, then all fields are set.
  message.setextension(unittest::optional_int64_extension, original_value);
  testutil::expectallextensionsset(message);
}

test(extensionsettest, setallocatedextensin) {
  unittest::testallextensions message;
  expect_false(message.hasextension(
      unittest::optional_foreign_message_extension));
  // add a extension using setallocatedextension
  unittest::foreignmessage* foreign_message = new unittest::foreignmessage();
  message.setallocatedextension(unittest::optional_foreign_message_extension,
                                foreign_message);
  expect_true(message.hasextension(
      unittest::optional_foreign_message_extension));
  expect_eq(foreign_message,
            message.mutableextension(
                unittest::optional_foreign_message_extension));
  expect_eq(foreign_message,
            &message.getextension(
                unittest::optional_foreign_message_extension));

  // setallocatedextension should delete the previously existing extension.
  // (we reply on unittest to check memory leaks for this case)
  message.setallocatedextension(unittest::optional_foreign_message_extension,
                                 new unittest::foreignmessage());

  // setallocatedextension with a null parameter is equivalent to clearextenion.
  message.setallocatedextension(unittest::optional_foreign_message_extension,
                                 null);
  expect_false(message.hasextension(
      unittest::optional_foreign_message_extension));
}

test(extensionsettest, releaseextension) {
  unittest::testmessageset message;
  expect_false(message.hasextension(
      unittest::testmessagesetextension1::message_set_extension));
  // add a extension using setallocatedextension
  unittest::testmessagesetextension1* extension =
      new unittest::testmessagesetextension1();
  message.setallocatedextension(
      unittest::testmessagesetextension1::message_set_extension,
      extension);
  expect_true(message.hasextension(
      unittest::testmessagesetextension1::message_set_extension));
  // release the extension using releaseextension
  unittest::testmessagesetextension1* released_extension =
      message.releaseextension(
        unittest::testmessagesetextension1::message_set_extension);
  expect_eq(extension, released_extension);
  expect_false(message.hasextension(
      unittest::testmessagesetextension1::message_set_extension));
  // releaseextension will return the underlying object even after
  // clearextension is called.
  message.setallocatedextension(
      unittest::testmessagesetextension1::message_set_extension,
      extension);
  message.clearextension(
      unittest::testmessagesetextension1::message_set_extension);
  released_extension = message.releaseextension(
        unittest::testmessagesetextension1::message_set_extension);
  expect_true(released_extension != null);
  delete released_extension;
}


test(extensionsettest, copyfrom) {
  unittest::testallextensions message1, message2;

  testutil::setallextensions(&message1);
  message2.copyfrom(message1);
  testutil::expectallextensionsset(message2);
  message2.copyfrom(message1);  // exercise copy when fields already exist
  testutil::expectallextensionsset(message2);
}

test(extensiosettest, copyfrompacked) {
  unittest::testpackedextensions message1, message2;

  testutil::setpackedextensions(&message1);
  message2.copyfrom(message1);
  testutil::expectpackedextensionsset(message2);
  message2.copyfrom(message1);  // exercise copy when fields already exist
  testutil::expectpackedextensionsset(message2);
}

test(extensionsettest, copyfromupcasted) {
  unittest::testallextensions message1, message2;
  const message& upcasted_message = message1;

  testutil::setallextensions(&message1);
  message2.copyfrom(upcasted_message);
  testutil::expectallextensionsset(message2);
  // exercise copy when fields already exist
  message2.copyfrom(upcasted_message);
  testutil::expectallextensionsset(message2);
}

test(extensionsettest, swapwithempty) {
  unittest::testallextensions message1, message2;
  testutil::setallextensions(&message1);

  testutil::expectallextensionsset(message1);
  testutil::expectextensionsclear(message2);
  message1.swap(&message2);
  testutil::expectallextensionsset(message2);
  testutil::expectextensionsclear(message1);
}

test(extensionsettest, swapwithself) {
  unittest::testallextensions message;
  testutil::setallextensions(&message);

  testutil::expectallextensionsset(message);
  message.swap(&message);
  testutil::expectallextensionsset(message);
}

test(extensionsettest, serializationtoarray) {
  // serialize as testallextensions and parse as testalltypes to insure wire
  // compatibility of extensions.
  //
  // this checks serialization to a flat array by explicitly reserving space in
  // the string and calling the generated message's
  // serializewithcachedsizestoarray.
  unittest::testallextensions source;
  unittest::testalltypes destination;
  testutil::setallextensions(&source);
  int size = source.bytesize();
  string data;
  data.resize(size);
  uint8* target = reinterpret_cast<uint8*>(string_as_array(&data));
  uint8* end = source.serializewithcachedsizestoarray(target);
  expect_eq(size, end - target);
  expect_true(destination.parsefromstring(data));
  testutil::expectallfieldsset(destination);
}

test(extensionsettest, serializationtostream) {
  // serialize as testallextensions and parse as testalltypes to insure wire
  // compatibility of extensions.
  //
  // this checks serialization to an output stream by creating an array output
  // stream that can only buffer 1 byte at a time - this prevents the message
  // from ever jumping to the fast path, ensuring that serialization happens via
  // the codedoutputstream.
  unittest::testallextensions source;
  unittest::testalltypes destination;
  testutil::setallextensions(&source);
  int size = source.bytesize();
  string data;
  data.resize(size);
  {
    io::arrayoutputstream array_stream(string_as_array(&data), size, 1);
    io::codedoutputstream output_stream(&array_stream);
    source.serializewithcachedsizes(&output_stream);
    assert_false(output_stream.haderror());
  }
  expect_true(destination.parsefromstring(data));
  testutil::expectallfieldsset(destination);
}

test(extensionsettest, packedserializationtoarray) {
  // serialize as testpackedextensions and parse as testpackedtypes to insure
  // wire compatibility of extensions.
  //
  // this checks serialization to a flat array by explicitly reserving space in
  // the string and calling the generated message's
  // serializewithcachedsizestoarray.
  unittest::testpackedextensions source;
  unittest::testpackedtypes destination;
  testutil::setpackedextensions(&source);
  int size = source.bytesize();
  string data;
  data.resize(size);
  uint8* target = reinterpret_cast<uint8*>(string_as_array(&data));
  uint8* end = source.serializewithcachedsizestoarray(target);
  expect_eq(size, end - target);
  expect_true(destination.parsefromstring(data));
  testutil::expectpackedfieldsset(destination);
}

test(extensionsettest, packedserializationtostream) {
  // serialize as testpackedextensions and parse as testpackedtypes to insure
  // wire compatibility of extensions.
  //
  // this checks serialization to an output stream by creating an array output
  // stream that can only buffer 1 byte at a time - this prevents the message
  // from ever jumping to the fast path, ensuring that serialization happens via
  // the codedoutputstream.
  unittest::testpackedextensions source;
  unittest::testpackedtypes destination;
  testutil::setpackedextensions(&source);
  int size = source.bytesize();
  string data;
  data.resize(size);
  {
    io::arrayoutputstream array_stream(string_as_array(&data), size, 1);
    io::codedoutputstream output_stream(&array_stream);
    source.serializewithcachedsizes(&output_stream);
    assert_false(output_stream.haderror());
  }
  expect_true(destination.parsefromstring(data));
  testutil::expectpackedfieldsset(destination);
}

test(extensionsettest, parsing) {
  // serialize as testalltypes and parse as testallextensions.
  unittest::testalltypes source;
  unittest::testallextensions destination;
  string data;

  testutil::setallfields(&source);
  source.serializetostring(&data);
  expect_true(destination.parsefromstring(data));
  testutil::expectallextensionsset(destination);
}

test(extensionsettest, packedparsing) {
  // serialize as testpackedtypes and parse as testpackedextensions.
  unittest::testpackedtypes source;
  unittest::testpackedextensions destination;
  string data;

  testutil::setpackedfields(&source);
  source.serializetostring(&data);
  expect_true(destination.parsefromstring(data));
  testutil::expectpackedextensionsset(destination);
}

test(extensionsettest, isinitialized) {
  // test that isinitialized() returns false if required fields in nested
  // extensions are missing.
  unittest::testallextensions message;

  expect_true(message.isinitialized());

  message.mutableextension(unittest::testrequired::single);
  expect_false(message.isinitialized());

  message.mutableextension(unittest::testrequired::single)->set_a(1);
  expect_false(message.isinitialized());
  message.mutableextension(unittest::testrequired::single)->set_b(2);
  expect_false(message.isinitialized());
  message.mutableextension(unittest::testrequired::single)->set_c(3);
  expect_true(message.isinitialized());

  message.addextension(unittest::testrequired::multi);
  expect_false(message.isinitialized());

  message.mutableextension(unittest::testrequired::multi, 0)->set_a(1);
  expect_false(message.isinitialized());
  message.mutableextension(unittest::testrequired::multi, 0)->set_b(2);
  expect_false(message.isinitialized());
  message.mutableextension(unittest::testrequired::multi, 0)->set_c(3);
  expect_true(message.isinitialized());
}

test(extensionsettest, mutablestring) {
  // test the mutable string accessors.
  unittest::testallextensions message;

  message.mutableextension(unittest::optional_string_extension)->assign("foo");
  expect_true(message.hasextension(unittest::optional_string_extension));
  expect_eq("foo", message.getextension(unittest::optional_string_extension));

  message.addextension(unittest::repeated_string_extension)->assign("bar");
  assert_eq(1, message.extensionsize(unittest::repeated_string_extension));
  expect_eq("bar",
            message.getextension(unittest::repeated_string_extension, 0));
}

test(extensionsettest, spaceusedexcludingself) {
  // scalar primitive extensions should increase the extension set size by a
  // minimum of the size of the primitive type.
#define test_scalar_extensions_space_used(type, value)                        \
  do {                                                                        \
    unittest::testallextensions message;                                      \
    const int base_size = message.spaceused();                                \
    message.setextension(unittest::optional_##type##_extension, value);       \
    int min_expected_size = base_size +                                       \
        sizeof(message.getextension(unittest::optional_##type##_extension));  \
    expect_le(min_expected_size, message.spaceused());                        \
  } while (0)

  test_scalar_extensions_space_used(int32   , 101);
  test_scalar_extensions_space_used(int64   , 102);
  test_scalar_extensions_space_used(uint32  , 103);
  test_scalar_extensions_space_used(uint64  , 104);
  test_scalar_extensions_space_used(sint32  , 105);
  test_scalar_extensions_space_used(sint64  , 106);
  test_scalar_extensions_space_used(fixed32 , 107);
  test_scalar_extensions_space_used(fixed64 , 108);
  test_scalar_extensions_space_used(sfixed32, 109);
  test_scalar_extensions_space_used(sfixed64, 110);
  test_scalar_extensions_space_used(float   , 111);
  test_scalar_extensions_space_used(double  , 112);
  test_scalar_extensions_space_used(bool    , true);
#undef test_scalar_extensions_space_used
  {
    unittest::testallextensions message;
    const int base_size = message.spaceused();
    message.setextension(unittest::optional_nested_enum_extension,
                         unittest::testalltypes::foo);
    int min_expected_size = base_size +
        sizeof(message.getextension(unittest::optional_nested_enum_extension));
    expect_le(min_expected_size, message.spaceused());
  }
  {
    // strings may cause extra allocations depending on their length; ensure
    // that gets included as well.
    unittest::testallextensions message;
    const int base_size = message.spaceused();
    const string s("this is a fairly large string that will cause some "
                   "allocation in order to store it in the extension");
    message.setextension(unittest::optional_string_extension, s);
    int min_expected_size = base_size + s.length();
    expect_le(min_expected_size, message.spaceused());
  }
  {
    // messages also have additional allocation that need to be counted.
    unittest::testallextensions message;
    const int base_size = message.spaceused();
    unittest::foreignmessage foreign;
    foreign.set_c(42);
    message.mutableextension(unittest::optional_foreign_message_extension)->
        copyfrom(foreign);
    int min_expected_size = base_size + foreign.spaceused();
    expect_le(min_expected_size, message.spaceused());
  }

  // repeated primitive extensions will increase space used by at least a
  // repeatedfield<t>, and will cause additional allocations when the array
  // gets too big for the initial space.
  // this macro:
  //   - adds a value to the repeated extension, then clears it, establishing
  //     the base size.
  //   - adds a small number of values, testing that it doesn't increase the
  //     spaceused()
  //   - adds a large number of values (requiring allocation in the repeated
  //     field), and ensures that that allocation is included in spaceused()
#define test_repeated_extensions_space_used(type, cpptype, value)              \
  do {                                                                         \
    unittest::testallextensions message;                                       \
    const int base_size = message.spaceused();                                 \
    int min_expected_size = sizeof(repeatedfield<cpptype>) + base_size;        \
    message.addextension(unittest::repeated_##type##_extension, value);        \
    message.clearextension(unittest::repeated_##type##_extension);             \
    const int empty_repeated_field_size = message.spaceused();                 \
    expect_le(min_expected_size, empty_repeated_field_size) << #type;          \
    message.addextension(unittest::repeated_##type##_extension, value);        \
    message.addextension(unittest::repeated_##type##_extension, value);        \
    expect_eq(empty_repeated_field_size, message.spaceused()) << #type;        \
    message.clearextension(unittest::repeated_##type##_extension);             \
    for (int i = 0; i < 16; ++i) {                                             \
      message.addextension(unittest::repeated_##type##_extension, value);      \
    }                                                                          \
    int expected_size = sizeof(cpptype) * (16 -                                \
        kminrepeatedfieldallocationsize) + empty_repeated_field_size;          \
    expect_eq(expected_size, message.spaceused()) << #type;                    \
  } while (0)

  test_repeated_extensions_space_used(int32   , int32 , 101);
  test_repeated_extensions_space_used(int64   , int64 , 102);
  test_repeated_extensions_space_used(uint32  , uint32, 103);
  test_repeated_extensions_space_used(uint64  , uint64, 104);
  test_repeated_extensions_space_used(sint32  , int32 , 105);
  test_repeated_extensions_space_used(sint64  , int64 , 106);
  test_repeated_extensions_space_used(fixed32 , uint32, 107);
  test_repeated_extensions_space_used(fixed64 , uint64, 108);
  test_repeated_extensions_space_used(sfixed32, int32 , 109);
  test_repeated_extensions_space_used(sfixed64, int64 , 110);
  test_repeated_extensions_space_used(float   , float , 111);
  test_repeated_extensions_space_used(double  , double, 112);
  test_repeated_extensions_space_used(bool    , bool  , true);
  test_repeated_extensions_space_used(nested_enum, int,
                                      unittest::testalltypes::foo);
#undef test_repeated_extensions_space_used
  // repeated strings
  {
    unittest::testallextensions message;
    const int base_size = message.spaceused();
    int min_expected_size = sizeof(repeatedptrfield<string>) + base_size;
    const string value(256, 'x');
    // once items are allocated, they may stick around even when cleared so
    // without the hardcore memory management accessors there isn't a notion of
    // the empty repeated field memory usage as there is with primitive types.
    for (int i = 0; i < 16; ++i) {
      message.addextension(unittest::repeated_string_extension, value);
    }
    min_expected_size += (sizeof(value) + value.size()) *
        (16 - kminrepeatedfieldallocationsize);
    expect_le(min_expected_size, message.spaceused());
  }
  // repeated messages
  {
    unittest::testallextensions message;
    const int base_size = message.spaceused();
    int min_expected_size = sizeof(repeatedptrfield<unittest::foreignmessage>) +
        base_size;
    unittest::foreignmessage prototype;
    prototype.set_c(2);
    for (int i = 0; i < 16; ++i) {
      message.addextension(unittest::repeated_foreign_message_extension)->
          copyfrom(prototype);
    }
    min_expected_size +=
        (16 - kminrepeatedfieldallocationsize) * prototype.spaceused();
    expect_le(min_expected_size, message.spaceused());
  }
}

#ifdef protobuf_has_death_test

test(extensionsettest, invalidenumdeath) {
  unittest::testallextensions message;
  expect_debug_death(
    message.setextension(unittest::optional_foreign_enum_extension,
                         static_cast<unittest::foreignenum>(53)),
    "isvalid");
}

#endif  // protobuf_has_death_test

test(extensionsettest, dynamicextensions) {
  // test adding a dynamic extension to a compiled-in message object.

  filedescriptorproto dynamic_proto;
  dynamic_proto.set_name("dynamic_extensions_test.proto");
  dynamic_proto.add_dependency(
      unittest::testallextensions::descriptor()->file()->name());
  dynamic_proto.set_package("dynamic_extensions");

  // copy the fields and nested types from testdynamicextensions into our new
  // proto, converting the fields into extensions.
  const descriptor* template_descriptor =
      unittest::testdynamicextensions::descriptor();
  descriptorproto template_descriptor_proto;
  template_descriptor->copyto(&template_descriptor_proto);
  dynamic_proto.mutable_message_type()->mergefrom(
      template_descriptor_proto.nested_type());
  dynamic_proto.mutable_enum_type()->mergefrom(
      template_descriptor_proto.enum_type());
  dynamic_proto.mutable_extension()->mergefrom(
      template_descriptor_proto.field());

  // for each extension that we added...
  for (int i = 0; i < dynamic_proto.extension_size(); i++) {
    // set its extendee to testallextensions.
    fielddescriptorproto* extension = dynamic_proto.mutable_extension(i);
    extension->set_extendee(
        unittest::testallextensions::descriptor()->full_name());

    // if the field refers to one of the types nested in testdynamicextensions,
    // make it refer to the type in our dynamic proto instead.
    string prefix = "." + template_descriptor->full_name() + ".";
    if (extension->has_type_name()) {
      string* type_name = extension->mutable_type_name();
      if (hasprefixstring(*type_name, prefix)) {
        type_name->replace(0, prefix.size(), ".dynamic_extensions.");
      }
    }
  }

  // now build the file, using the generated pool as an underlay.
  descriptorpool dynamic_pool(descriptorpool::generated_pool());
  const filedescriptor* file = dynamic_pool.buildfile(dynamic_proto);
  assert_true(file != null);
  dynamicmessagefactory dynamic_factory(&dynamic_pool);
  dynamic_factory.setdelegatetogeneratedfactory(true);

  // construct a message that we can parse with the extensions we defined.
  // since the extensions were based off of the fields of testdynamicextensions,
  // we can use that message to create this test message.
  string data;
  {
    unittest::testdynamicextensions message;
    message.set_scalar_extension(123);
    message.set_enum_extension(unittest::foreign_bar);
    message.set_dynamic_enum_extension(
        unittest::testdynamicextensions::dynamic_baz);
    message.mutable_message_extension()->set_c(456);
    message.mutable_dynamic_message_extension()->set_dynamic_field(789);
    message.add_repeated_extension("foo");
    message.add_repeated_extension("bar");
    message.add_packed_extension(12);
    message.add_packed_extension(-34);
    message.add_packed_extension(56);
    message.add_packed_extension(-78);

    // also add some unknown fields.

    // an unknown enum value (for a known field).
    message.mutable_unknown_fields()->addvarint(
      unittest::testdynamicextensions::kdynamicenumextensionfieldnumber,
      12345);
    // a regular unknown field.
    message.mutable_unknown_fields()->addlengthdelimited(54321, "unknown");

    message.serializetostring(&data);
  }

  // now we can parse this using our dynamic extension definitions...
  unittest::testallextensions message;
  {
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setextensionregistry(&dynamic_pool, &dynamic_factory);
    assert_true(message.parsefromcodedstream(&input));
    assert_true(input.consumedentiremessage());
  }

  // can we print it?
  expect_eq(
    "[dynamic_extensions.scalar_extension]: 123\n"
    "[dynamic_extensions.enum_extension]: foreign_bar\n"
    "[dynamic_extensions.dynamic_enum_extension]: dynamic_baz\n"
    "[dynamic_extensions.message_extension] {\n"
    "  c: 456\n"
    "}\n"
    "[dynamic_extensions.dynamic_message_extension] {\n"
    "  dynamic_field: 789\n"
    "}\n"
    "[dynamic_extensions.repeated_extension]: \"foo\"\n"
    "[dynamic_extensions.repeated_extension]: \"bar\"\n"
    "[dynamic_extensions.packed_extension]: 12\n"
    "[dynamic_extensions.packed_extension]: -34\n"
    "[dynamic_extensions.packed_extension]: 56\n"
    "[dynamic_extensions.packed_extension]: -78\n"
    "2002: 12345\n"
    "54321: \"unknown\"\n",
    message.debugstring());

  // can we serialize it?
  // (don't use expect_eq because we don't want to dump raw binary data to the
  // terminal on failure.)
  expect_true(message.serializeasstring() == data);

  // what if we parse using the reflection-based parser?
  {
    unittest::testallextensions message2;
    io::arrayinputstream raw_input(data.data(), data.size());
    io::codedinputstream input(&raw_input);
    input.setextensionregistry(&dynamic_pool, &dynamic_factory);
    assert_true(wireformat::parseandmergepartial(&input, &message2));
    assert_true(input.consumedentiremessage());
    expect_eq(message.debugstring(), message2.debugstring());
  }

  // are the embedded generated types actually using the generated objects?
  {
    const fielddescriptor* message_extension =
        file->findextensionbyname("message_extension");
    assert_true(message_extension != null);
    const message& sub_message =
        message.getreflection()->getmessage(message, message_extension);
    const unittest::foreignmessage* typed_sub_message =
#ifdef google_protobuf_no_rtti
        static_cast<const unittest::foreignmessage*>(&sub_message);
#else
        dynamic_cast<const unittest::foreignmessage*>(&sub_message);
#endif
    assert_true(typed_sub_message != null);
    expect_eq(456, typed_sub_message->c());
  }

  // what does getmessage() return for the embedded dynamic type if it isn't
  // present?
  {
    const fielddescriptor* dynamic_message_extension =
        file->findextensionbyname("dynamic_message_extension");
    assert_true(dynamic_message_extension != null);
    const message& parent = unittest::testallextensions::default_instance();
    const message& sub_message =
        parent.getreflection()->getmessage(parent, dynamic_message_extension,
                                           &dynamic_factory);
    const message* prototype =
        dynamic_factory.getprototype(dynamic_message_extension->message_type());
    expect_eq(prototype, &sub_message);
  }
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
