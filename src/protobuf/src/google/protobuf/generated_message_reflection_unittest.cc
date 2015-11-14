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
//
// to test generatedmessagereflection, we actually let the protocol compiler
// generate a full protocol message implementation and then test its
// reflection interface.  this is much easier and more maintainable than
// trying to create our own message class for generatedmessagereflection
// to wrap.
//
// the tests here closely mirror some of the tests in
// compiler/cpp/unittest, except using the reflection interface
// rather than generated accessors.

#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/unittest.pb.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

namespace {

// shorthand to get a fielddescriptor for a field of unittest::testalltypes.
const fielddescriptor* f(const string& name) {
  const fielddescriptor* result =
    unittest::testalltypes::descriptor()->findfieldbyname(name);
  google_check(result != null);
  return result;
}

test(generatedmessagereflectiontest, defaults) {
  // check that all default values are set correctly in the initial message.
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  reflection_tester.expectclearviareflection(message);

  const reflection* reflection = message.getreflection();

  // messages should return pointers to default instances until first use.
  // (this is not checked by expectclear() since it is not actually true after
  // the fields have been set and then cleared.)
  expect_eq(&unittest::testalltypes::optionalgroup::default_instance(),
            &reflection->getmessage(message, f("optionalgroup")));
  expect_eq(&unittest::testalltypes::nestedmessage::default_instance(),
            &reflection->getmessage(message, f("optional_nested_message")));
  expect_eq(&unittest::foreignmessage::default_instance(),
            &reflection->getmessage(message, f("optional_foreign_message")));
  expect_eq(&unittest_import::importmessage::default_instance(),
            &reflection->getmessage(message, f("optional_import_message")));
}

test(generatedmessagereflectiontest, accessors) {
  // set every field to a unique value then go back and check all those
  // values.
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  reflection_tester.setallfieldsviareflection(&message);
  testutil::expectallfieldsset(message);
  reflection_tester.expectallfieldssetviareflection(message);

  reflection_tester.modifyrepeatedfieldsviareflection(&message);
  testutil::expectrepeatedfieldsmodified(message);
}

test(generatedmessagereflectiontest, getstringreference) {
  // test that getstringreference() returns the underlying string when it is
  // a normal string field.
  unittest::testalltypes message;
  message.set_optional_string("foo");
  message.add_repeated_string("foo");

  const reflection* reflection = message.getreflection();
  string scratch;

  expect_eq(&message.optional_string(),
      &reflection->getstringreference(message, f("optional_string"), &scratch))
    << "for simple string fields, getstringreference() should return a "
       "reference to the underlying string.";
  expect_eq(&message.repeated_string(0),
      &reflection->getrepeatedstringreference(message, f("repeated_string"),
                                              0, &scratch))
    << "for simple string fields, getrepeatedstringreference() should return "
       "a reference to the underlying string.";
}


test(generatedmessagereflectiontest, defaultsafterclear) {
  // check that after setting all fields and then clearing, getting an
  // embedded message does not return the default instance.
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  testutil::setallfields(&message);
  message.clear();

  const reflection* reflection = message.getreflection();

  expect_ne(&unittest::testalltypes::optionalgroup::default_instance(),
            &reflection->getmessage(message, f("optionalgroup")));
  expect_ne(&unittest::testalltypes::nestedmessage::default_instance(),
            &reflection->getmessage(message, f("optional_nested_message")));
  expect_ne(&unittest::foreignmessage::default_instance(),
            &reflection->getmessage(message, f("optional_foreign_message")));
  expect_ne(&unittest_import::importmessage::default_instance(),
            &reflection->getmessage(message, f("optional_import_message")));
}


test(generatedmessagereflectiontest, swap) {
  unittest::testalltypes message1;
  unittest::testalltypes message2;

  testutil::setallfields(&message1);

  const reflection* reflection = message1.getreflection();
  reflection->swap(&message1, &message2);

  testutil::expectclear(message1);
  testutil::expectallfieldsset(message2);
}

test(generatedmessagereflectiontest, swapwithbothset) {
  unittest::testalltypes message1;
  unittest::testalltypes message2;

  testutil::setallfields(&message1);
  testutil::setallfields(&message2);
  testutil::modifyrepeatedfields(&message2);

  const reflection* reflection = message1.getreflection();
  reflection->swap(&message1, &message2);

  testutil::expectrepeatedfieldsmodified(message1);
  testutil::expectallfieldsset(message2);

  message1.set_optional_int32(532819);

  reflection->swap(&message1, &message2);

  expect_eq(532819, message2.optional_int32());
}

test(generatedmessagereflectiontest, swapextensions) {
  unittest::testallextensions message1;
  unittest::testallextensions message2;

  testutil::setallextensions(&message1);

  const reflection* reflection = message1.getreflection();
  reflection->swap(&message1, &message2);

  testutil::expectextensionsclear(message1);
  testutil::expectallextensionsset(message2);
}

test(generatedmessagereflectiontest, swapunknown) {
  unittest::testemptymessage message1, message2;

  message1.mutable_unknown_fields()->addvarint(1234, 1);

  expect_eq(1, message1.unknown_fields().field_count());
  expect_eq(0, message2.unknown_fields().field_count());
  const reflection* reflection = message1.getreflection();
  reflection->swap(&message1, &message2);
  expect_eq(0, message1.unknown_fields().field_count());
  expect_eq(1, message2.unknown_fields().field_count());
}

test(generatedmessagereflectiontest, removelast) {
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  testutil::setallfields(&message);

  reflection_tester.removelastrepeatedsviareflection(&message);

  testutil::expectlastrepeatedsremoved(message);
}

test(generatedmessagereflectiontest, removelastextensions) {
  unittest::testallextensions message;
  testutil::reflectiontester reflection_tester(
    unittest::testallextensions::descriptor());

  testutil::setallextensions(&message);

  reflection_tester.removelastrepeatedsviareflection(&message);

  testutil::expectlastrepeatedextensionsremoved(message);
}

test(generatedmessagereflectiontest, releaselast) {
  unittest::testalltypes message;
  const descriptor* descriptor = message.getdescriptor();
  testutil::reflectiontester reflection_tester(descriptor);

  testutil::setallfields(&message);

  reflection_tester.releaselastrepeatedsviareflection(&message, false);

  testutil::expectlastrepeatedsreleased(message);

  // now test that we actually release the right message.
  message.clear();
  testutil::setallfields(&message);
  assert_eq(2, message.repeated_foreign_message_size());
  const protobuf_unittest::foreignmessage* expected =
      message.mutable_repeated_foreign_message(1);
  scoped_ptr<message> released(message.getreflection()->releaselast(
      &message, descriptor->findfieldbyname("repeated_foreign_message")));
  expect_eq(expected, released.get());
}

test(generatedmessagereflectiontest, releaselastextensions) {
  unittest::testallextensions message;
  const descriptor* descriptor = message.getdescriptor();
  testutil::reflectiontester reflection_tester(descriptor);

  testutil::setallextensions(&message);

  reflection_tester.releaselastrepeatedsviareflection(&message, true);

  testutil::expectlastrepeatedextensionsreleased(message);

  // now test that we actually release the right message.
  message.clear();
  testutil::setallextensions(&message);
  assert_eq(2, message.extensionsize(
      unittest::repeated_foreign_message_extension));
  const protobuf_unittest::foreignmessage* expected = message.mutableextension(
      unittest::repeated_foreign_message_extension, 1);
  scoped_ptr<message> released(message.getreflection()->releaselast(
      &message, descriptor->file()->findextensionbyname(
          "repeated_foreign_message_extension")));
  expect_eq(expected, released.get());

}

test(generatedmessagereflectiontest, swaprepeatedelements) {
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  testutil::setallfields(&message);

  // swap and test that fields are all swapped.
  reflection_tester.swaprepeatedsviareflection(&message);
  testutil::expectrepeatedsswapped(message);

  // swap back and test that fields are all back to original values.
  reflection_tester.swaprepeatedsviareflection(&message);
  testutil::expectallfieldsset(message);
}

test(generatedmessagereflectiontest, swaprepeatedelementsextension) {
  unittest::testallextensions message;
  testutil::reflectiontester reflection_tester(
    unittest::testallextensions::descriptor());

  testutil::setallextensions(&message);

  // swap and test that fields are all swapped.
  reflection_tester.swaprepeatedsviareflection(&message);
  testutil::expectrepeatedextensionsswapped(message);

  // swap back and test that fields are all back to original values.
  reflection_tester.swaprepeatedsviareflection(&message);
  testutil::expectallextensionsset(message);
}

test(generatedmessagereflectiontest, extensions) {
  // set every extension to a unique value then go back and check all those
  // values.
  unittest::testallextensions message;
  testutil::reflectiontester reflection_tester(
    unittest::testallextensions::descriptor());

  reflection_tester.setallfieldsviareflection(&message);
  testutil::expectallextensionsset(message);
  reflection_tester.expectallfieldssetviareflection(message);

  reflection_tester.modifyrepeatedfieldsviareflection(&message);
  testutil::expectrepeatedextensionsmodified(message);
}

test(generatedmessagereflectiontest, findextensiontypebynumber) {
  const reflection* reflection =
    unittest::testallextensions::default_instance().getreflection();

  const fielddescriptor* extension1 =
    unittest::testallextensions::descriptor()->file()->findextensionbyname(
      "optional_int32_extension");
  const fielddescriptor* extension2 =
    unittest::testallextensions::descriptor()->file()->findextensionbyname(
      "repeated_string_extension");

  expect_eq(extension1,
            reflection->findknownextensionbynumber(extension1->number()));
  expect_eq(extension2,
            reflection->findknownextensionbynumber(extension2->number()));

  // non-existent extension.
  expect_true(reflection->findknownextensionbynumber(62341) == null);

  // extensions of testallextensions should not show up as extensions of
  // other types.
  expect_true(unittest::testalltypes::default_instance().getreflection()->
              findknownextensionbynumber(extension1->number()) == null);
}

test(generatedmessagereflectiontest, findknownextensionbyname) {
  const reflection* reflection =
    unittest::testallextensions::default_instance().getreflection();

  const fielddescriptor* extension1 =
    unittest::testallextensions::descriptor()->file()->findextensionbyname(
      "optional_int32_extension");
  const fielddescriptor* extension2 =
    unittest::testallextensions::descriptor()->file()->findextensionbyname(
      "repeated_string_extension");

  expect_eq(extension1,
            reflection->findknownextensionbyname(extension1->full_name()));
  expect_eq(extension2,
            reflection->findknownextensionbyname(extension2->full_name()));

  // non-existent extension.
  expect_true(reflection->findknownextensionbyname("no_such_ext") == null);

  // extensions of testallextensions should not show up as extensions of
  // other types.
  expect_true(unittest::testalltypes::default_instance().getreflection()->
              findknownextensionbyname(extension1->full_name()) == null);
}

test(generatedmessagereflectiontest, releasemessagetest) {
  unittest::testalltypes message;
  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());

