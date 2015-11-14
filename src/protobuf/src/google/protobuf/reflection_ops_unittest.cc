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

#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/test_util.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace internal {
namespace {

test(reflectionopstest, sanitycheck) {
  unittest::testalltypes message;

  testutil::setallfields(&message);
  testutil::expectallfieldsset(message);
}

test(reflectionopstest, copy) {
  unittest::testalltypes message, message2;

  testutil::setallfields(&message);

  reflectionops::copy(message, &message2);

  testutil::expectallfieldsset(message2);

  // copying from self should be a no-op.
  reflectionops::copy(message2, &message2);
  testutil::expectallfieldsset(message2);
}

test(reflectionopstest, copyextensions) {
  unittest::testallextensions message, message2;

  testutil::setallextensions(&message);

  reflectionops::copy(message, &message2);

  testutil::expectallextensionsset(message2);
}

test(reflectionopstest, merge) {
  // note:  copy is implemented in terms of merge() so technically the copy
  //   test already tested most of this.

  unittest::testalltypes message, message2;

  testutil::setallfields(&message);

  // this field will test merging into an empty spot.
  message2.set_optional_int32(message.optional_int32());
  message.clear_optional_int32();

  // this tests overwriting.
  message2.set_optional_string(message.optional_string());
  message.set_optional_string("something else");

  // this tests concatenating.
  message2.add_repeated_int32(message.repeated_int32(1));
  int32 i = message.repeated_int32(0);
  message.clear_repeated_int32();
  message.add_repeated_int32(i);

  reflectionops::merge(message2, &message);

  testutil::expectallfieldsset(message);
}

test(reflectionopstest, mergeextensions) {
  // note:  copy is implemented in terms of merge() so technically the copy
  //   test already tested most of this.

  unittest::testallextensions message, message2;

  testutil::setallextensions(&message);

  // this field will test merging into an empty spot.
  message2.setextension(unittest::optional_int32_extension,
    message.getextension(unittest::optional_int32_extension));
  message.clearextension(unittest::optional_int32_extension);

  // this tests overwriting.
  message2.setextension(unittest::optional_string_extension,
    message.getextension(unittest::optional_string_extension));
  message.setextension(unittest::optional_string_extension, "something else");

  // this tests concatenating.
  message2.addextension(unittest::repeated_int32_extension,
    message.getextension(unittest::repeated_int32_extension, 1));
  int32 i = message.getextension(unittest::repeated_int32_extension, 0);
  message.clearextension(unittest::repeated_int32_extension);
  message.addextension(unittest::repeated_int32_extension, i);

  reflectionops::merge(message2, &message);

  testutil::expectallextensionsset(message);
}

test(reflectionopstest, mergeunknown) {
  // test that the messages' unknownfieldsets are correctly merged.
  unittest::testemptymessage message1, message2;
  message1.mutable_unknown_fields()->addvarint(1234, 1);
  message2.mutable_unknown_fields()->addvarint(1234, 2);

  reflectionops::merge(message2, &message1);

  assert_eq(2, message1.unknown_fields().field_count());
  assert_eq(unknownfield::type_varint,
            message1.unknown_fields().field(0).type());
  expect_eq(1, message1.unknown_fields().field(0).varint());
  assert_eq(unknownfield::type_varint,
            message1.unknown_fields().field(1).type());
  expect_eq(2, message1.unknown_fields().field(1).varint());
}

#ifdef protobuf_has_death_test

test(reflectionopstest, mergefromself) {
  // note:  copy is implemented in terms of merge() so technically the copy
  //   test already tested most of this.

  unittest::testalltypes message;

  expect_death(
    reflectionops::merge(message, &message),
    "&from");
}

#endif  // protobuf_has_death_test

test(reflectionopstest, clear) {
  unittest::testalltypes message;

  testutil::setallfields(&message);

  reflectionops::clear(&message);

  testutil::expectclear(message);

  // check that getting embedded messages returns the objects created during
  // setallfields() rather than default instances.
  expect_ne(&unittest::testalltypes::optionalgroup::default_instance(),
            &message.optionalgroup());
  expect_ne(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.optional_nested_message());
  expect_ne(&unittest::foreignmessage::default_instance(),
            &message.optional_foreign_message());
  expect_ne(&unittest_import::importmessage::default_instance(),
            &message.optional_import_message());
}

test(reflectionopstest, clearextensions) {
  unittest::testallextensions message;

  testutil::setallextensions(&message);

  reflectionops::clear(&message);

  testutil::expectextensionsclear(message);

  // check that getting embedded messages returns the objects created during
  // setallextensions() rather than default instances.
  expect_ne(&unittest::optionalgroup_extension::default_instance(),
            &message.getextension(unittest::optionalgroup_extension));
  expect_ne(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.getextension(unittest::optional_nested_message_extension));
  expect_ne(&unittest::foreignmessage::default_instance(),
            &message.getextension(
              unittest::optional_foreign_message_extension));
  expect_ne(&unittest_import::importmessage::default_instance(),
            &message.getextension(unittest::optional_import_message_extension));
}

test(reflectionopstest, clearunknown) {
  // test that the message's unknownfieldset is correctly cleared.
  unittest::testemptymessage message;
  message.mutable_unknown_fields()->addvarint(1234, 1);

  reflectionops::clear(&message);

  expect_eq(0, message.unknown_fields().field_count());
}

test(reflectionopstest, discardunknownfields) {
  unittest::testalltypes message;
  testutil::setallfields(&message);

  // set some unknown fields in message.
  message.mutable_unknown_fields()
        ->addvarint(123456, 654321);
  message.mutable_optional_nested_message()
        ->mutable_unknown_fields()
        ->addvarint(123456, 654321);
  message.mutable_repeated_nested_message(0)
        ->mutable_unknown_fields()
        ->addvarint(123456, 654321);

  expect_eq(1, message.unknown_fields().field_count());
  expect_eq(1, message.optional_nested_message()
                      .unknown_fields().field_count());
  expect_eq(1, message.repeated_nested_message(0)
                      .unknown_fields().field_count());

  // discard them.
  reflectionops::discardunknownfields(&message);
  testutil::expectallfieldsset(message);

  expect_eq(0, message.unknown_fields().field_count());
  expect_eq(0, message.optional_nested_message()
                      .unknown_fields().field_count());
  expect_eq(0, message.repeated_nested_message(0)
                      .unknown_fields().field_count());
}

test(reflectionopstest, discardunknownextensions) {
  unittest::testallextensions message;
  testutil::setallextensions(&message);

  // set some unknown fields.
  message.mutable_unknown_fields()
        ->addvarint(123456, 654321);
  message.mutableextension(unittest::optional_nested_message_extension)
        ->mutable_unknown_fields()
        ->addvarint(123456, 654321);
  message.mutableextension(unittest::repeated_nested_message_extension, 0)
        ->mutable_unknown_fields()
        ->addvarint(123456, 654321);

  expect_eq(1, message.unknown_fields().field_count());
  expect_eq(1,
    message.getextension(unittest::optional_nested_message_extension)
           .unknown_fields().field_count());
  expect_eq(1,
    message.getextension(unittest::repeated_nested_message_extension, 0)
           .unknown_fields().field_count());

  // discard them.
  reflectionops::discardunknownfields(&message);
  testutil::expectallextensionsset(message);

  expect_eq(0, message.unknown_fields().field_count());
  expect_eq(0,
    message.getextension(unittest::optional_nested_message_extension)
           .unknown_fields().field_count());
  expect_eq(0,
    message.getextension(unittest::repeated_nested_message_extension, 0)
           .unknown_fields().field_count());
}

test(reflectionopstest, isinitialized) {
  unittest::testrequired message;

  expect_false(reflectionops::isinitialized(message));
  message.set_a(1);
  expect_false(reflectionops::isinitialized(message));
  message.set_b(2);
  expect_false(reflectionops::isinitialized(message));
  message.set_c(3);
  expect_true(reflectionops::isinitialized(message));
}

test(reflectionopstest, foreignisinitialized) {
  unittest::testrequiredforeign message;

  // starts out initialized because the foreign message is itself an optional
  // field.
  expect_true(reflectionops::isinitialized(message));

  // once we create that field, the message is no longer initialized.
  message.mutable_optional_message();
  expect_false(reflectionops::isinitialized(message));

  // initialize it.  now we're initialized.
  message.mutable_optional_message()->set_a(1);
  message.mutable_optional_message()->set_b(2);
  message.mutable_optional_message()->set_c(3);
  expect_true(reflectionops::isinitialized(message));

  // add a repeated version of the message.  no longer initialized.
  unittest::testrequired* sub_message = message.add_repeated_message();
  expect_false(reflectionops::isinitialized(message));

  // initialize that repeated version.
  sub_message->set_a(1);
  sub_message->set_b(2);
  sub_message->set_c(3);
  expect_true(reflectionops::isinitialized(message));
}

test(reflectionopstest, extensionisinitialized) {
  unittest::testallextensions message;

  // starts out initialized because the foreign message is itself an optional
  // field.
  expect_true(reflectionops::isinitialized(message));

  // once we create that field, the message is no longer initialized.
  message.mutableextension(unittest::testrequired::single);
  expect_false(reflectionops::isinitialized(message));

  // initialize it.  now we're initialized.
  message.mutableextension(unittest::testrequired::single)->set_a(1);
  message.mutableextension(unittest::testrequired::single)->set_b(2);
  message.mutableextension(unittest::testrequired::single)->set_c(3);
  expect_true(reflectionops::isinitialized(message));

  // add a repeated version of the message.  no longer initialized.
  message.addextension(unittest::testrequired::multi);
  expect_false(reflectionops::isinitialized(message));

  // initialize that repeated version.
  message.mutableextension(unittest::testrequired::multi, 0)->set_a(1);
  message.mutableextension(unittest::testrequired::multi, 0)->set_b(2);
  message.mutableextension(unittest::testrequired::multi, 0)->set_c(3);
  expect_true(reflectionops::isinitialized(message));
}

static string findinitializationerrors(const message& message) {
  vector<string> errors;
  reflectionops::findinitializationerrors(message, "", &errors);
  return joinstrings(errors, ",");
}

test(reflectionopstest, findinitializationerrors) {
  unittest::testrequired message;
  expect_eq("a,b,c", findinitializationerrors(message));
}

test(reflectionopstest, findforeigninitializationerrors) {
  unittest::testrequiredforeign message;
  message.mutable_optional_message();
  message.add_repeated_message();
  message.add_repeated_message();
  expect_eq("optional_message.a,"
            "optional_message.b,"
            "optional_message.c,"
            "repeated_message[0].a,"
            "repeated_message[0].b,"
            "repeated_message[0].c,"
            "repeated_message[1].a,"
            "repeated_message[1].b,"
            "repeated_message[1].c",
            findinitializationerrors(message));
}

test(reflectionopstest, findextensioninitializationerrors) {
  unittest::testallextensions message;
  message.mutableextension(unittest::testrequired::single);
  message.addextension(unittest::testrequired::multi);
  message.addextension(unittest::testrequired::multi);
  expect_eq("(protobuf_unittest.testrequired.single).a,"
            "(protobuf_unittest.testrequired.single).b,"
            "(protobuf_unittest.testrequired.single).c,"
            "(protobuf_unittest.testrequired.multi)[0].a,"
            "(protobuf_unittest.testrequired.multi)[0].b,"
            "(protobuf_unittest.testrequired.multi)[0].c,"
            "(protobuf_unittest.testrequired.multi)[1].a,"
            "(protobuf_unittest.testrequired.multi)[1].b,"
            "(protobuf_unittest.testrequired.multi)[1].c",
            findinitializationerrors(message));
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