  // when nothing is set, we expect all released messages to be null.
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::is_null);

  // after fields are set we should get non-null releases.
  reflection_tester.setallfieldsviareflection(&message);
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::not_null);

  // after clear() we may or may not get a message from releasemessage().
  // this is implementation specific.
  reflection_tester.setallfieldsviareflection(&message);
  message.clear();
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::can_be_null);

  // test a different code path for setting after releasing.
  testutil::setallfields(&message);
  testutil::expectallfieldsset(message);
}

test(generatedmessagereflectiontest, releaseextensionmessagetest) {
  unittest::testallextensions message;
  testutil::reflectiontester reflection_tester(
    unittest::testallextensions::descriptor());

  // when nothing is set, we expect all released messages to be null.
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::is_null);

  // after fields are set we should get non-null releases.
  reflection_tester.setallfieldsviareflection(&message);
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::not_null);

  // after clear() we may or may not get a message from releasemessage().
  // this is implementation specific.
  reflection_tester.setallfieldsviareflection(&message);
  message.clear();
  reflection_tester.expectmessagesreleasedviareflection(
      &message, testutil::reflectiontester::can_be_null);

  // test a different code path for setting after releasing.
  testutil::setallextensions(&message);
  testutil::expectallextensionsset(message);
}

#ifdef protobuf_has_death_test

test(generatedmessagereflectiontest, usageerrors) {
  unittest::testalltypes message;
  const reflection* reflection = message.getreflection();
  const descriptor* descriptor = message.getdescriptor();

#define f(name) descriptor->findfieldbyname(name)

  // testing every single failure mode would be too much work.  let's just
  // check a few.
  expect_death(
    reflection->getint32(
      message, descriptor->findfieldbyname("optional_int64")),
    "protocol buffer reflection usage error:\n"
    "  method      : google::protobuf::reflection::getint32\n"
    "  message type: protobuf_unittest\\.testalltypes\n"
    "  field       : protobuf_unittest\\.testalltypes\\.optional_int64\n"
    "  problem     : field is not the right type for this message:\n"
    "    expected  : cpptype_int32\n"
    "    field type: cpptype_int64");
  expect_death(
    reflection->getint32(
      message, descriptor->findfieldbyname("repeated_int32")),
    "protocol buffer reflection usage error:\n"
    "  method      : google::protobuf::reflection::getint32\n"
    "  message type: protobuf_unittest.testalltypes\n"
    "  field       : protobuf_unittest.testalltypes.repeated_int32\n"
    "  problem     : field is repeated; the method requires a singular field.");
  expect_death(
    reflection->getint32(
      message, unittest::foreignmessage::descriptor()->findfieldbyname("c")),
    "protocol buffer reflection usage error:\n"
    "  method      : google::protobuf::reflection::getint32\n"
    "  message type: protobuf_unittest.testalltypes\n"
    "  field       : protobuf_unittest.foreignmessage.c\n"
    "  problem     : field does not match message type.");
  expect_death(
    reflection->hasfield(
      message, unittest::foreignmessage::descriptor()->findfieldbyname("c")),
    "protocol buffer reflection usage error:\n"
    "  method      : google::protobuf::reflection::hasfield\n"
    "  message type: protobuf_unittest.testalltypes\n"
    "  field       : protobuf_unittest.foreignmessage.c\n"
    "  problem     : field does not match message type.");

#undef f
}

#endif  // protobuf_has_death_test


}  // namespace
}  // namespace protobuf
}  // namespace google
