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

#ifdef _win32
// verify that #including windows.h does not break anything (e.g. because
// windows.h #defines getmessage() as a macro).
#include <windows.h>
#endif

#include <google/protobuf/test_util.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

void testutil::setallfields(unittest::testalltypes* message) {
  setoptionalfields(message);
  addrepeatedfields1(message);
  addrepeatedfields2(message);
  setdefaultfields(message);
}

void testutil::setoptionalfields(unittest::testalltypes* message) {
  message->set_optional_int32   (101);
  message->set_optional_int64   (102);
  message->set_optional_uint32  (103);
  message->set_optional_uint64  (104);
  message->set_optional_sint32  (105);
  message->set_optional_sint64  (106);
  message->set_optional_fixed32 (107);
  message->set_optional_fixed64 (108);
  message->set_optional_sfixed32(109);
  message->set_optional_sfixed64(110);
  message->set_optional_float   (111);
  message->set_optional_double  (112);
  message->set_optional_bool    (true);
  message->set_optional_string  ("115");
  message->set_optional_bytes   ("116");

  message->mutable_optionalgroup                 ()->set_a(117);
  message->mutable_optional_nested_message       ()->set_bb(118);
  message->mutable_optional_foreign_message      ()->set_c(119);
  message->mutable_optional_import_message       ()->set_d(120);
  message->mutable_optional_public_import_message()->set_e(126);
  message->mutable_optional_lazy_message         ()->set_bb(127);

  message->set_optional_nested_enum (unittest::testalltypes::baz);
  message->set_optional_foreign_enum(unittest::foreign_baz      );
  message->set_optional_import_enum (unittest_import::import_baz);

  // stringpiece and cord fields are only accessible via reflection in the
  // open source release; see comments in compiler/cpp/string_field.cc.
#ifndef protobuf_test_no_descriptors
  message->getreflection()->setstring(
    message,
    message->getdescriptor()->findfieldbyname("optional_string_piece"),
    "124");
  message->getreflection()->setstring(
    message,
    message->getdescriptor()->findfieldbyname("optional_cord"),
    "125");
#endif  // !protobuf_test_no_descriptors
}

// -------------------------------------------------------------------

void testutil::addrepeatedfields1(unittest::testalltypes* message) {
  message->add_repeated_int32   (201);
  message->add_repeated_int64   (202);
  message->add_repeated_uint32  (203);
  message->add_repeated_uint64  (204);
  message->add_repeated_sint32  (205);
  message->add_repeated_sint64  (206);
  message->add_repeated_fixed32 (207);
  message->add_repeated_fixed64 (208);
  message->add_repeated_sfixed32(209);
  message->add_repeated_sfixed64(210);
  message->add_repeated_float   (211);
  message->add_repeated_double  (212);
  message->add_repeated_bool    (true);
  message->add_repeated_string  ("215");
  message->add_repeated_bytes   ("216");

  message->add_repeatedgroup           ()->set_a(217);
  message->add_repeated_nested_message ()->set_bb(218);
  message->add_repeated_foreign_message()->set_c(219);
  message->add_repeated_import_message ()->set_d(220);
  message->add_repeated_lazy_message   ()->set_bb(227);

  message->add_repeated_nested_enum (unittest::testalltypes::bar);
  message->add_repeated_foreign_enum(unittest::foreign_bar      );
  message->add_repeated_import_enum (unittest_import::import_bar);

#ifndef protobuf_test_no_descriptors
  message->getreflection()->addstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_string_piece"),
    "224");
  message->getreflection()->addstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_cord"),
    "225");
#endif  // !protobuf_test_no_descriptors
}

void testutil::addrepeatedfields2(unittest::testalltypes* message) {
  // add a second one of each field.
  message->add_repeated_int32   (301);
  message->add_repeated_int64   (302);
  message->add_repeated_uint32  (303);
  message->add_repeated_uint64  (304);
  message->add_repeated_sint32  (305);
  message->add_repeated_sint64  (306);
  message->add_repeated_fixed32 (307);
  message->add_repeated_fixed64 (308);
  message->add_repeated_sfixed32(309);
  message->add_repeated_sfixed64(310);
  message->add_repeated_float   (311);
  message->add_repeated_double  (312);
  message->add_repeated_bool    (false);
  message->add_repeated_string  ("315");
  message->add_repeated_bytes   ("316");

  message->add_repeatedgroup           ()->set_a(317);
  message->add_repeated_nested_message ()->set_bb(318);
  message->add_repeated_foreign_message()->set_c(319);
  message->add_repeated_import_message ()->set_d(320);
  message->add_repeated_lazy_message   ()->set_bb(327);

  message->add_repeated_nested_enum (unittest::testalltypes::baz);
  message->add_repeated_foreign_enum(unittest::foreign_baz      );
  message->add_repeated_import_enum (unittest_import::import_baz);

#ifndef protobuf_test_no_descriptors
  message->getreflection()->addstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_string_piece"),
    "324");
  message->getreflection()->addstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_cord"),
    "325");
#endif  // !protobuf_test_no_descriptors
}

// -------------------------------------------------------------------

void testutil::setdefaultfields(unittest::testalltypes* message) {
  message->set_default_int32   (401);
  message->set_default_int64   (402);
  message->set_default_uint32  (403);
  message->set_default_uint64  (404);
  message->set_default_sint32  (405);
  message->set_default_sint64  (406);
  message->set_default_fixed32 (407);
  message->set_default_fixed64 (408);
  message->set_default_sfixed32(409);
  message->set_default_sfixed64(410);
  message->set_default_float   (411);
  message->set_default_double  (412);
  message->set_default_bool    (false);
  message->set_default_string  ("415");
  message->set_default_bytes   ("416");

  message->set_default_nested_enum (unittest::testalltypes::foo);
  message->set_default_foreign_enum(unittest::foreign_foo      );
  message->set_default_import_enum (unittest_import::import_foo);

#ifndef protobuf_test_no_descriptors
  message->getreflection()->setstring(
    message,
    message->getdescriptor()->findfieldbyname("default_string_piece"),
    "424");
  message->getreflection()->setstring(
    message,
    message->getdescriptor()->findfieldbyname("default_cord"),
    "425");
#endif  // !protobuf_test_no_descriptors
}

// -------------------------------------------------------------------

void testutil::modifyrepeatedfields(unittest::testalltypes* message) {
  message->set_repeated_int32   (1, 501);
  message->set_repeated_int64   (1, 502);
  message->set_repeated_uint32  (1, 503);
  message->set_repeated_uint64  (1, 504);
  message->set_repeated_sint32  (1, 505);
  message->set_repeated_sint64  (1, 506);
  message->set_repeated_fixed32 (1, 507);
  message->set_repeated_fixed64 (1, 508);
  message->set_repeated_sfixed32(1, 509);
  message->set_repeated_sfixed64(1, 510);
  message->set_repeated_float   (1, 511);
  message->set_repeated_double  (1, 512);
  message->set_repeated_bool    (1, true);
  message->set_repeated_string  (1, "515");
  message->set_repeated_bytes   (1, "516");

  message->mutable_repeatedgroup           (1)->set_a(517);
  message->mutable_repeated_nested_message (1)->set_bb(518);
  message->mutable_repeated_foreign_message(1)->set_c(519);
  message->mutable_repeated_import_message (1)->set_d(520);
  message->mutable_repeated_lazy_message   (1)->set_bb(527);

  message->set_repeated_nested_enum (1, unittest::testalltypes::foo);
  message->set_repeated_foreign_enum(1, unittest::foreign_foo      );
  message->set_repeated_import_enum (1, unittest_import::import_foo);

#ifndef protobuf_test_no_descriptors
  message->getreflection()->setrepeatedstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_string_piece"),
    1, "524");
  message->getreflection()->setrepeatedstring(
    message,
    message->getdescriptor()->findfieldbyname("repeated_cord"),
    1, "525");
#endif  // !protobuf_test_no_descriptors
}

// -------------------------------------------------------------------

void testutil::expectallfieldsset(const unittest::testalltypes& message) {
  expect_true(message.has_optional_int32   ());
  expect_true(message.has_optional_int64   ());
  expect_true(message.has_optional_uint32  ());
  expect_true(message.has_optional_uint64  ());
  expect_true(message.has_optional_sint32  ());
  expect_true(message.has_optional_sint64  ());
  expect_true(message.has_optional_fixed32 ());
  expect_true(message.has_optional_fixed64 ());
  expect_true(message.has_optional_sfixed32());
  expect_true(message.has_optional_sfixed64());
  expect_true(message.has_optional_float   ());
  expect_true(message.has_optional_double  ());
  expect_true(message.has_optional_bool    ());
  expect_true(message.has_optional_string  ());
  expect_true(message.has_optional_bytes   ());

  expect_true(message.has_optionalgroup                 ());
  expect_true(message.has_optional_nested_message       ());
  expect_true(message.has_optional_foreign_message      ());
  expect_true(message.has_optional_import_message       ());
  expect_true(message.has_optional_public_import_message());
  expect_true(message.has_optional_lazy_message         ());

  expect_true(message.optionalgroup                 ().has_a());
  expect_true(message.optional_nested_message       ().has_bb());
  expect_true(message.optional_foreign_message      ().has_c());
  expect_true(message.optional_import_message       ().has_d());
  expect_true(message.optional_public_import_message().has_e());
  expect_true(message.optional_lazy_message         ().has_bb());

  expect_true(message.has_optional_nested_enum ());
  expect_true(message.has_optional_foreign_enum());
  expect_true(message.has_optional_import_enum ());

#ifndef protobuf_test_no_descriptors
  expect_true(message.has_optional_string_piece());
  expect_true(message.has_optional_cord());
#endif

  expect_eq(101  , message.optional_int32   ());
  expect_eq(102  , message.optional_int64   ());
  expect_eq(103  , message.optional_uint32  ());
  expect_eq(104  , message.optional_uint64  ());
  expect_eq(105  , message.optional_sint32  ());
  expect_eq(106  , message.optional_sint64  ());
  expect_eq(107  , message.optional_fixed32 ());
  expect_eq(108  , message.optional_fixed64 ());
  expect_eq(109  , message.optional_sfixed32());
  expect_eq(110  , message.optional_sfixed64());
  expect_eq(111  , message.optional_float   ());
  expect_eq(112  , message.optional_double  ());
  expect_true(     message.optional_bool    ());
  expect_eq("115", message.optional_string  ());
  expect_eq("116", message.optional_bytes   ());

  expect_eq(117, message.optionalgroup                  ().a());
  expect_eq(118, message.optional_nested_message        ().bb());
  expect_eq(119, message.optional_foreign_message       ().c());
  expect_eq(120, message.optional_import_message        ().d());
  expect_eq(126, message.optional_public_import_message ().e());
  expect_eq(127, message.optional_lazy_message          ().bb());

  expect_eq(unittest::testalltypes::baz, message.optional_nested_enum ());
  expect_eq(unittest::foreign_baz      , message.optional_foreign_enum());
  expect_eq(unittest_import::import_baz, message.optional_import_enum ());


  // -----------------------------------------------------------------

  assert_eq(2, message.repeated_int32_size   ());
  assert_eq(2, message.repeated_int64_size   ());
  assert_eq(2, message.repeated_uint32_size  ());
  assert_eq(2, message.repeated_uint64_size  ());
  assert_eq(2, message.repeated_sint32_size  ());
  assert_eq(2, message.repeated_sint64_size  ());
  assert_eq(2, message.repeated_fixed32_size ());
  assert_eq(2, message.repeated_fixed64_size ());
  assert_eq(2, message.repeated_sfixed32_size());
  assert_eq(2, message.repeated_sfixed64_size());
  assert_eq(2, message.repeated_float_size   ());
  assert_eq(2, message.repeated_double_size  ());
  assert_eq(2, message.repeated_bool_size    ());
  assert_eq(2, message.repeated_string_size  ());
  assert_eq(2, message.repeated_bytes_size   ());

  assert_eq(2, message.repeatedgroup_size           ());
  assert_eq(2, message.repeated_nested_message_size ());
  assert_eq(2, message.repeated_foreign_message_size());
  assert_eq(2, message.repeated_import_message_size ());
  assert_eq(2, message.repeated_lazy_message_size   ());
  assert_eq(2, message.repeated_nested_enum_size    ());
  assert_eq(2, message.repeated_foreign_enum_size   ());
  assert_eq(2, message.repeated_import_enum_size    ());

#ifndef protobuf_test_no_descriptors
  assert_eq(2, message.repeated_string_piece_size());
  assert_eq(2, message.repeated_cord_size());
#endif

  expect_eq(201  , message.repeated_int32   (0));
  expect_eq(202  , message.repeated_int64   (0));
  expect_eq(203  , message.repeated_uint32  (0));
  expect_eq(204  , message.repeated_uint64  (0));
  expect_eq(205  , message.repeated_sint32  (0));
  expect_eq(206  , message.repeated_sint64  (0));
  expect_eq(207  , message.repeated_fixed32 (0));
  expect_eq(208  , message.repeated_fixed64 (0));
  expect_eq(209  , message.repeated_sfixed32(0));
  expect_eq(210  , message.repeated_sfixed64(0));
  expect_eq(211  , message.repeated_float   (0));
  expect_eq(212  , message.repeated_double  (0));
  expect_true(     message.repeated_bool    (0));
  expect_eq("215", message.repeated_string  (0));
  expect_eq("216", message.repeated_bytes   (0));

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(227, message.repeated_lazy_message   (0).bb());


  expect_eq(unittest::testalltypes::bar, message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_bar      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_bar, message.repeated_import_enum (0));

  expect_eq(301  , message.repeated_int32   (1));
  expect_eq(302  , message.repeated_int64   (1));
  expect_eq(303  , message.repeated_uint32  (1));
  expect_eq(304  , message.repeated_uint64  (1));
  expect_eq(305  , message.repeated_sint32  (1));
  expect_eq(306  , message.repeated_sint64  (1));
  expect_eq(307  , message.repeated_fixed32 (1));
  expect_eq(308  , message.repeated_fixed64 (1));
  expect_eq(309  , message.repeated_sfixed32(1));
  expect_eq(310  , message.repeated_sfixed64(1));
  expect_eq(311  , message.repeated_float   (1));
  expect_eq(312  , message.repeated_double  (1));
  expect_false(    message.repeated_bool    (1));
  expect_eq("315", message.repeated_string  (1));
  expect_eq("316", message.repeated_bytes   (1));

  expect_eq(317, message.repeatedgroup           (1).a());
  expect_eq(318, message.repeated_nested_message (1).bb());
  expect_eq(319, message.repeated_foreign_message(1).c());
  expect_eq(320, message.repeated_import_message (1).d());
  expect_eq(327, message.repeated_lazy_message   (1).bb());

  expect_eq(unittest::testalltypes::baz, message.repeated_nested_enum (1));
  expect_eq(unittest::foreign_baz      , message.repeated_foreign_enum(1));
  expect_eq(unittest_import::import_baz, message.repeated_import_enum (1));


  // -----------------------------------------------------------------

  expect_true(message.has_default_int32   ());
  expect_true(message.has_default_int64   ());
  expect_true(message.has_default_uint32  ());
  expect_true(message.has_default_uint64  ());
  expect_true(message.has_default_sint32  ());
  expect_true(message.has_default_sint64  ());
  expect_true(message.has_default_fixed32 ());
  expect_true(message.has_default_fixed64 ());
  expect_true(message.has_default_sfixed32());
  expect_true(message.has_default_sfixed64());
  expect_true(message.has_default_float   ());
  expect_true(message.has_default_double  ());
  expect_true(message.has_default_bool    ());
  expect_true(message.has_default_string  ());
  expect_true(message.has_default_bytes   ());

  expect_true(message.has_default_nested_enum ());
  expect_true(message.has_default_foreign_enum());
  expect_true(message.has_default_import_enum ());


  expect_eq(401  , message.default_int32   ());
  expect_eq(402  , message.default_int64   ());
  expect_eq(403  , message.default_uint32  ());
  expect_eq(404  , message.default_uint64  ());
  expect_eq(405  , message.default_sint32  ());
  expect_eq(406  , message.default_sint64  ());
  expect_eq(407  , message.default_fixed32 ());
  expect_eq(408  , message.default_fixed64 ());
  expect_eq(409  , message.default_sfixed32());
  expect_eq(410  , message.default_sfixed64());
  expect_eq(411  , message.default_float   ());
  expect_eq(412  , message.default_double  ());
  expect_false(    message.default_bool    ());
  expect_eq("415", message.default_string  ());
  expect_eq("416", message.default_bytes   ());

  expect_eq(unittest::testalltypes::foo, message.default_nested_enum ());
  expect_eq(unittest::foreign_foo      , message.default_foreign_enum());
  expect_eq(unittest_import::import_foo, message.default_import_enum ());

}

// -------------------------------------------------------------------

void testutil::expectclear(const unittest::testalltypes& message) {
  // has_blah() should initially be false for all optional fields.
  expect_false(message.has_optional_int32   ());
  expect_false(message.has_optional_int64   ());
  expect_false(message.has_optional_uint32  ());
  expect_false(message.has_optional_uint64  ());
  expect_false(message.has_optional_sint32  ());
  expect_false(message.has_optional_sint64  ());
  expect_false(message.has_optional_fixed32 ());
  expect_false(message.has_optional_fixed64 ());
  expect_false(message.has_optional_sfixed32());
  expect_false(message.has_optional_sfixed64());
  expect_false(message.has_optional_float   ());
  expect_false(message.has_optional_double  ());
  expect_false(message.has_optional_bool    ());
  expect_false(message.has_optional_string  ());
  expect_false(message.has_optional_bytes   ());

  expect_false(message.has_optionalgroup                 ());
  expect_false(message.has_optional_nested_message       ());
  expect_false(message.has_optional_foreign_message      ());
  expect_false(message.has_optional_import_message       ());
  expect_false(message.has_optional_public_import_message());
  expect_false(message.has_optional_lazy_message         ());

  expect_false(message.has_optional_nested_enum ());
  expect_false(message.has_optional_foreign_enum());
  expect_false(message.has_optional_import_enum ());

  expect_false(message.has_optional_string_piece());
  expect_false(message.has_optional_cord());

  // optional fields without defaults are set to zero or something like it.
  expect_eq(0    , message.optional_int32   ());
  expect_eq(0    , message.optional_int64   ());
  expect_eq(0    , message.optional_uint32  ());
  expect_eq(0    , message.optional_uint64  ());
  expect_eq(0    , message.optional_sint32  ());
  expect_eq(0    , message.optional_sint64  ());
  expect_eq(0    , message.optional_fixed32 ());
  expect_eq(0    , message.optional_fixed64 ());
  expect_eq(0    , message.optional_sfixed32());
  expect_eq(0    , message.optional_sfixed64());
  expect_eq(0    , message.optional_float   ());
  expect_eq(0    , message.optional_double  ());
  expect_false(    message.optional_bool    ());
  expect_eq(""   , message.optional_string  ());
  expect_eq(""   , message.optional_bytes   ());

  // embedded messages should also be clear.
  expect_false(message.optionalgroup                 ().has_a());
  expect_false(message.optional_nested_message       ().has_bb());
  expect_false(message.optional_foreign_message      ().has_c());
  expect_false(message.optional_import_message       ().has_d());
  expect_false(message.optional_public_import_message().has_e());
  expect_false(message.optional_lazy_message         ().has_bb());

  expect_eq(0, message.optionalgroup                 ().a());
  expect_eq(0, message.optional_nested_message       ().bb());
  expect_eq(0, message.optional_foreign_message      ().c());
  expect_eq(0, message.optional_import_message       ().d());
  expect_eq(0, message.optional_public_import_message().e());
  expect_eq(0, message.optional_lazy_message         ().bb());

  // enums without defaults are set to the first value in the enum.
  expect_eq(unittest::testalltypes::foo, message.optional_nested_enum ());
  expect_eq(unittest::foreign_foo      , message.optional_foreign_enum());
  expect_eq(unittest_import::import_foo, message.optional_import_enum ());


  // repeated fields are empty.
  expect_eq(0, message.repeated_int32_size   ());
  expect_eq(0, message.repeated_int64_size   ());
  expect_eq(0, message.repeated_uint32_size  ());
  expect_eq(0, message.repeated_uint64_size  ());
  expect_eq(0, message.repeated_sint32_size  ());
  expect_eq(0, message.repeated_sint64_size  ());
  expect_eq(0, message.repeated_fixed32_size ());
  expect_eq(0, message.repeated_fixed64_size ());
  expect_eq(0, message.repeated_sfixed32_size());
  expect_eq(0, message.repeated_sfixed64_size());
  expect_eq(0, message.repeated_float_size   ());
  expect_eq(0, message.repeated_double_size  ());
  expect_eq(0, message.repeated_bool_size    ());
  expect_eq(0, message.repeated_string_size  ());
  expect_eq(0, message.repeated_bytes_size   ());

  expect_eq(0, message.repeatedgroup_size           ());
  expect_eq(0, message.repeated_nested_message_size ());
  expect_eq(0, message.repeated_foreign_message_size());
  expect_eq(0, message.repeated_import_message_size ());
  expect_eq(0, message.repeated_lazy_message_size   ());
  expect_eq(0, message.repeated_nested_enum_size    ());
  expect_eq(0, message.repeated_foreign_enum_size   ());
  expect_eq(0, message.repeated_import_enum_size    ());

  expect_eq(0, message.repeated_string_piece_size());
  expect_eq(0, message.repeated_cord_size());

  // has_blah() should also be false for all default fields.
  expect_false(message.has_default_int32   ());
  expect_false(message.has_default_int64   ());
  expect_false(message.has_default_uint32  ());
  expect_false(message.has_default_uint64  ());
  expect_false(message.has_default_sint32  ());
  expect_false(message.has_default_sint64  ());
  expect_false(message.has_default_fixed32 ());
  expect_false(message.has_default_fixed64 ());
  expect_false(message.has_default_sfixed32());
  expect_false(message.has_default_sfixed64());
  expect_false(message.has_default_float   ());
  expect_false(message.has_default_double  ());
  expect_false(message.has_default_bool    ());
  expect_false(message.has_default_string  ());
  expect_false(message.has_default_bytes   ());

  expect_false(message.has_default_nested_enum ());
  expect_false(message.has_default_foreign_enum());
  expect_false(message.has_default_import_enum ());


  // fields with defaults have their default values (duh).
  expect_eq( 41    , message.default_int32   ());
  expect_eq( 42    , message.default_int64   ());
  expect_eq( 43    , message.default_uint32  ());
  expect_eq( 44    , message.default_uint64  ());
  expect_eq(-45    , message.default_sint32  ());
  expect_eq( 46    , message.default_sint64  ());
  expect_eq( 47    , message.default_fixed32 ());
  expect_eq( 48    , message.default_fixed64 ());
  expect_eq( 49    , message.default_sfixed32());
  expect_eq(-50    , message.default_sfixed64());
  expect_eq( 51.5  , message.default_float   ());
  expect_eq( 52e3  , message.default_double  ());
  expect_true(       message.default_bool    ());
  expect_eq("hello", message.default_string  ());
  expect_eq("world", message.default_bytes   ());

  expect_eq(unittest::testalltypes::bar, message.default_nested_enum ());
  expect_eq(unittest::foreign_bar      , message.default_foreign_enum());
  expect_eq(unittest_import::import_bar, message.default_import_enum ());

}

// -------------------------------------------------------------------

void testutil::expectrepeatedfieldsmodified(
    const unittest::testalltypes& message) {
  // modifyrepeatedfields only sets the second repeated element of each
  // field.  in addition to verifying this, we also verify that the first
  // element and size were *not* modified.
  assert_eq(2, message.repeated_int32_size   ());
  assert_eq(2, message.repeated_int64_size   ());
  assert_eq(2, message.repeated_uint32_size  ());
  assert_eq(2, message.repeated_uint64_size  ());
  assert_eq(2, message.repeated_sint32_size  ());
  assert_eq(2, message.repeated_sint64_size  ());
  assert_eq(2, message.repeated_fixed32_size ());
  assert_eq(2, message.repeated_fixed64_size ());
  assert_eq(2, message.repeated_sfixed32_size());
  assert_eq(2, message.repeated_sfixed64_size());
  assert_eq(2, message.repeated_float_size   ());
  assert_eq(2, message.repeated_double_size  ());
  assert_eq(2, message.repeated_bool_size    ());
  assert_eq(2, message.repeated_string_size  ());
  assert_eq(2, message.repeated_bytes_size   ());

  assert_eq(2, message.repeatedgroup_size           ());
  assert_eq(2, message.repeated_nested_message_size ());
  assert_eq(2, message.repeated_foreign_message_size());
  assert_eq(2, message.repeated_import_message_size ());
  assert_eq(2, message.repeated_lazy_message_size   ());
  assert_eq(2, message.repeated_nested_enum_size    ());
  assert_eq(2, message.repeated_foreign_enum_size   ());
  assert_eq(2, message.repeated_import_enum_size    ());

#ifndef protobuf_test_no_descriptors
  assert_eq(2, message.repeated_string_piece_size());
  assert_eq(2, message.repeated_cord_size());
#endif

  expect_eq(201  , message.repeated_int32   (0));
  expect_eq(202  , message.repeated_int64   (0));
  expect_eq(203  , message.repeated_uint32  (0));
  expect_eq(204  , message.repeated_uint64  (0));
  expect_eq(205  , message.repeated_sint32  (0));
  expect_eq(206  , message.repeated_sint64  (0));
  expect_eq(207  , message.repeated_fixed32 (0));
  expect_eq(208  , message.repeated_fixed64 (0));
  expect_eq(209  , message.repeated_sfixed32(0));
  expect_eq(210  , message.repeated_sfixed64(0));
  expect_eq(211  , message.repeated_float   (0));
  expect_eq(212  , message.repeated_double  (0));
  expect_true(     message.repeated_bool    (0));
  expect_eq("215", message.repeated_string  (0));
  expect_eq("216", message.repeated_bytes   (0));

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(227, message.repeated_lazy_message   (0).bb());

  expect_eq(unittest::testalltypes::bar, message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_bar      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_bar, message.repeated_import_enum (0));


  // actually verify the second (modified) elements now.
  expect_eq(501  , message.repeated_int32   (1));
  expect_eq(502  , message.repeated_int64   (1));
  expect_eq(503  , message.repeated_uint32  (1));
  expect_eq(504  , message.repeated_uint64  (1));
  expect_eq(505  , message.repeated_sint32  (1));
  expect_eq(506  , message.repeated_sint64  (1));
  expect_eq(507  , message.repeated_fixed32 (1));
  expect_eq(508  , message.repeated_fixed64 (1));
  expect_eq(509  , message.repeated_sfixed32(1));
  expect_eq(510  , message.repeated_sfixed64(1));
  expect_eq(511  , message.repeated_float   (1));
  expect_eq(512  , message.repeated_double  (1));
  expect_true(     message.repeated_bool    (1));
  expect_eq("515", message.repeated_string  (1));
  expect_eq("516", message.repeated_bytes   (1));

  expect_eq(517, message.repeatedgroup           (1).a());
  expect_eq(518, message.repeated_nested_message (1).bb());
  expect_eq(519, message.repeated_foreign_message(1).c());
  expect_eq(520, message.repeated_import_message (1).d());
  expect_eq(527, message.repeated_lazy_message   (1).bb());

  expect_eq(unittest::testalltypes::foo, message.repeated_nested_enum (1));
  expect_eq(unittest::foreign_foo      , message.repeated_foreign_enum(1));
  expect_eq(unittest_import::import_foo, message.repeated_import_enum (1));

}

// -------------------------------------------------------------------

void testutil::setpackedfields(unittest::testpackedtypes* message) {
  message->add_packed_int32   (601);
  message->add_packed_int64   (602);
  message->add_packed_uint32  (603);
  message->add_packed_uint64  (604);
  message->add_packed_sint32  (605);
  message->add_packed_sint64  (606);
  message->add_packed_fixed32 (607);
  message->add_packed_fixed64 (608);
  message->add_packed_sfixed32(609);
  message->add_packed_sfixed64(610);
  message->add_packed_float   (611);
  message->add_packed_double  (612);
  message->add_packed_bool    (true);
  message->add_packed_enum    (unittest::foreign_bar);
  // add a second one of each field
  message->add_packed_int32   (701);
  message->add_packed_int64   (702);
  message->add_packed_uint32  (703);
  message->add_packed_uint64  (704);
  message->add_packed_sint32  (705);
  message->add_packed_sint64  (706);
  message->add_packed_fixed32 (707);
  message->add_packed_fixed64 (708);
  message->add_packed_sfixed32(709);
  message->add_packed_sfixed64(710);
  message->add_packed_float   (711);
  message->add_packed_double  (712);
  message->add_packed_bool    (false);
  message->add_packed_enum    (unittest::foreign_baz);
}

void testutil::setunpackedfields(unittest::testunpackedtypes* message) {
  // the values applied here must match those of setpackedfields.

  message->add_unpacked_int32   (601);
  message->add_unpacked_int64   (602);
  message->add_unpacked_uint32  (603);
  message->add_unpacked_uint64  (604);
  message->add_unpacked_sint32  (605);
  message->add_unpacked_sint64  (606);
  message->add_unpacked_fixed32 (607);
  message->add_unpacked_fixed64 (608);
  message->add_unpacked_sfixed32(609);
  message->add_unpacked_sfixed64(610);
  message->add_unpacked_float   (611);
  message->add_unpacked_double  (612);
  message->add_unpacked_bool    (true);
  message->add_unpacked_enum    (unittest::foreign_bar);
  // add a second one of each field
  message->add_unpacked_int32   (701);
  message->add_unpacked_int64   (702);
  message->add_unpacked_uint32  (703);
  message->add_unpacked_uint64  (704);
  message->add_unpacked_sint32  (705);
  message->add_unpacked_sint64  (706);
  message->add_unpacked_fixed32 (707);
  message->add_unpacked_fixed64 (708);
  message->add_unpacked_sfixed32(709);
  message->add_unpacked_sfixed64(710);
  message->add_unpacked_float   (711);
  message->add_unpacked_double  (712);
  message->add_unpacked_bool    (false);
  message->add_unpacked_enum    (unittest::foreign_baz);
}

// -------------------------------------------------------------------

void testutil::modifypackedfields(unittest::testpackedtypes* message) {
  message->set_packed_int32   (1, 801);
  message->set_packed_int64   (1, 802);
  message->set_packed_uint32  (1, 803);
  message->set_packed_uint64  (1, 804);
  message->set_packed_sint32  (1, 805);
  message->set_packed_sint64  (1, 806);
  message->set_packed_fixed32 (1, 807);
  message->set_packed_fixed64 (1, 808);
  message->set_packed_sfixed32(1, 809);
  message->set_packed_sfixed64(1, 810);
  message->set_packed_float   (1, 811);
  message->set_packed_double  (1, 812);
  message->set_packed_bool    (1, true);
  message->set_packed_enum    (1, unittest::foreign_foo);
}

// -------------------------------------------------------------------

void testutil::expectpackedfieldsset(const unittest::testpackedtypes& message) {
  assert_eq(2, message.packed_int32_size   ());
  assert_eq(2, message.packed_int64_size   ());
  assert_eq(2, message.packed_uint32_size  ());
  assert_eq(2, message.packed_uint64_size  ());
  assert_eq(2, message.packed_sint32_size  ());
  assert_eq(2, message.packed_sint64_size  ());
  assert_eq(2, message.packed_fixed32_size ());
  assert_eq(2, message.packed_fixed64_size ());
  assert_eq(2, message.packed_sfixed32_size());
  assert_eq(2, message.packed_sfixed64_size());
  assert_eq(2, message.packed_float_size   ());
  assert_eq(2, message.packed_double_size  ());
  assert_eq(2, message.packed_bool_size    ());
  assert_eq(2, message.packed_enum_size    ());

  expect_eq(601  , message.packed_int32   (0));
  expect_eq(602  , message.packed_int64   (0));
  expect_eq(603  , message.packed_uint32  (0));
  expect_eq(604  , message.packed_uint64  (0));
  expect_eq(605  , message.packed_sint32  (0));
  expect_eq(606  , message.packed_sint64  (0));
  expect_eq(607  , message.packed_fixed32 (0));
  expect_eq(608  , message.packed_fixed64 (0));
  expect_eq(609  , message.packed_sfixed32(0));
  expect_eq(610  , message.packed_sfixed64(0));
  expect_eq(611  , message.packed_float   (0));
  expect_eq(612  , message.packed_double  (0));
  expect_true(     message.packed_bool    (0));
  expect_eq(unittest::foreign_bar, message.packed_enum(0));

  expect_eq(701  , message.packed_int32   (1));
  expect_eq(702  , message.packed_int64   (1));
  expect_eq(703  , message.packed_uint32  (1));
  expect_eq(704  , message.packed_uint64  (1));
  expect_eq(705  , message.packed_sint32  (1));
  expect_eq(706  , message.packed_sint64  (1));
  expect_eq(707  , message.packed_fixed32 (1));
  expect_eq(708  , message.packed_fixed64 (1));
  expect_eq(709  , message.packed_sfixed32(1));
  expect_eq(710  , message.packed_sfixed64(1));
  expect_eq(711  , message.packed_float   (1));
  expect_eq(712  , message.packed_double  (1));
  expect_false(    message.packed_bool    (1));
  expect_eq(unittest::foreign_baz, message.packed_enum(1));
}

void testutil::expectunpackedfieldsset(
    const unittest::testunpackedtypes& message) {
  // the values expected here must match those of expectpackedfieldsset.

  assert_eq(2, message.unpacked_int32_size   ());
  assert_eq(2, message.unpacked_int64_size   ());
  assert_eq(2, message.unpacked_uint32_size  ());
  assert_eq(2, message.unpacked_uint64_size  ());
  assert_eq(2, message.unpacked_sint32_size  ());
  assert_eq(2, message.unpacked_sint64_size  ());
  assert_eq(2, message.unpacked_fixed32_size ());
  assert_eq(2, message.unpacked_fixed64_size ());
  assert_eq(2, message.unpacked_sfixed32_size());
  assert_eq(2, message.unpacked_sfixed64_size());
  assert_eq(2, message.unpacked_float_size   ());
  assert_eq(2, message.unpacked_double_size  ());
  assert_eq(2, message.unpacked_bool_size    ());
  assert_eq(2, message.unpacked_enum_size    ());

  expect_eq(601  , message.unpacked_int32   (0));
  expect_eq(602  , message.unpacked_int64   (0));
  expect_eq(603  , message.unpacked_uint32  (0));
  expect_eq(604  , message.unpacked_uint64  (0));
  expect_eq(605  , message.unpacked_sint32  (0));
  expect_eq(606  , message.unpacked_sint64  (0));
  expect_eq(607  , message.unpacked_fixed32 (0));
  expect_eq(608  , message.unpacked_fixed64 (0));
  expect_eq(609  , message.unpacked_sfixed32(0));
  expect_eq(610  , message.unpacked_sfixed64(0));
  expect_eq(611  , message.unpacked_float   (0));
  expect_eq(612  , message.unpacked_double  (0));
  expect_true(     message.unpacked_bool    (0));
  expect_eq(unittest::foreign_bar, message.unpacked_enum(0));

  expect_eq(701  , message.unpacked_int32   (1));
  expect_eq(702  , message.unpacked_int64   (1));
  expect_eq(703  , message.unpacked_uint32  (1));
  expect_eq(704  , message.unpacked_uint64  (1));
  expect_eq(705  , message.unpacked_sint32  (1));
  expect_eq(706  , message.unpacked_sint64  (1));
  expect_eq(707  , message.unpacked_fixed32 (1));
  expect_eq(708  , message.unpacked_fixed64 (1));
  expect_eq(709  , message.unpacked_sfixed32(1));
  expect_eq(710  , message.unpacked_sfixed64(1));
  expect_eq(711  , message.unpacked_float   (1));
  expect_eq(712  , message.unpacked_double  (1));
  expect_false(    message.unpacked_bool    (1));
  expect_eq(unittest::foreign_baz, message.unpacked_enum(1));
}

// -------------------------------------------------------------------

void testutil::expectpackedclear(
    const unittest::testpackedtypes& message) {
  // packed repeated fields are empty.
  expect_eq(0, message.packed_int32_size   ());
  expect_eq(0, message.packed_int64_size   ());
  expect_eq(0, message.packed_uint32_size  ());
  expect_eq(0, message.packed_uint64_size  ());
  expect_eq(0, message.packed_sint32_size  ());
  expect_eq(0, message.packed_sint64_size  ());
  expect_eq(0, message.packed_fixed32_size ());
  expect_eq(0, message.packed_fixed64_size ());
  expect_eq(0, message.packed_sfixed32_size());
  expect_eq(0, message.packed_sfixed64_size());
  expect_eq(0, message.packed_float_size   ());
  expect_eq(0, message.packed_double_size  ());
  expect_eq(0, message.packed_bool_size    ());
  expect_eq(0, message.packed_enum_size    ());
}

// -------------------------------------------------------------------

void testutil::expectpackedfieldsmodified(
    const unittest::testpackedtypes& message) {
  // do the same for packed repeated fields.
  assert_eq(2, message.packed_int32_size   ());
  assert_eq(2, message.packed_int64_size   ());
  assert_eq(2, message.packed_uint32_size  ());
  assert_eq(2, message.packed_uint64_size  ());
  assert_eq(2, message.packed_sint32_size  ());
  assert_eq(2, message.packed_sint64_size  ());
  assert_eq(2, message.packed_fixed32_size ());
  assert_eq(2, message.packed_fixed64_size ());
  assert_eq(2, message.packed_sfixed32_size());
  assert_eq(2, message.packed_sfixed64_size());
  assert_eq(2, message.packed_float_size   ());
  assert_eq(2, message.packed_double_size  ());
  assert_eq(2, message.packed_bool_size    ());
  assert_eq(2, message.packed_enum_size    ());

  expect_eq(601  , message.packed_int32   (0));
  expect_eq(602  , message.packed_int64   (0));
  expect_eq(603  , message.packed_uint32  (0));
  expect_eq(604  , message.packed_uint64  (0));
  expect_eq(605  , message.packed_sint32  (0));
  expect_eq(606  , message.packed_sint64  (0));
  expect_eq(607  , message.packed_fixed32 (0));
  expect_eq(608  , message.packed_fixed64 (0));
  expect_eq(609  , message.packed_sfixed32(0));
  expect_eq(610  , message.packed_sfixed64(0));
  expect_eq(611  , message.packed_float   (0));
  expect_eq(612  , message.packed_double  (0));
  expect_true(     message.packed_bool    (0));
  expect_eq(unittest::foreign_bar, message.packed_enum(0));
  // actually verify the second (modified) elements now.
  expect_eq(801  , message.packed_int32   (1));
  expect_eq(802  , message.packed_int64   (1));
  expect_eq(803  , message.packed_uint32  (1));
  expect_eq(804  , message.packed_uint64  (1));
  expect_eq(805  , message.packed_sint32  (1));
  expect_eq(806  , message.packed_sint64  (1));
  expect_eq(807  , message.packed_fixed32 (1));
  expect_eq(808  , message.packed_fixed64 (1));
  expect_eq(809  , message.packed_sfixed32(1));
  expect_eq(810  , message.packed_sfixed64(1));
  expect_eq(811  , message.packed_float   (1));
  expect_eq(812  , message.packed_double  (1));
  expect_true(     message.packed_bool    (1));
  expect_eq(unittest::foreign_foo, message.packed_enum(1));
}

// ===================================================================
// extensions
//
// all this code is exactly equivalent to the above code except that it's
// manipulating extension fields instead of normal ones.
//
// i gave up on the 80-char limit here.  sorry.

void testutil::setallextensions(unittest::testallextensions* message) {
  message->setextension(unittest::optional_int32_extension   , 101);
  message->setextension(unittest::optional_int64_extension   , 102);
  message->setextension(unittest::optional_uint32_extension  , 103);
  message->setextension(unittest::optional_uint64_extension  , 104);
  message->setextension(unittest::optional_sint32_extension  , 105);
  message->setextension(unittest::optional_sint64_extension  , 106);
  message->setextension(unittest::optional_fixed32_extension , 107);
  message->setextension(unittest::optional_fixed64_extension , 108);
  message->setextension(unittest::optional_sfixed32_extension, 109);
  message->setextension(unittest::optional_sfixed64_extension, 110);
  message->setextension(unittest::optional_float_extension   , 111);
  message->setextension(unittest::optional_double_extension  , 112);
  message->setextension(unittest::optional_bool_extension    , true);
  message->setextension(unittest::optional_string_extension  , "115");
  message->setextension(unittest::optional_bytes_extension   , "116");

  message->mutableextension(unittest::optionalgroup_extension           )->set_a(117);
  message->mutableextension(unittest::optional_nested_message_extension )->set_bb(118);
  message->mutableextension(unittest::optional_foreign_message_extension)->set_c(119);
  message->mutableextension(unittest::optional_import_message_extension )->set_d(120);

  message->setextension(unittest::optional_nested_enum_extension , unittest::testalltypes::baz);
  message->setextension(unittest::optional_foreign_enum_extension, unittest::foreign_baz      );
  message->setextension(unittest::optional_import_enum_extension , unittest_import::import_baz);

  message->setextension(unittest::optional_string_piece_extension, "124");
  message->setextension(unittest::optional_cord_extension, "125");

  message->mutableextension(unittest::optional_public_import_message_extension)->set_e(126);
  message->mutableextension(unittest::optional_lazy_message_extension)->set_bb(127);

  // -----------------------------------------------------------------

  message->addextension(unittest::repeated_int32_extension   , 201);
  message->addextension(unittest::repeated_int64_extension   , 202);
  message->addextension(unittest::repeated_uint32_extension  , 203);
  message->addextension(unittest::repeated_uint64_extension  , 204);
  message->addextension(unittest::repeated_sint32_extension  , 205);
  message->addextension(unittest::repeated_sint64_extension  , 206);
  message->addextension(unittest::repeated_fixed32_extension , 207);
  message->addextension(unittest::repeated_fixed64_extension , 208);
  message->addextension(unittest::repeated_sfixed32_extension, 209);
  message->addextension(unittest::repeated_sfixed64_extension, 210);
  message->addextension(unittest::repeated_float_extension   , 211);
  message->addextension(unittest::repeated_double_extension  , 212);
  message->addextension(unittest::repeated_bool_extension    , true);
  message->addextension(unittest::repeated_string_extension  , "215");
  message->addextension(unittest::repeated_bytes_extension   , "216");

  message->addextension(unittest::repeatedgroup_extension           )->set_a(217);
  message->addextension(unittest::repeated_nested_message_extension )->set_bb(218);
  message->addextension(unittest::repeated_foreign_message_extension)->set_c(219);
  message->addextension(unittest::repeated_import_message_extension )->set_d(220);
  message->addextension(unittest::repeated_lazy_message_extension   )->set_bb(227);

  message->addextension(unittest::repeated_nested_enum_extension , unittest::testalltypes::bar);
  message->addextension(unittest::repeated_foreign_enum_extension, unittest::foreign_bar      );
  message->addextension(unittest::repeated_import_enum_extension , unittest_import::import_bar);

  message->addextension(unittest::repeated_string_piece_extension, "224");
  message->addextension(unittest::repeated_cord_extension, "225");

  // add a second one of each field.
  message->addextension(unittest::repeated_int32_extension   , 301);
  message->addextension(unittest::repeated_int64_extension   , 302);
  message->addextension(unittest::repeated_uint32_extension  , 303);
  message->addextension(unittest::repeated_uint64_extension  , 304);
  message->addextension(unittest::repeated_sint32_extension  , 305);
  message->addextension(unittest::repeated_sint64_extension  , 306);
  message->addextension(unittest::repeated_fixed32_extension , 307);
  message->addextension(unittest::repeated_fixed64_extension , 308);
  message->addextension(unittest::repeated_sfixed32_extension, 309);
  message->addextension(unittest::repeated_sfixed64_extension, 310);
  message->addextension(unittest::repeated_float_extension   , 311);
  message->addextension(unittest::repeated_double_extension  , 312);
  message->addextension(unittest::repeated_bool_extension    , false);
  message->addextension(unittest::repeated_string_extension  , "315");
  message->addextension(unittest::repeated_bytes_extension   , "316");

  message->addextension(unittest::repeatedgroup_extension           )->set_a(317);
  message->addextension(unittest::repeated_nested_message_extension )->set_bb(318);
  message->addextension(unittest::repeated_foreign_message_extension)->set_c(319);
  message->addextension(unittest::repeated_import_message_extension )->set_d(320);
  message->addextension(unittest::repeated_lazy_message_extension   )->set_bb(327);

  message->addextension(unittest::repeated_nested_enum_extension , unittest::testalltypes::baz);
  message->addextension(unittest::repeated_foreign_enum_extension, unittest::foreign_baz      );
  message->addextension(unittest::repeated_import_enum_extension , unittest_import::import_baz);

  message->addextension(unittest::repeated_string_piece_extension, "324");
  message->addextension(unittest::repeated_cord_extension, "325");

  // -----------------------------------------------------------------

  message->setextension(unittest::default_int32_extension   , 401);
  message->setextension(unittest::default_int64_extension   , 402);
  message->setextension(unittest::default_uint32_extension  , 403);
  message->setextension(unittest::default_uint64_extension  , 404);
  message->setextension(unittest::default_sint32_extension  , 405);
  message->setextension(unittest::default_sint64_extension  , 406);
  message->setextension(unittest::default_fixed32_extension , 407);
  message->setextension(unittest::default_fixed64_extension , 408);
  message->setextension(unittest::default_sfixed32_extension, 409);
  message->setextension(unittest::default_sfixed64_extension, 410);
  message->setextension(unittest::default_float_extension   , 411);
  message->setextension(unittest::default_double_extension  , 412);
  message->setextension(unittest::default_bool_extension    , false);
  message->setextension(unittest::default_string_extension  , "415");
  message->setextension(unittest::default_bytes_extension   , "416");

  message->setextension(unittest::default_nested_enum_extension , unittest::testalltypes::foo);
  message->setextension(unittest::default_foreign_enum_extension, unittest::foreign_foo      );
  message->setextension(unittest::default_import_enum_extension , unittest_import::import_foo);

  message->setextension(unittest::default_string_piece_extension, "424");
  message->setextension(unittest::default_cord_extension, "425");
}

// -------------------------------------------------------------------

void testutil::setallfieldsandextensions(
    unittest::testfieldorderings* message) {
  google_check(message);
  message->set_my_int(1);
  message->set_my_string("foo");
  message->set_my_float(1.0);
  message->setextension(unittest::my_extension_int, 23);
  message->setextension(unittest::my_extension_string, "bar");
}

// -------------------------------------------------------------------

void testutil::modifyrepeatedextensions(unittest::testallextensions* message) {
  message->setextension(unittest::repeated_int32_extension   , 1, 501);
  message->setextension(unittest::repeated_int64_extension   , 1, 502);
  message->setextension(unittest::repeated_uint32_extension  , 1, 503);
  message->setextension(unittest::repeated_uint64_extension  , 1, 504);
  message->setextension(unittest::repeated_sint32_extension  , 1, 505);
  message->setextension(unittest::repeated_sint64_extension  , 1, 506);
  message->setextension(unittest::repeated_fixed32_extension , 1, 507);
  message->setextension(unittest::repeated_fixed64_extension , 1, 508);
  message->setextension(unittest::repeated_sfixed32_extension, 1, 509);
  message->setextension(unittest::repeated_sfixed64_extension, 1, 510);
  message->setextension(unittest::repeated_float_extension   , 1, 511);
  message->setextension(unittest::repeated_double_extension  , 1, 512);
  message->setextension(unittest::repeated_bool_extension    , 1, true);
  message->setextension(unittest::repeated_string_extension  , 1, "515");
  message->setextension(unittest::repeated_bytes_extension   , 1, "516");

  message->mutableextension(unittest::repeatedgroup_extension           , 1)->set_a(517);
  message->mutableextension(unittest::repeated_nested_message_extension , 1)->set_bb(518);
  message->mutableextension(unittest::repeated_foreign_message_extension, 1)->set_c(519);
  message->mutableextension(unittest::repeated_import_message_extension , 1)->set_d(520);
  message->mutableextension(unittest::repeated_lazy_message_extension   , 1)->set_bb(527);

  message->setextension(unittest::repeated_nested_enum_extension , 1, unittest::testalltypes::foo);
  message->setextension(unittest::repeated_foreign_enum_extension, 1, unittest::foreign_foo      );
  message->setextension(unittest::repeated_import_enum_extension , 1, unittest_import::import_foo);

  message->setextension(unittest::repeated_string_piece_extension, 1, "524");
  message->setextension(unittest::repeated_cord_extension, 1, "525");
}

// -------------------------------------------------------------------

void testutil::expectallextensionsset(
    const unittest::testallextensions& message) {
  expect_true(message.hasextension(unittest::optional_int32_extension   ));
  expect_true(message.hasextension(unittest::optional_int64_extension   ));
  expect_true(message.hasextension(unittest::optional_uint32_extension  ));
  expect_true(message.hasextension(unittest::optional_uint64_extension  ));
  expect_true(message.hasextension(unittest::optional_sint32_extension  ));
  expect_true(message.hasextension(unittest::optional_sint64_extension  ));
  expect_true(message.hasextension(unittest::optional_fixed32_extension ));
  expect_true(message.hasextension(unittest::optional_fixed64_extension ));
  expect_true(message.hasextension(unittest::optional_sfixed32_extension));
  expect_true(message.hasextension(unittest::optional_sfixed64_extension));
  expect_true(message.hasextension(unittest::optional_float_extension   ));
  expect_true(message.hasextension(unittest::optional_double_extension  ));
  expect_true(message.hasextension(unittest::optional_bool_extension    ));
  expect_true(message.hasextension(unittest::optional_string_extension  ));
  expect_true(message.hasextension(unittest::optional_bytes_extension   ));

  expect_true(message.hasextension(unittest::optionalgroup_extension                 ));
  expect_true(message.hasextension(unittest::optional_nested_message_extension       ));
  expect_true(message.hasextension(unittest::optional_foreign_message_extension      ));
  expect_true(message.hasextension(unittest::optional_import_message_extension       ));
  expect_true(message.hasextension(unittest::optional_public_import_message_extension));
  expect_true(message.hasextension(unittest::optional_lazy_message_extension         ));

  expect_true(message.getextension(unittest::optionalgroup_extension                 ).has_a());
  expect_true(message.getextension(unittest::optional_nested_message_extension       ).has_bb());
  expect_true(message.getextension(unittest::optional_foreign_message_extension      ).has_c());
  expect_true(message.getextension(unittest::optional_import_message_extension       ).has_d());
  expect_true(message.getextension(unittest::optional_public_import_message_extension).has_e());
  expect_true(message.getextension(unittest::optional_lazy_message_extension         ).has_bb());

  expect_true(message.hasextension(unittest::optional_nested_enum_extension ));
  expect_true(message.hasextension(unittest::optional_foreign_enum_extension));
  expect_true(message.hasextension(unittest::optional_import_enum_extension ));

  expect_true(message.hasextension(unittest::optional_string_piece_extension));
  expect_true(message.hasextension(unittest::optional_cord_extension));

  expect_eq(101  , message.getextension(unittest::optional_int32_extension   ));
  expect_eq(102  , message.getextension(unittest::optional_int64_extension   ));
  expect_eq(103  , message.getextension(unittest::optional_uint32_extension  ));
  expect_eq(104  , message.getextension(unittest::optional_uint64_extension  ));
  expect_eq(105  , message.getextension(unittest::optional_sint32_extension  ));
  expect_eq(106  , message.getextension(unittest::optional_sint64_extension  ));
  expect_eq(107  , message.getextension(unittest::optional_fixed32_extension ));
  expect_eq(108  , message.getextension(unittest::optional_fixed64_extension ));
  expect_eq(109  , message.getextension(unittest::optional_sfixed32_extension));
  expect_eq(110  , message.getextension(unittest::optional_sfixed64_extension));
  expect_eq(111  , message.getextension(unittest::optional_float_extension   ));
  expect_eq(112  , message.getextension(unittest::optional_double_extension  ));
  expect_true(     message.getextension(unittest::optional_bool_extension    ));
  expect_eq("115", message.getextension(unittest::optional_string_extension  ));
  expect_eq("116", message.getextension(unittest::optional_bytes_extension   ));

  expect_eq(117, message.getextension(unittest::optionalgroup_extension           ).a());
  expect_eq(118, message.getextension(unittest::optional_nested_message_extension ).bb());
  expect_eq(119, message.getextension(unittest::optional_foreign_message_extension).c());
  expect_eq(120, message.getextension(unittest::optional_import_message_extension ).d());

  expect_eq(unittest::testalltypes::baz, message.getextension(unittest::optional_nested_enum_extension ));
  expect_eq(unittest::foreign_baz      , message.getextension(unittest::optional_foreign_enum_extension));
  expect_eq(unittest_import::import_baz, message.getextension(unittest::optional_import_enum_extension ));

  expect_eq("124", message.getextension(unittest::optional_string_piece_extension));
  expect_eq("125", message.getextension(unittest::optional_cord_extension));
  expect_eq(126, message.getextension(unittest::optional_public_import_message_extension ).e());
  expect_eq(127, message.getextension(unittest::optional_lazy_message_extension).bb());

  // -----------------------------------------------------------------

  assert_eq(2, message.extensionsize(unittest::repeated_int32_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_int64_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed32_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed64_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed32_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed64_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_float_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_double_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bool_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_string_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bytes_extension   ));

  assert_eq(2, message.extensionsize(unittest::repeatedgroup_extension           ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_message_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_import_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_lazy_message_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_enum_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_enum_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_import_enum_extension    ));

  assert_eq(2, message.extensionsize(unittest::repeated_string_piece_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_cord_extension));

  expect_eq(201  , message.getextension(unittest::repeated_int32_extension   , 0));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension   , 0));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension  , 0));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension  , 0));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension  , 0));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension  , 0));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension , 0));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension , 0));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension, 0));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension, 0));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension   , 0));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension  , 0));
  expect_true(     message.getextension(unittest::repeated_bool_extension    , 0));
  expect_eq("215", message.getextension(unittest::repeated_string_extension  , 0));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension   , 0));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension   , 0).bb());

  expect_eq(unittest::testalltypes::bar, message.getextension(unittest::repeated_nested_enum_extension , 0));
  expect_eq(unittest::foreign_bar      , message.getextension(unittest::repeated_foreign_enum_extension, 0));
  expect_eq(unittest_import::import_bar, message.getextension(unittest::repeated_import_enum_extension , 0));

  expect_eq("224", message.getextension(unittest::repeated_string_piece_extension, 0));
  expect_eq("225", message.getextension(unittest::repeated_cord_extension, 0));

  expect_eq(301  , message.getextension(unittest::repeated_int32_extension   , 1));
  expect_eq(302  , message.getextension(unittest::repeated_int64_extension   , 1));
  expect_eq(303  , message.getextension(unittest::repeated_uint32_extension  , 1));
  expect_eq(304  , message.getextension(unittest::repeated_uint64_extension  , 1));
  expect_eq(305  , message.getextension(unittest::repeated_sint32_extension  , 1));
  expect_eq(306  , message.getextension(unittest::repeated_sint64_extension  , 1));
  expect_eq(307  , message.getextension(unittest::repeated_fixed32_extension , 1));
  expect_eq(308  , message.getextension(unittest::repeated_fixed64_extension , 1));
  expect_eq(309  , message.getextension(unittest::repeated_sfixed32_extension, 1));
  expect_eq(310  , message.getextension(unittest::repeated_sfixed64_extension, 1));
  expect_eq(311  , message.getextension(unittest::repeated_float_extension   , 1));
  expect_eq(312  , message.getextension(unittest::repeated_double_extension  , 1));
  expect_false(    message.getextension(unittest::repeated_bool_extension    , 1));
  expect_eq("315", message.getextension(unittest::repeated_string_extension  , 1));
  expect_eq("316", message.getextension(unittest::repeated_bytes_extension   , 1));

  expect_eq(317, message.getextension(unittest::repeatedgroup_extension           , 1).a());
  expect_eq(318, message.getextension(unittest::repeated_nested_message_extension , 1).bb());
  expect_eq(319, message.getextension(unittest::repeated_foreign_message_extension, 1).c());
  expect_eq(320, message.getextension(unittest::repeated_import_message_extension , 1).d());
  expect_eq(327, message.getextension(unittest::repeated_lazy_message_extension   , 1).bb());

  expect_eq(unittest::testalltypes::baz, message.getextension(unittest::repeated_nested_enum_extension , 1));
  expect_eq(unittest::foreign_baz      , message.getextension(unittest::repeated_foreign_enum_extension, 1));
  expect_eq(unittest_import::import_baz, message.getextension(unittest::repeated_import_enum_extension , 1));

  expect_eq("324", message.getextension(unittest::repeated_string_piece_extension, 1));
  expect_eq("325", message.getextension(unittest::repeated_cord_extension, 1));

  // -----------------------------------------------------------------

  expect_true(message.hasextension(unittest::default_int32_extension   ));
  expect_true(message.hasextension(unittest::default_int64_extension   ));
  expect_true(message.hasextension(unittest::default_uint32_extension  ));
  expect_true(message.hasextension(unittest::default_uint64_extension  ));
  expect_true(message.hasextension(unittest::default_sint32_extension  ));
  expect_true(message.hasextension(unittest::default_sint64_extension  ));
  expect_true(message.hasextension(unittest::default_fixed32_extension ));
  expect_true(message.hasextension(unittest::default_fixed64_extension ));
  expect_true(message.hasextension(unittest::default_sfixed32_extension));
  expect_true(message.hasextension(unittest::default_sfixed64_extension));
  expect_true(message.hasextension(unittest::default_float_extension   ));
  expect_true(message.hasextension(unittest::default_double_extension  ));
  expect_true(message.hasextension(unittest::default_bool_extension    ));
  expect_true(message.hasextension(unittest::default_string_extension  ));
  expect_true(message.hasextension(unittest::default_bytes_extension   ));

  expect_true(message.hasextension(unittest::default_nested_enum_extension ));
  expect_true(message.hasextension(unittest::default_foreign_enum_extension));
  expect_true(message.hasextension(unittest::default_import_enum_extension ));

  expect_true(message.hasextension(unittest::default_string_piece_extension));
  expect_true(message.hasextension(unittest::default_cord_extension));

  expect_eq(401  , message.getextension(unittest::default_int32_extension   ));
  expect_eq(402  , message.getextension(unittest::default_int64_extension   ));
  expect_eq(403  , message.getextension(unittest::default_uint32_extension  ));
  expect_eq(404  , message.getextension(unittest::default_uint64_extension  ));
  expect_eq(405  , message.getextension(unittest::default_sint32_extension  ));
  expect_eq(406  , message.getextension(unittest::default_sint64_extension  ));
  expect_eq(407  , message.getextension(unittest::default_fixed32_extension ));
  expect_eq(408  , message.getextension(unittest::default_fixed64_extension ));
  expect_eq(409  , message.getextension(unittest::default_sfixed32_extension));
  expect_eq(410  , message.getextension(unittest::default_sfixed64_extension));
  expect_eq(411  , message.getextension(unittest::default_float_extension   ));
  expect_eq(412  , message.getextension(unittest::default_double_extension  ));
  expect_false(    message.getextension(unittest::default_bool_extension    ));
  expect_eq("415", message.getextension(unittest::default_string_extension  ));
  expect_eq("416", message.getextension(unittest::default_bytes_extension   ));

  expect_eq(unittest::testalltypes::foo, message.getextension(unittest::default_nested_enum_extension ));
  expect_eq(unittest::foreign_foo      , message.getextension(unittest::default_foreign_enum_extension));
  expect_eq(unittest_import::import_foo, message.getextension(unittest::default_import_enum_extension ));

  expect_eq("424", message.getextension(unittest::default_string_piece_extension));
  expect_eq("425", message.getextension(unittest::default_cord_extension));
}

// -------------------------------------------------------------------

void testutil::expectextensionsclear(
    const unittest::testallextensions& message) {
  string serialized;
  assert_true(message.serializetostring(&serialized));
  expect_eq("", serialized);
  expect_eq(0, message.bytesize());

  // has_blah() should initially be false for all optional fields.
  expect_false(message.hasextension(unittest::optional_int32_extension   ));
  expect_false(message.hasextension(unittest::optional_int64_extension   ));
  expect_false(message.hasextension(unittest::optional_uint32_extension  ));
  expect_false(message.hasextension(unittest::optional_uint64_extension  ));
  expect_false(message.hasextension(unittest::optional_sint32_extension  ));
  expect_false(message.hasextension(unittest::optional_sint64_extension  ));
  expect_false(message.hasextension(unittest::optional_fixed32_extension ));
  expect_false(message.hasextension(unittest::optional_fixed64_extension ));
  expect_false(message.hasextension(unittest::optional_sfixed32_extension));
  expect_false(message.hasextension(unittest::optional_sfixed64_extension));
  expect_false(message.hasextension(unittest::optional_float_extension   ));
  expect_false(message.hasextension(unittest::optional_double_extension  ));
  expect_false(message.hasextension(unittest::optional_bool_extension    ));
  expect_false(message.hasextension(unittest::optional_string_extension  ));
  expect_false(message.hasextension(unittest::optional_bytes_extension   ));

  expect_false(message.hasextension(unittest::optionalgroup_extension                 ));
  expect_false(message.hasextension(unittest::optional_nested_message_extension       ));
  expect_false(message.hasextension(unittest::optional_foreign_message_extension      ));
  expect_false(message.hasextension(unittest::optional_import_message_extension       ));
  expect_false(message.hasextension(unittest::optional_public_import_message_extension));
  expect_false(message.hasextension(unittest::optional_lazy_message_extension         ));

  expect_false(message.hasextension(unittest::optional_nested_enum_extension ));
  expect_false(message.hasextension(unittest::optional_foreign_enum_extension));
  expect_false(message.hasextension(unittest::optional_import_enum_extension ));

  expect_false(message.hasextension(unittest::optional_string_piece_extension));
  expect_false(message.hasextension(unittest::optional_cord_extension));

  // optional fields without defaults are set to zero or something like it.
  expect_eq(0    , message.getextension(unittest::optional_int32_extension   ));
  expect_eq(0    , message.getextension(unittest::optional_int64_extension   ));
  expect_eq(0    , message.getextension(unittest::optional_uint32_extension  ));
  expect_eq(0    , message.getextension(unittest::optional_uint64_extension  ));
  expect_eq(0    , message.getextension(unittest::optional_sint32_extension  ));
  expect_eq(0    , message.getextension(unittest::optional_sint64_extension  ));
  expect_eq(0    , message.getextension(unittest::optional_fixed32_extension ));
  expect_eq(0    , message.getextension(unittest::optional_fixed64_extension ));
  expect_eq(0    , message.getextension(unittest::optional_sfixed32_extension));
  expect_eq(0    , message.getextension(unittest::optional_sfixed64_extension));
  expect_eq(0    , message.getextension(unittest::optional_float_extension   ));
  expect_eq(0    , message.getextension(unittest::optional_double_extension  ));
  expect_false(    message.getextension(unittest::optional_bool_extension    ));
  expect_eq(""   , message.getextension(unittest::optional_string_extension  ));
  expect_eq(""   , message.getextension(unittest::optional_bytes_extension   ));

  // embedded messages should also be clear.
  expect_false(message.getextension(unittest::optionalgroup_extension                 ).has_a());
  expect_false(message.getextension(unittest::optional_nested_message_extension       ).has_bb());
  expect_false(message.getextension(unittest::optional_foreign_message_extension      ).has_c());
  expect_false(message.getextension(unittest::optional_import_message_extension       ).has_d());
  expect_false(message.getextension(unittest::optional_public_import_message_extension).has_e());
  expect_false(message.getextension(unittest::optional_lazy_message_extension         ).has_bb());

  expect_eq(0, message.getextension(unittest::optionalgroup_extension                 ).a());
  expect_eq(0, message.getextension(unittest::optional_nested_message_extension       ).bb());
  expect_eq(0, message.getextension(unittest::optional_foreign_message_extension      ).c());
  expect_eq(0, message.getextension(unittest::optional_import_message_extension       ).d());
  expect_eq(0, message.getextension(unittest::optional_public_import_message_extension).e());
  expect_eq(0, message.getextension(unittest::optional_lazy_message_extension         ).bb());

  // enums without defaults are set to the first value in the enum.
  expect_eq(unittest::testalltypes::foo, message.getextension(unittest::optional_nested_enum_extension ));
  expect_eq(unittest::foreign_foo      , message.getextension(unittest::optional_foreign_enum_extension));
  expect_eq(unittest_import::import_foo, message.getextension(unittest::optional_import_enum_extension ));

  expect_eq("", message.getextension(unittest::optional_string_piece_extension));
  expect_eq("", message.getextension(unittest::optional_cord_extension));

  // repeated fields are empty.
  expect_eq(0, message.extensionsize(unittest::repeated_int32_extension   ));
  expect_eq(0, message.extensionsize(unittest::repeated_int64_extension   ));
  expect_eq(0, message.extensionsize(unittest::repeated_uint32_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_uint64_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_sint32_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_sint64_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_fixed32_extension ));
  expect_eq(0, message.extensionsize(unittest::repeated_fixed64_extension ));
  expect_eq(0, message.extensionsize(unittest::repeated_sfixed32_extension));
  expect_eq(0, message.extensionsize(unittest::repeated_sfixed64_extension));
  expect_eq(0, message.extensionsize(unittest::repeated_float_extension   ));
  expect_eq(0, message.extensionsize(unittest::repeated_double_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_bool_extension    ));
  expect_eq(0, message.extensionsize(unittest::repeated_string_extension  ));
  expect_eq(0, message.extensionsize(unittest::repeated_bytes_extension   ));

  expect_eq(0, message.extensionsize(unittest::repeatedgroup_extension           ));
  expect_eq(0, message.extensionsize(unittest::repeated_nested_message_extension ));
  expect_eq(0, message.extensionsize(unittest::repeated_foreign_message_extension));
  expect_eq(0, message.extensionsize(unittest::repeated_import_message_extension ));
  expect_eq(0, message.extensionsize(unittest::repeated_lazy_message_extension   ));
  expect_eq(0, message.extensionsize(unittest::repeated_nested_enum_extension    ));
  expect_eq(0, message.extensionsize(unittest::repeated_foreign_enum_extension   ));
  expect_eq(0, message.extensionsize(unittest::repeated_import_enum_extension    ));

  expect_eq(0, message.extensionsize(unittest::repeated_string_piece_extension));
  expect_eq(0, message.extensionsize(unittest::repeated_cord_extension));

  // has_blah() should also be false for all default fields.
  expect_false(message.hasextension(unittest::default_int32_extension   ));
  expect_false(message.hasextension(unittest::default_int64_extension   ));
  expect_false(message.hasextension(unittest::default_uint32_extension  ));
  expect_false(message.hasextension(unittest::default_uint64_extension  ));
  expect_false(message.hasextension(unittest::default_sint32_extension  ));
  expect_false(message.hasextension(unittest::default_sint64_extension  ));
  expect_false(message.hasextension(unittest::default_fixed32_extension ));
  expect_false(message.hasextension(unittest::default_fixed64_extension ));
  expect_false(message.hasextension(unittest::default_sfixed32_extension));
  expect_false(message.hasextension(unittest::default_sfixed64_extension));
  expect_false(message.hasextension(unittest::default_float_extension   ));
  expect_false(message.hasextension(unittest::default_double_extension  ));
  expect_false(message.hasextension(unittest::default_bool_extension    ));
  expect_false(message.hasextension(unittest::default_string_extension  ));
  expect_false(message.hasextension(unittest::default_bytes_extension   ));

  expect_false(message.hasextension(unittest::default_nested_enum_extension ));
  expect_false(message.hasextension(unittest::default_foreign_enum_extension));
  expect_false(message.hasextension(unittest::default_import_enum_extension ));

  expect_false(message.hasextension(unittest::default_string_piece_extension));
  expect_false(message.hasextension(unittest::default_cord_extension));

  // fields with defaults have their default values (duh).
  expect_eq( 41    , message.getextension(unittest::default_int32_extension   ));
  expect_eq( 42    , message.getextension(unittest::default_int64_extension   ));
  expect_eq( 43    , message.getextension(unittest::default_uint32_extension  ));
  expect_eq( 44    , message.getextension(unittest::default_uint64_extension  ));
  expect_eq(-45    , message.getextension(unittest::default_sint32_extension  ));
  expect_eq( 46    , message.getextension(unittest::default_sint64_extension  ));
  expect_eq( 47    , message.getextension(unittest::default_fixed32_extension ));
  expect_eq( 48    , message.getextension(unittest::default_fixed64_extension ));
  expect_eq( 49    , message.getextension(unittest::default_sfixed32_extension));
  expect_eq(-50    , message.getextension(unittest::default_sfixed64_extension));
  expect_eq( 51.5  , message.getextension(unittest::default_float_extension   ));
  expect_eq( 52e3  , message.getextension(unittest::default_double_extension  ));
  expect_true(       message.getextension(unittest::default_bool_extension    ));
  expect_eq("hello", message.getextension(unittest::default_string_extension  ));
  expect_eq("world", message.getextension(unittest::default_bytes_extension   ));

  expect_eq(unittest::testalltypes::bar, message.getextension(unittest::default_nested_enum_extension ));
  expect_eq(unittest::foreign_bar      , message.getextension(unittest::default_foreign_enum_extension));
  expect_eq(unittest_import::import_bar, message.getextension(unittest::default_import_enum_extension ));

  expect_eq("abc", message.getextension(unittest::default_string_piece_extension));
  expect_eq("123", message.getextension(unittest::default_cord_extension));
}

// -------------------------------------------------------------------

void testutil::expectrepeatedextensionsmodified(
    const unittest::testallextensions& message) {
  // modifyrepeatedfields only sets the second repeated element of each
  // field.  in addition to verifying this, we also verify that the first
  // element and size were *not* modified.
  assert_eq(2, message.extensionsize(unittest::repeated_int32_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_int64_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed32_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed64_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed32_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed64_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_float_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_double_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bool_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_string_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bytes_extension   ));

  assert_eq(2, message.extensionsize(unittest::repeatedgroup_extension           ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_message_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_import_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_lazy_message_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_enum_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_enum_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_import_enum_extension    ));

  assert_eq(2, message.extensionsize(unittest::repeated_string_piece_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_cord_extension));

  expect_eq(201  , message.getextension(unittest::repeated_int32_extension   , 0));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension   , 0));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension  , 0));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension  , 0));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension  , 0));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension  , 0));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension , 0));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension , 0));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension, 0));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension, 0));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension   , 0));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension  , 0));
  expect_true(     message.getextension(unittest::repeated_bool_extension    , 0));
  expect_eq("215", message.getextension(unittest::repeated_string_extension  , 0));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension   , 0));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension   , 0).bb());

  expect_eq(unittest::testalltypes::bar, message.getextension(unittest::repeated_nested_enum_extension , 0));
  expect_eq(unittest::foreign_bar      , message.getextension(unittest::repeated_foreign_enum_extension, 0));
  expect_eq(unittest_import::import_bar, message.getextension(unittest::repeated_import_enum_extension , 0));

  expect_eq("224", message.getextension(unittest::repeated_string_piece_extension, 0));
  expect_eq("225", message.getextension(unittest::repeated_cord_extension, 0));

  // actually verify the second (modified) elements now.
  expect_eq(501  , message.getextension(unittest::repeated_int32_extension   , 1));
  expect_eq(502  , message.getextension(unittest::repeated_int64_extension   , 1));
  expect_eq(503  , message.getextension(unittest::repeated_uint32_extension  , 1));
  expect_eq(504  , message.getextension(unittest::repeated_uint64_extension  , 1));
  expect_eq(505  , message.getextension(unittest::repeated_sint32_extension  , 1));
  expect_eq(506  , message.getextension(unittest::repeated_sint64_extension  , 1));
  expect_eq(507  , message.getextension(unittest::repeated_fixed32_extension , 1));
  expect_eq(508  , message.getextension(unittest::repeated_fixed64_extension , 1));
  expect_eq(509  , message.getextension(unittest::repeated_sfixed32_extension, 1));
  expect_eq(510  , message.getextension(unittest::repeated_sfixed64_extension, 1));
  expect_eq(511  , message.getextension(unittest::repeated_float_extension   , 1));
  expect_eq(512  , message.getextension(unittest::repeated_double_extension  , 1));
  expect_true(     message.getextension(unittest::repeated_bool_extension    , 1));
  expect_eq("515", message.getextension(unittest::repeated_string_extension  , 1));
  expect_eq("516", message.getextension(unittest::repeated_bytes_extension   , 1));

  expect_eq(517, message.getextension(unittest::repeatedgroup_extension           , 1).a());
  expect_eq(518, message.getextension(unittest::repeated_nested_message_extension , 1).bb());
  expect_eq(519, message.getextension(unittest::repeated_foreign_message_extension, 1).c());
  expect_eq(520, message.getextension(unittest::repeated_import_message_extension , 1).d());
  expect_eq(527, message.getextension(unittest::repeated_lazy_message_extension   , 1).bb());

  expect_eq(unittest::testalltypes::foo, message.getextension(unittest::repeated_nested_enum_extension , 1));
  expect_eq(unittest::foreign_foo      , message.getextension(unittest::repeated_foreign_enum_extension, 1));
  expect_eq(unittest_import::import_foo, message.getextension(unittest::repeated_import_enum_extension , 1));

  expect_eq("524", message.getextension(unittest::repeated_string_piece_extension, 1));
  expect_eq("525", message.getextension(unittest::repeated_cord_extension, 1));
}

// -------------------------------------------------------------------

void testutil::setpackedextensions(unittest::testpackedextensions* message) {
  message->addextension(unittest::packed_int32_extension   , 601);
  message->addextension(unittest::packed_int64_extension   , 602);
  message->addextension(unittest::packed_uint32_extension  , 603);
  message->addextension(unittest::packed_uint64_extension  , 604);
  message->addextension(unittest::packed_sint32_extension  , 605);
  message->addextension(unittest::packed_sint64_extension  , 606);
  message->addextension(unittest::packed_fixed32_extension , 607);
  message->addextension(unittest::packed_fixed64_extension , 608);
  message->addextension(unittest::packed_sfixed32_extension, 609);
  message->addextension(unittest::packed_sfixed64_extension, 610);
  message->addextension(unittest::packed_float_extension   , 611);
  message->addextension(unittest::packed_double_extension  , 612);
  message->addextension(unittest::packed_bool_extension    , true);
  message->addextension(unittest::packed_enum_extension, unittest::foreign_bar);
  // add a second one of each field
  message->addextension(unittest::packed_int32_extension   , 701);
  message->addextension(unittest::packed_int64_extension   , 702);
  message->addextension(unittest::packed_uint32_extension  , 703);
  message->addextension(unittest::packed_uint64_extension  , 704);
  message->addextension(unittest::packed_sint32_extension  , 705);
  message->addextension(unittest::packed_sint64_extension  , 706);
  message->addextension(unittest::packed_fixed32_extension , 707);
  message->addextension(unittest::packed_fixed64_extension , 708);
  message->addextension(unittest::packed_sfixed32_extension, 709);
  message->addextension(unittest::packed_sfixed64_extension, 710);
  message->addextension(unittest::packed_float_extension   , 711);
  message->addextension(unittest::packed_double_extension  , 712);
  message->addextension(unittest::packed_bool_extension    , false);
  message->addextension(unittest::packed_enum_extension, unittest::foreign_baz);
}

// -------------------------------------------------------------------

void testutil::modifypackedextensions(unittest::testpackedextensions* message) {
  message->setextension(unittest::packed_int32_extension   , 1, 801);
  message->setextension(unittest::packed_int64_extension   , 1, 802);
  message->setextension(unittest::packed_uint32_extension  , 1, 803);
  message->setextension(unittest::packed_uint64_extension  , 1, 804);
  message->setextension(unittest::packed_sint32_extension  , 1, 805);
  message->setextension(unittest::packed_sint64_extension  , 1, 806);
  message->setextension(unittest::packed_fixed32_extension , 1, 807);
  message->setextension(unittest::packed_fixed64_extension , 1, 808);
  message->setextension(unittest::packed_sfixed32_extension, 1, 809);
  message->setextension(unittest::packed_sfixed64_extension, 1, 810);
  message->setextension(unittest::packed_float_extension   , 1, 811);
  message->setextension(unittest::packed_double_extension  , 1, 812);
  message->setextension(unittest::packed_bool_extension    , 1, true);
  message->setextension(unittest::packed_enum_extension    , 1,
                        unittest::foreign_foo);
}

// -------------------------------------------------------------------

void testutil::expectpackedextensionsset(
    const unittest::testpackedextensions& message) {
  assert_eq(2, message.extensionsize(unittest::packed_int32_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_int64_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_uint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_uint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed32_extension ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed64_extension ));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed32_extension));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed64_extension));
  assert_eq(2, message.extensionsize(unittest::packed_float_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_double_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_bool_extension    ));
  assert_eq(2, message.extensionsize(unittest::packed_enum_extension    ));

  expect_eq(601  , message.getextension(unittest::packed_int32_extension   , 0));
  expect_eq(602  , message.getextension(unittest::packed_int64_extension   , 0));
  expect_eq(603  , message.getextension(unittest::packed_uint32_extension  , 0));
  expect_eq(604  , message.getextension(unittest::packed_uint64_extension  , 0));
  expect_eq(605  , message.getextension(unittest::packed_sint32_extension  , 0));
  expect_eq(606  , message.getextension(unittest::packed_sint64_extension  , 0));
  expect_eq(607  , message.getextension(unittest::packed_fixed32_extension , 0));
  expect_eq(608  , message.getextension(unittest::packed_fixed64_extension , 0));
  expect_eq(609  , message.getextension(unittest::packed_sfixed32_extension, 0));
  expect_eq(610  , message.getextension(unittest::packed_sfixed64_extension, 0));
  expect_eq(611  , message.getextension(unittest::packed_float_extension   , 0));
  expect_eq(612  , message.getextension(unittest::packed_double_extension  , 0));
  expect_true(     message.getextension(unittest::packed_bool_extension    , 0));
  expect_eq(unittest::foreign_bar,
            message.getextension(unittest::packed_enum_extension, 0));
  expect_eq(701  , message.getextension(unittest::packed_int32_extension   , 1));
  expect_eq(702  , message.getextension(unittest::packed_int64_extension   , 1));
  expect_eq(703  , message.getextension(unittest::packed_uint32_extension  , 1));
  expect_eq(704  , message.getextension(unittest::packed_uint64_extension  , 1));
  expect_eq(705  , message.getextension(unittest::packed_sint32_extension  , 1));
  expect_eq(706  , message.getextension(unittest::packed_sint64_extension  , 1));
  expect_eq(707  , message.getextension(unittest::packed_fixed32_extension , 1));
  expect_eq(708  , message.getextension(unittest::packed_fixed64_extension , 1));
  expect_eq(709  , message.getextension(unittest::packed_sfixed32_extension, 1));
  expect_eq(710  , message.getextension(unittest::packed_sfixed64_extension, 1));
  expect_eq(711  , message.getextension(unittest::packed_float_extension   , 1));
  expect_eq(712  , message.getextension(unittest::packed_double_extension  , 1));
  expect_false(    message.getextension(unittest::packed_bool_extension    , 1));
  expect_eq(unittest::foreign_baz,
            message.getextension(unittest::packed_enum_extension, 1));
}

// -------------------------------------------------------------------

void testutil::expectpackedextensionsclear(
    const unittest::testpackedextensions& message) {
  expect_eq(0, message.extensionsize(unittest::packed_int32_extension   ));
  expect_eq(0, message.extensionsize(unittest::packed_int64_extension   ));
  expect_eq(0, message.extensionsize(unittest::packed_uint32_extension  ));
  expect_eq(0, message.extensionsize(unittest::packed_uint64_extension  ));
  expect_eq(0, message.extensionsize(unittest::packed_sint32_extension  ));
  expect_eq(0, message.extensionsize(unittest::packed_sint64_extension  ));
  expect_eq(0, message.extensionsize(unittest::packed_fixed32_extension ));
  expect_eq(0, message.extensionsize(unittest::packed_fixed64_extension ));
  expect_eq(0, message.extensionsize(unittest::packed_sfixed32_extension));
  expect_eq(0, message.extensionsize(unittest::packed_sfixed64_extension));
  expect_eq(0, message.extensionsize(unittest::packed_float_extension   ));
  expect_eq(0, message.extensionsize(unittest::packed_double_extension  ));
  expect_eq(0, message.extensionsize(unittest::packed_bool_extension    ));
  expect_eq(0, message.extensionsize(unittest::packed_enum_extension    ));
}

// -------------------------------------------------------------------

void testutil::expectpackedextensionsmodified(
    const unittest::testpackedextensions& message) {
  assert_eq(2, message.extensionsize(unittest::packed_int32_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_int64_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_uint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_uint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed32_extension ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed64_extension ));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed32_extension));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed64_extension));
  assert_eq(2, message.extensionsize(unittest::packed_float_extension   ));
  assert_eq(2, message.extensionsize(unittest::packed_double_extension  ));
  assert_eq(2, message.extensionsize(unittest::packed_bool_extension    ));
  assert_eq(2, message.extensionsize(unittest::packed_enum_extension    ));
  expect_eq(601  , message.getextension(unittest::packed_int32_extension   , 0));
  expect_eq(602  , message.getextension(unittest::packed_int64_extension   , 0));
  expect_eq(603  , message.getextension(unittest::packed_uint32_extension  , 0));
  expect_eq(604  , message.getextension(unittest::packed_uint64_extension  , 0));
  expect_eq(605  , message.getextension(unittest::packed_sint32_extension  , 0));
  expect_eq(606  , message.getextension(unittest::packed_sint64_extension  , 0));
  expect_eq(607  , message.getextension(unittest::packed_fixed32_extension , 0));
  expect_eq(608  , message.getextension(unittest::packed_fixed64_extension , 0));
  expect_eq(609  , message.getextension(unittest::packed_sfixed32_extension, 0));
  expect_eq(610  , message.getextension(unittest::packed_sfixed64_extension, 0));
  expect_eq(611  , message.getextension(unittest::packed_float_extension   , 0));
  expect_eq(612  , message.getextension(unittest::packed_double_extension  , 0));
  expect_true(     message.getextension(unittest::packed_bool_extension    , 0));
  expect_eq(unittest::foreign_bar,
            message.getextension(unittest::packed_enum_extension, 0));

  // actually verify the second (modified) elements now.
  expect_eq(801  , message.getextension(unittest::packed_int32_extension   , 1));
  expect_eq(802  , message.getextension(unittest::packed_int64_extension   , 1));
  expect_eq(803  , message.getextension(unittest::packed_uint32_extension  , 1));
  expect_eq(804  , message.getextension(unittest::packed_uint64_extension  , 1));
  expect_eq(805  , message.getextension(unittest::packed_sint32_extension  , 1));
  expect_eq(806  , message.getextension(unittest::packed_sint64_extension  , 1));
  expect_eq(807  , message.getextension(unittest::packed_fixed32_extension , 1));
  expect_eq(808  , message.getextension(unittest::packed_fixed64_extension , 1));
  expect_eq(809  , message.getextension(unittest::packed_sfixed32_extension, 1));
  expect_eq(810  , message.getextension(unittest::packed_sfixed64_extension, 1));
  expect_eq(811  , message.getextension(unittest::packed_float_extension   , 1));
  expect_eq(812  , message.getextension(unittest::packed_double_extension  , 1));
  expect_true(     message.getextension(unittest::packed_bool_extension    , 1));
  expect_eq(unittest::foreign_foo,
            message.getextension(unittest::packed_enum_extension, 1));
}

// -------------------------------------------------------------------

void testutil::expectallfieldsandextensionsinorder(const string& serialized) {
  // we set each field individually, serialize separately, and concatenate all
  // the strings in canonical order to determine the expected serialization.
  string expected;
  unittest::testfieldorderings message;
  message.set_my_int(1);  // field 1.
  message.appendtostring(&expected);
  message.clear();
  message.setextension(unittest::my_extension_int, 23);  // field 5.
  message.appendtostring(&expected);
  message.clear();
  message.set_my_string("foo");  // field 11.
  message.appendtostring(&expected);
  message.clear();
  message.setextension(unittest::my_extension_string, "bar");  // field 50.
  message.appendtostring(&expected);
  message.clear();
  message.set_my_float(1.0);  // field 101.
  message.appendtostring(&expected);
  message.clear();

  // we don't expect_eq() since we don't want to print raw bytes to stdout.
  expect_true(serialized == expected);
}

void testutil::expectlastrepeatedsremoved(
    const unittest::testalltypes& message) {
  assert_eq(1, message.repeated_int32_size   ());
  assert_eq(1, message.repeated_int64_size   ());
  assert_eq(1, message.repeated_uint32_size  ());
  assert_eq(1, message.repeated_uint64_size  ());
  assert_eq(1, message.repeated_sint32_size  ());
  assert_eq(1, message.repeated_sint64_size  ());
  assert_eq(1, message.repeated_fixed32_size ());
  assert_eq(1, message.repeated_fixed64_size ());
  assert_eq(1, message.repeated_sfixed32_size());
  assert_eq(1, message.repeated_sfixed64_size());
  assert_eq(1, message.repeated_float_size   ());
  assert_eq(1, message.repeated_double_size  ());
  assert_eq(1, message.repeated_bool_size    ());
  assert_eq(1, message.repeated_string_size  ());
  assert_eq(1, message.repeated_bytes_size   ());

  assert_eq(1, message.repeatedgroup_size           ());
  assert_eq(1, message.repeated_nested_message_size ());
  assert_eq(1, message.repeated_foreign_message_size());
  assert_eq(1, message.repeated_import_message_size ());
  assert_eq(1, message.repeated_import_message_size ());
  assert_eq(1, message.repeated_nested_enum_size    ());
  assert_eq(1, message.repeated_foreign_enum_size   ());
  assert_eq(1, message.repeated_import_enum_size    ());

#ifndef protobuf_test_no_descriptors
  assert_eq(1, message.repeated_string_piece_size());
  assert_eq(1, message.repeated_cord_size());
#endif

  // test that the remaining element is the correct one.
  expect_eq(201  , message.repeated_int32   (0));
  expect_eq(202  , message.repeated_int64   (0));
  expect_eq(203  , message.repeated_uint32  (0));
  expect_eq(204  , message.repeated_uint64  (0));
  expect_eq(205  , message.repeated_sint32  (0));
  expect_eq(206  , message.repeated_sint64  (0));
  expect_eq(207  , message.repeated_fixed32 (0));
  expect_eq(208  , message.repeated_fixed64 (0));
  expect_eq(209  , message.repeated_sfixed32(0));
  expect_eq(210  , message.repeated_sfixed64(0));
  expect_eq(211  , message.repeated_float   (0));
  expect_eq(212  , message.repeated_double  (0));
  expect_true(     message.repeated_bool    (0));
  expect_eq("215", message.repeated_string  (0));
  expect_eq("216", message.repeated_bytes   (0));

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(220, message.repeated_import_message (0).d());

  expect_eq(unittest::testalltypes::bar, message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_bar      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_bar, message.repeated_import_enum (0));
}

void testutil::expectlastrepeatedextensionsremoved(
    const unittest::testallextensions& message) {

  // test that one element was removed.
  assert_eq(1, message.extensionsize(unittest::repeated_int32_extension   ));
  assert_eq(1, message.extensionsize(unittest::repeated_int64_extension   ));
  assert_eq(1, message.extensionsize(unittest::repeated_uint32_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_uint64_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_sint32_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_sint64_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_fixed32_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_fixed64_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_sfixed32_extension));
  assert_eq(1, message.extensionsize(unittest::repeated_sfixed64_extension));
  assert_eq(1, message.extensionsize(unittest::repeated_float_extension   ));
  assert_eq(1, message.extensionsize(unittest::repeated_double_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_bool_extension    ));
  assert_eq(1, message.extensionsize(unittest::repeated_string_extension  ));
  assert_eq(1, message.extensionsize(unittest::repeated_bytes_extension   ));

  assert_eq(1, message.extensionsize(unittest::repeatedgroup_extension           ));
  assert_eq(1, message.extensionsize(unittest::repeated_nested_message_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_foreign_message_extension));
  assert_eq(1, message.extensionsize(unittest::repeated_import_message_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_lazy_message_extension   ));
  assert_eq(1, message.extensionsize(unittest::repeated_nested_enum_extension    ));
  assert_eq(1, message.extensionsize(unittest::repeated_foreign_enum_extension   ));
  assert_eq(1, message.extensionsize(unittest::repeated_import_enum_extension    ));

  assert_eq(1, message.extensionsize(unittest::repeated_string_piece_extension));
  assert_eq(1, message.extensionsize(unittest::repeated_cord_extension));

  // test that the remaining element is the correct one.
  expect_eq(201  , message.getextension(unittest::repeated_int32_extension   , 0));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension   , 0));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension  , 0));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension  , 0));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension  , 0));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension  , 0));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension , 0));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension , 0));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension, 0));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension, 0));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension   , 0));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension  , 0));
  expect_true(     message.getextension(unittest::repeated_bool_extension    , 0));
  expect_eq("215", message.getextension(unittest::repeated_string_extension  , 0));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension   , 0));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension   , 0).bb());

  expect_eq(unittest::testalltypes::bar, message.getextension(unittest::repeated_nested_enum_extension , 0));
  expect_eq(unittest::foreign_bar      , message.getextension(unittest::repeated_foreign_enum_extension, 0));
  expect_eq(unittest_import::import_bar, message.getextension(unittest::repeated_import_enum_extension , 0));

  expect_eq("224", message.getextension(unittest::repeated_string_piece_extension, 0));
  expect_eq("225", message.getextension(unittest::repeated_cord_extension, 0));
}

void testutil::expectlastrepeatedsreleased(
    const unittest::testalltypes& message) {
  assert_eq(1, message.repeatedgroup_size           ());
  assert_eq(1, message.repeated_nested_message_size ());
  assert_eq(1, message.repeated_foreign_message_size());
  assert_eq(1, message.repeated_import_message_size ());
  assert_eq(1, message.repeated_import_message_size ());

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(220, message.repeated_import_message (0).d());
}

void testutil::expectlastrepeatedextensionsreleased(
    const unittest::testallextensions& message) {
  assert_eq(1, message.extensionsize(unittest::repeatedgroup_extension           ));
  assert_eq(1, message.extensionsize(unittest::repeated_nested_message_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_foreign_message_extension));
  assert_eq(1, message.extensionsize(unittest::repeated_import_message_extension ));
  assert_eq(1, message.extensionsize(unittest::repeated_lazy_message_extension   ));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension   , 0).bb());
}

void testutil::expectrepeatedsswapped(
    const unittest::testalltypes& message) {
  assert_eq(2, message.repeated_int32_size   ());
  assert_eq(2, message.repeated_int64_size   ());
  assert_eq(2, message.repeated_uint32_size  ());
  assert_eq(2, message.repeated_uint64_size  ());
  assert_eq(2, message.repeated_sint32_size  ());
  assert_eq(2, message.repeated_sint64_size  ());
  assert_eq(2, message.repeated_fixed32_size ());
  assert_eq(2, message.repeated_fixed64_size ());
  assert_eq(2, message.repeated_sfixed32_size());
  assert_eq(2, message.repeated_sfixed64_size());
  assert_eq(2, message.repeated_float_size   ());
  assert_eq(2, message.repeated_double_size  ());
  assert_eq(2, message.repeated_bool_size    ());
  assert_eq(2, message.repeated_string_size  ());
  assert_eq(2, message.repeated_bytes_size   ());

  assert_eq(2, message.repeatedgroup_size           ());
  assert_eq(2, message.repeated_nested_message_size ());
  assert_eq(2, message.repeated_foreign_message_size());
  assert_eq(2, message.repeated_import_message_size ());
  assert_eq(2, message.repeated_import_message_size ());
  assert_eq(2, message.repeated_nested_enum_size    ());
  assert_eq(2, message.repeated_foreign_enum_size   ());
  assert_eq(2, message.repeated_import_enum_size    ());

#ifndef protobuf_test_no_descriptors
  assert_eq(2, message.repeated_string_piece_size());
  assert_eq(2, message.repeated_cord_size());
#endif

  // test that the first element and second element are flipped.
  expect_eq(201  , message.repeated_int32   (1));
  expect_eq(202  , message.repeated_int64   (1));
  expect_eq(203  , message.repeated_uint32  (1));
  expect_eq(204  , message.repeated_uint64  (1));
  expect_eq(205  , message.repeated_sint32  (1));
  expect_eq(206  , message.repeated_sint64  (1));
  expect_eq(207  , message.repeated_fixed32 (1));
  expect_eq(208  , message.repeated_fixed64 (1));
  expect_eq(209  , message.repeated_sfixed32(1));
  expect_eq(210  , message.repeated_sfixed64(1));
  expect_eq(211  , message.repeated_float   (1));
  expect_eq(212  , message.repeated_double  (1));
  expect_true(     message.repeated_bool    (1));
  expect_eq("215", message.repeated_string  (1));
  expect_eq("216", message.repeated_bytes   (1));

  expect_eq(217, message.repeatedgroup           (1).a());
  expect_eq(218, message.repeated_nested_message (1).bb());
  expect_eq(219, message.repeated_foreign_message(1).c());
  expect_eq(220, message.repeated_import_message (1).d());
  expect_eq(220, message.repeated_import_message (1).d());

  expect_eq(unittest::testalltypes::bar, message.repeated_nested_enum (1));
  expect_eq(unittest::foreign_bar      , message.repeated_foreign_enum(1));
  expect_eq(unittest_import::import_bar, message.repeated_import_enum (1));

  expect_eq(301  , message.repeated_int32   (0));
  expect_eq(302  , message.repeated_int64   (0));
  expect_eq(303  , message.repeated_uint32  (0));
  expect_eq(304  , message.repeated_uint64  (0));
  expect_eq(305  , message.repeated_sint32  (0));
  expect_eq(306  , message.repeated_sint64  (0));
  expect_eq(307  , message.repeated_fixed32 (0));
  expect_eq(308  , message.repeated_fixed64 (0));
  expect_eq(309  , message.repeated_sfixed32(0));
  expect_eq(310  , message.repeated_sfixed64(0));
  expect_eq(311  , message.repeated_float   (0));
  expect_eq(312  , message.repeated_double  (0));
  expect_false(    message.repeated_bool    (0));
  expect_eq("315", message.repeated_string  (0));
  expect_eq("316", message.repeated_bytes   (0));

  expect_eq(317, message.repeatedgroup           (0).a());
  expect_eq(318, message.repeated_nested_message (0).bb());
  expect_eq(319, message.repeated_foreign_message(0).c());
  expect_eq(320, message.repeated_import_message (0).d());
  expect_eq(320, message.repeated_import_message (0).d());

  expect_eq(unittest::testalltypes::baz, message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_baz      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_baz, message.repeated_import_enum (0));
}

void testutil::expectrepeatedextensionsswapped(
    const unittest::testallextensions& message) {

  assert_eq(2, message.extensionsize(unittest::repeated_int32_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_int64_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint32_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint64_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed32_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed64_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed32_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed64_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_float_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_double_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bool_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_string_extension  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bytes_extension   ));

  assert_eq(2, message.extensionsize(unittest::repeatedgroup_extension           ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_message_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_import_message_extension ));
  assert_eq(2, message.extensionsize(unittest::repeated_lazy_message_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_enum_extension    ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_enum_extension   ));
  assert_eq(2, message.extensionsize(unittest::repeated_import_enum_extension    ));

  assert_eq(2, message.extensionsize(unittest::repeated_string_piece_extension));
  assert_eq(2, message.extensionsize(unittest::repeated_cord_extension));

  expect_eq(201  , message.getextension(unittest::repeated_int32_extension   , 1));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension   , 1));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension  , 1));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension  , 1));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension  , 1));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension  , 1));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension , 1));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension , 1));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension, 1));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension, 1));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension   , 1));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension  , 1));
  expect_true(     message.getextension(unittest::repeated_bool_extension    , 1));
  expect_eq("215", message.getextension(unittest::repeated_string_extension  , 1));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension   , 1));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension           , 1).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension , 1).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension, 1).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension , 1).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension   , 1).bb());

  expect_eq(unittest::testalltypes::bar, message.getextension(unittest::repeated_nested_enum_extension , 1));
  expect_eq(unittest::foreign_bar      , message.getextension(unittest::repeated_foreign_enum_extension, 1));
  expect_eq(unittest_import::import_bar, message.getextension(unittest::repeated_import_enum_extension , 1));

  expect_eq("224", message.getextension(unittest::repeated_string_piece_extension, 1));
  expect_eq("225", message.getextension(unittest::repeated_cord_extension, 1));

  expect_eq(301  , message.getextension(unittest::repeated_int32_extension   , 0));
  expect_eq(302  , message.getextension(unittest::repeated_int64_extension   , 0));
  expect_eq(303  , message.getextension(unittest::repeated_uint32_extension  , 0));
  expect_eq(304  , message.getextension(unittest::repeated_uint64_extension  , 0));
  expect_eq(305  , message.getextension(unittest::repeated_sint32_extension  , 0));
  expect_eq(306  , message.getextension(unittest::repeated_sint64_extension  , 0));
  expect_eq(307  , message.getextension(unittest::repeated_fixed32_extension , 0));
  expect_eq(308  , message.getextension(unittest::repeated_fixed64_extension , 0));
  expect_eq(309  , message.getextension(unittest::repeated_sfixed32_extension, 0));
  expect_eq(310  , message.getextension(unittest::repeated_sfixed64_extension, 0));
  expect_eq(311  , message.getextension(unittest::repeated_float_extension   , 0));
  expect_eq(312  , message.getextension(unittest::repeated_double_extension  , 0));
  expect_false(    message.getextension(unittest::repeated_bool_extension    , 0));
  expect_eq("315", message.getextension(unittest::repeated_string_extension  , 0));
  expect_eq("316", message.getextension(unittest::repeated_bytes_extension   , 0));

  expect_eq(317, message.getextension(unittest::repeatedgroup_extension           , 0).a());
  expect_eq(318, message.getextension(unittest::repeated_nested_message_extension , 0).bb());
  expect_eq(319, message.getextension(unittest::repeated_foreign_message_extension, 0).c());
  expect_eq(320, message.getextension(unittest::repeated_import_message_extension , 0).d());
  expect_eq(327, message.getextension(unittest::repeated_lazy_message_extension   , 0).bb());

  expect_eq(unittest::testalltypes::baz, message.getextension(unittest::repeated_nested_enum_extension , 0));
  expect_eq(unittest::foreign_baz      , message.getextension(unittest::repeated_foreign_enum_extension, 0));
  expect_eq(unittest_import::import_baz, message.getextension(unittest::repeated_import_enum_extension , 0));

  expect_eq("324", message.getextension(unittest::repeated_string_piece_extension, 0));
  expect_eq("325", message.getextension(unittest::repeated_cord_extension, 0));
}

// ===================================================================

testutil::reflectiontester::reflectiontester(
    const descriptor* base_descriptor)
  : base_descriptor_(base_descriptor) {

  const descriptorpool* pool = base_descriptor->file()->pool();

  nested_b_ =
    pool->findfieldbyname("protobuf_unittest.testalltypes.nestedmessage.bb");
  foreign_c_ =
    pool->findfieldbyname("protobuf_unittest.foreignmessage.c");
  import_d_ =
    pool->findfieldbyname("protobuf_unittest_import.importmessage.d");
  import_e_ =
    pool->findfieldbyname("protobuf_unittest_import.publicimportmessage.e");
  nested_foo_ =
    pool->findenumvaluebyname("protobuf_unittest.testalltypes.foo");
  nested_bar_ =
    pool->findenumvaluebyname("protobuf_unittest.testalltypes.bar");
  nested_baz_ =
    pool->findenumvaluebyname("protobuf_unittest.testalltypes.baz");
  foreign_foo_ =
    pool->findenumvaluebyname("protobuf_unittest.foreign_foo");
  foreign_bar_ =
    pool->findenumvaluebyname("protobuf_unittest.foreign_bar");
  foreign_baz_ =
    pool->findenumvaluebyname("protobuf_unittest.foreign_baz");
  import_foo_ =
    pool->findenumvaluebyname("protobuf_unittest_import.import_foo");
  import_bar_ =
    pool->findenumvaluebyname("protobuf_unittest_import.import_bar");
  import_baz_ =
    pool->findenumvaluebyname("protobuf_unittest_import.import_baz");

  if (base_descriptor_->name() == "testallextensions") {
    group_a_ =
      pool->findfieldbyname("protobuf_unittest.optionalgroup_extension.a");
    repeated_group_a_ =
      pool->findfieldbyname("protobuf_unittest.repeatedgroup_extension.a");
  } else {
    group_a_ =
      pool->findfieldbyname("protobuf_unittest.testalltypes.optionalgroup.a");
    repeated_group_a_ =
      pool->findfieldbyname("protobuf_unittest.testalltypes.repeatedgroup.a");
  }

  expect_true(group_a_          != null);
  expect_true(repeated_group_a_ != null);
  expect_true(nested_b_         != null);
  expect_true(foreign_c_        != null);
  expect_true(import_d_         != null);
  expect_true(import_e_         != null);
  expect_true(nested_foo_       != null);
  expect_true(nested_bar_       != null);
  expect_true(nested_baz_       != null);
  expect_true(foreign_foo_      != null);
  expect_true(foreign_bar_      != null);
  expect_true(foreign_baz_      != null);
  expect_true(import_foo_       != null);
  expect_true(import_bar_       != null);
  expect_true(import_baz_       != null);
}

// shorthand to get a fielddescriptor for a field of unittest::testalltypes.
const fielddescriptor* testutil::reflectiontester::f(const string& name) {
  const fielddescriptor* result = null;
  if (base_descriptor_->name() == "testallextensions" ||
      base_descriptor_->name() == "testpackedextensions") {
    result = base_descriptor_->file()->findextensionbyname(name + "_extension");
  } else {
    result = base_descriptor_->findfieldbyname(name);
  }
  google_check(result != null);
  return result;
}

// -------------------------------------------------------------------

void testutil::reflectiontester::setallfieldsviareflection(message* message) {
  const reflection* reflection = message->getreflection();
  message* sub_message;

  reflection->setint32 (message, f("optional_int32"   ), 101);
  reflection->setint64 (message, f("optional_int64"   ), 102);
  reflection->setuint32(message, f("optional_uint32"  ), 103);
  reflection->setuint64(message, f("optional_uint64"  ), 104);
  reflection->setint32 (message, f("optional_sint32"  ), 105);
  reflection->setint64 (message, f("optional_sint64"  ), 106);
  reflection->setuint32(message, f("optional_fixed32" ), 107);
  reflection->setuint64(message, f("optional_fixed64" ), 108);
  reflection->setint32 (message, f("optional_sfixed32"), 109);
  reflection->setint64 (message, f("optional_sfixed64"), 110);
  reflection->setfloat (message, f("optional_float"   ), 111);
  reflection->setdouble(message, f("optional_double"  ), 112);
  reflection->setbool  (message, f("optional_bool"    ), true);
  reflection->setstring(message, f("optional_string"  ), "115");
  reflection->setstring(message, f("optional_bytes"   ), "116");

  sub_message = reflection->mutablemessage(message, f("optionalgroup"));
  sub_message->getreflection()->setint32(sub_message, group_a_, 117);
  sub_message = reflection->mutablemessage(message, f("optional_nested_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 118);
  sub_message = reflection->mutablemessage(message, f("optional_foreign_message"));
  sub_message->getreflection()->setint32(sub_message, foreign_c_, 119);
  sub_message = reflection->mutablemessage(message, f("optional_import_message"));
  sub_message->getreflection()->setint32(sub_message, import_d_, 120);

  reflection->setenum(message, f("optional_nested_enum" ),  nested_baz_);
  reflection->setenum(message, f("optional_foreign_enum"), foreign_baz_);
  reflection->setenum(message, f("optional_import_enum" ),  import_baz_);

  reflection->setstring(message, f("optional_string_piece"), "124");
  reflection->setstring(message, f("optional_cord"), "125");

  sub_message = reflection->mutablemessage(message, f("optional_public_import_message"));
  sub_message->getreflection()->setint32(sub_message, import_e_, 126);

  sub_message = reflection->mutablemessage(message, f("optional_lazy_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 127);

  // -----------------------------------------------------------------

  reflection->addint32 (message, f("repeated_int32"   ), 201);
  reflection->addint64 (message, f("repeated_int64"   ), 202);
  reflection->adduint32(message, f("repeated_uint32"  ), 203);
  reflection->adduint64(message, f("repeated_uint64"  ), 204);
  reflection->addint32 (message, f("repeated_sint32"  ), 205);
  reflection->addint64 (message, f("repeated_sint64"  ), 206);
  reflection->adduint32(message, f("repeated_fixed32" ), 207);
  reflection->adduint64(message, f("repeated_fixed64" ), 208);
  reflection->addint32 (message, f("repeated_sfixed32"), 209);
  reflection->addint64 (message, f("repeated_sfixed64"), 210);
  reflection->addfloat (message, f("repeated_float"   ), 211);
  reflection->adddouble(message, f("repeated_double"  ), 212);
  reflection->addbool  (message, f("repeated_bool"    ), true);
  reflection->addstring(message, f("repeated_string"  ), "215");
  reflection->addstring(message, f("repeated_bytes"   ), "216");

  sub_message = reflection->addmessage(message, f("repeatedgroup"));
  sub_message->getreflection()->setint32(sub_message, repeated_group_a_, 217);
  sub_message = reflection->addmessage(message, f("repeated_nested_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 218);
  sub_message = reflection->addmessage(message, f("repeated_foreign_message"));
  sub_message->getreflection()->setint32(sub_message, foreign_c_, 219);
  sub_message = reflection->addmessage(message, f("repeated_import_message"));
  sub_message->getreflection()->setint32(sub_message, import_d_, 220);
  sub_message = reflection->addmessage(message, f("repeated_lazy_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 227);

  reflection->addenum(message, f("repeated_nested_enum" ),  nested_bar_);
  reflection->addenum(message, f("repeated_foreign_enum"), foreign_bar_);
  reflection->addenum(message, f("repeated_import_enum" ),  import_bar_);

  reflection->addstring(message, f("repeated_string_piece"), "224");
  reflection->addstring(message, f("repeated_cord"), "225");

  // add a second one of each field.
  reflection->addint32 (message, f("repeated_int32"   ), 301);
  reflection->addint64 (message, f("repeated_int64"   ), 302);
  reflection->adduint32(message, f("repeated_uint32"  ), 303);
  reflection->adduint64(message, f("repeated_uint64"  ), 304);
  reflection->addint32 (message, f("repeated_sint32"  ), 305);
  reflection->addint64 (message, f("repeated_sint64"  ), 306);
  reflection->adduint32(message, f("repeated_fixed32" ), 307);
  reflection->adduint64(message, f("repeated_fixed64" ), 308);
  reflection->addint32 (message, f("repeated_sfixed32"), 309);
  reflection->addint64 (message, f("repeated_sfixed64"), 310);
  reflection->addfloat (message, f("repeated_float"   ), 311);
  reflection->adddouble(message, f("repeated_double"  ), 312);
  reflection->addbool  (message, f("repeated_bool"    ), false);
  reflection->addstring(message, f("repeated_string"  ), "315");
  reflection->addstring(message, f("repeated_bytes"   ), "316");

  sub_message = reflection->addmessage(message, f("repeatedgroup"));
  sub_message->getreflection()->setint32(sub_message, repeated_group_a_, 317);
  sub_message = reflection->addmessage(message, f("repeated_nested_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 318);
  sub_message = reflection->addmessage(message, f("repeated_foreign_message"));
  sub_message->getreflection()->setint32(sub_message, foreign_c_, 319);
  sub_message = reflection->addmessage(message, f("repeated_import_message"));
  sub_message->getreflection()->setint32(sub_message, import_d_, 320);
  sub_message = reflection->addmessage(message, f("repeated_lazy_message"));
  sub_message->getreflection()->setint32(sub_message, nested_b_, 327);

  reflection->addenum(message, f("repeated_nested_enum" ),  nested_baz_);
  reflection->addenum(message, f("repeated_foreign_enum"), foreign_baz_);
  reflection->addenum(message, f("repeated_import_enum" ),  import_baz_);

  reflection->addstring(message, f("repeated_string_piece"), "324");
  reflection->addstring(message, f("repeated_cord"), "325");

  // -----------------------------------------------------------------

  reflection->setint32 (message, f("default_int32"   ), 401);
  reflection->setint64 (message, f("default_int64"   ), 402);
  reflection->setuint32(message, f("default_uint32"  ), 403);
  reflection->setuint64(message, f("default_uint64"  ), 404);
  reflection->setint32 (message, f("default_sint32"  ), 405);
  reflection->setint64 (message, f("default_sint64"  ), 406);
  reflection->setuint32(message, f("default_fixed32" ), 407);
  reflection->setuint64(message, f("default_fixed64" ), 408);
  reflection->setint32 (message, f("default_sfixed32"), 409);
  reflection->setint64 (message, f("default_sfixed64"), 410);
  reflection->setfloat (message, f("default_float"   ), 411);
  reflection->setdouble(message, f("default_double"  ), 412);
  reflection->setbool  (message, f("default_bool"    ), false);
  reflection->setstring(message, f("default_string"  ), "415");
  reflection->setstring(message, f("default_bytes"   ), "416");

  reflection->setenum(message, f("default_nested_enum" ),  nested_foo_);
  reflection->setenum(message, f("default_foreign_enum"), foreign_foo_);
  reflection->setenum(message, f("default_import_enum" ),  import_foo_);

  reflection->setstring(message, f("default_string_piece"), "424");
  reflection->setstring(message, f("default_cord"), "425");
}

void testutil::reflectiontester::setpackedfieldsviareflection(
    message* message) {
  const reflection* reflection = message->getreflection();
  reflection->addint32 (message, f("packed_int32"   ), 601);
  reflection->addint64 (message, f("packed_int64"   ), 602);
  reflection->adduint32(message, f("packed_uint32"  ), 603);
  reflection->adduint64(message, f("packed_uint64"  ), 604);
  reflection->addint32 (message, f("packed_sint32"  ), 605);
  reflection->addint64 (message, f("packed_sint64"  ), 606);
  reflection->adduint32(message, f("packed_fixed32" ), 607);
  reflection->adduint64(message, f("packed_fixed64" ), 608);
  reflection->addint32 (message, f("packed_sfixed32"), 609);
  reflection->addint64 (message, f("packed_sfixed64"), 610);
  reflection->addfloat (message, f("packed_float"   ), 611);
  reflection->adddouble(message, f("packed_double"  ), 612);
  reflection->addbool  (message, f("packed_bool"    ), true);
  reflection->addenum  (message, f("packed_enum"    ), foreign_bar_);

  reflection->addint32 (message, f("packed_int32"   ), 701);
  reflection->addint64 (message, f("packed_int64"   ), 702);
  reflection->adduint32(message, f("packed_uint32"  ), 703);
  reflection->adduint64(message, f("packed_uint64"  ), 704);
  reflection->addint32 (message, f("packed_sint32"  ), 705);
  reflection->addint64 (message, f("packed_sint64"  ), 706);
  reflection->adduint32(message, f("packed_fixed32" ), 707);
  reflection->adduint64(message, f("packed_fixed64" ), 708);
  reflection->addint32 (message, f("packed_sfixed32"), 709);
  reflection->addint64 (message, f("packed_sfixed64"), 710);
  reflection->addfloat (message, f("packed_float"   ), 711);
  reflection->adddouble(message, f("packed_double"  ), 712);
  reflection->addbool  (message, f("packed_bool"    ), false);
  reflection->addenum  (message, f("packed_enum"    ), foreign_baz_);
}

// -------------------------------------------------------------------

void testutil::reflectiontester::expectallfieldssetviareflection(
    const message& message) {
  // we have to split this into three function otherwise it creates a stack
  // frame so large that it triggers a warning.
  expectallfieldssetviareflection1(message);
  expectallfieldssetviareflection2(message);
  expectallfieldssetviareflection3(message);
}

void testutil::reflectiontester::expectallfieldssetviareflection1(
    const message& message) {
  const reflection* reflection = message.getreflection();
  string scratch;
  const message* sub_message;

  expect_true(reflection->hasfield(message, f("optional_int32"   )));
  expect_true(reflection->hasfield(message, f("optional_int64"   )));
  expect_true(reflection->hasfield(message, f("optional_uint32"  )));
  expect_true(reflection->hasfield(message, f("optional_uint64"  )));
  expect_true(reflection->hasfield(message, f("optional_sint32"  )));
  expect_true(reflection->hasfield(message, f("optional_sint64"  )));
  expect_true(reflection->hasfield(message, f("optional_fixed32" )));
  expect_true(reflection->hasfield(message, f("optional_fixed64" )));
  expect_true(reflection->hasfield(message, f("optional_sfixed32")));
  expect_true(reflection->hasfield(message, f("optional_sfixed64")));
  expect_true(reflection->hasfield(message, f("optional_float"   )));
  expect_true(reflection->hasfield(message, f("optional_double"  )));
  expect_true(reflection->hasfield(message, f("optional_bool"    )));
  expect_true(reflection->hasfield(message, f("optional_string"  )));
  expect_true(reflection->hasfield(message, f("optional_bytes"   )));

  expect_true(reflection->hasfield(message, f("optionalgroup"                 )));
  expect_true(reflection->hasfield(message, f("optional_nested_message"       )));
  expect_true(reflection->hasfield(message, f("optional_foreign_message"      )));
  expect_true(reflection->hasfield(message, f("optional_import_message"       )));
  expect_true(reflection->hasfield(message, f("optional_public_import_message")));
  expect_true(reflection->hasfield(message, f("optional_lazy_message"         )));

  sub_message = &reflection->getmessage(message, f("optionalgroup"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, group_a_));
  sub_message = &reflection->getmessage(message, f("optional_nested_message"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, nested_b_));
  sub_message = &reflection->getmessage(message, f("optional_foreign_message"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, foreign_c_));
  sub_message = &reflection->getmessage(message, f("optional_import_message"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, import_d_));
  sub_message = &reflection->getmessage(message, f("optional_public_import_message"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, import_e_));
  sub_message = &reflection->getmessage(message, f("optional_lazy_message"));
  expect_true(sub_message->getreflection()->hasfield(*sub_message, nested_b_));

  expect_true(reflection->hasfield(message, f("optional_nested_enum" )));
  expect_true(reflection->hasfield(message, f("optional_foreign_enum")));
  expect_true(reflection->hasfield(message, f("optional_import_enum" )));

  expect_true(reflection->hasfield(message, f("optional_string_piece")));
  expect_true(reflection->hasfield(message, f("optional_cord")));

  expect_eq(101  , reflection->getint32 (message, f("optional_int32"   )));
  expect_eq(102  , reflection->getint64 (message, f("optional_int64"   )));
  expect_eq(103  , reflection->getuint32(message, f("optional_uint32"  )));
  expect_eq(104  , reflection->getuint64(message, f("optional_uint64"  )));
  expect_eq(105  , reflection->getint32 (message, f("optional_sint32"  )));
  expect_eq(106  , reflection->getint64 (message, f("optional_sint64"  )));
  expect_eq(107  , reflection->getuint32(message, f("optional_fixed32" )));
  expect_eq(108  , reflection->getuint64(message, f("optional_fixed64" )));
  expect_eq(109  , reflection->getint32 (message, f("optional_sfixed32")));
  expect_eq(110  , reflection->getint64 (message, f("optional_sfixed64")));
  expect_eq(111  , reflection->getfloat (message, f("optional_float"   )));
  expect_eq(112  , reflection->getdouble(message, f("optional_double"  )));
  expect_true(     reflection->getbool  (message, f("optional_bool"    )));
  expect_eq("115", reflection->getstring(message, f("optional_string"  )));
  expect_eq("116", reflection->getstring(message, f("optional_bytes"   )));

  expect_eq("115", reflection->getstringreference(message, f("optional_string"), &scratch));
  expect_eq("116", reflection->getstringreference(message, f("optional_bytes" ), &scratch));

  sub_message = &reflection->getmessage(message, f("optionalgroup"));
  expect_eq(117, sub_message->getreflection()->getint32(*sub_message, group_a_));
  sub_message = &reflection->getmessage(message, f("optional_nested_message"));
  expect_eq(118, sub_message->getreflection()->getint32(*sub_message, nested_b_));
  sub_message = &reflection->getmessage(message, f("optional_foreign_message"));
  expect_eq(119, sub_message->getreflection()->getint32(*sub_message, foreign_c_));
  sub_message = &reflection->getmessage(message, f("optional_import_message"));
  expect_eq(120, sub_message->getreflection()->getint32(*sub_message, import_d_));
  sub_message = &reflection->getmessage(message, f("optional_public_import_message"));
  expect_eq(126, sub_message->getreflection()->getint32(*sub_message, import_e_));
  sub_message = &reflection->getmessage(message, f("optional_lazy_message"));
  expect_eq(127, sub_message->getreflection()->getint32(*sub_message, nested_b_));

  expect_eq( nested_baz_, reflection->getenum(message, f("optional_nested_enum" )));
  expect_eq(foreign_baz_, reflection->getenum(message, f("optional_foreign_enum")));
  expect_eq( import_baz_, reflection->getenum(message, f("optional_import_enum" )));

  expect_eq("124", reflection->getstring(message, f("optional_string_piece")));
  expect_eq("124", reflection->getstringreference(message, f("optional_string_piece"), &scratch));

  expect_eq("125", reflection->getstring(message, f("optional_cord")));
  expect_eq("125", reflection->getstringreference(message, f("optional_cord"), &scratch));

}

void testutil::reflectiontester::expectallfieldssetviareflection2(
    const message& message) {
  const reflection* reflection = message.getreflection();
  string scratch;
  const message* sub_message;

  // -----------------------------------------------------------------

  assert_eq(2, reflection->fieldsize(message, f("repeated_int32"   )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_int64"   )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_uint32"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_uint64"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_sint32"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_sint64"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_fixed32" )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_fixed64" )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_sfixed32")));
  assert_eq(2, reflection->fieldsize(message, f("repeated_sfixed64")));
  assert_eq(2, reflection->fieldsize(message, f("repeated_float"   )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_double"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_bool"    )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_string"  )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_bytes"   )));

  assert_eq(2, reflection->fieldsize(message, f("repeatedgroup"           )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_nested_message" )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_foreign_message")));
  assert_eq(2, reflection->fieldsize(message, f("repeated_import_message" )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_lazy_message"   )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_nested_enum"    )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_foreign_enum"   )));
  assert_eq(2, reflection->fieldsize(message, f("repeated_import_enum"    )));

  assert_eq(2, reflection->fieldsize(message, f("repeated_string_piece")));
  assert_eq(2, reflection->fieldsize(message, f("repeated_cord")));

  expect_eq(201  , reflection->getrepeatedint32 (message, f("repeated_int32"   ), 0));
  expect_eq(202  , reflection->getrepeatedint64 (message, f("repeated_int64"   ), 0));
  expect_eq(203  , reflection->getrepeateduint32(message, f("repeated_uint32"  ), 0));
  expect_eq(204  , reflection->getrepeateduint64(message, f("repeated_uint64"  ), 0));
  expect_eq(205  , reflection->getrepeatedint32 (message, f("repeated_sint32"  ), 0));
  expect_eq(206  , reflection->getrepeatedint64 (message, f("repeated_sint64"  ), 0));
  expect_eq(207  , reflection->getrepeateduint32(message, f("repeated_fixed32" ), 0));
  expect_eq(208  , reflection->getrepeateduint64(message, f("repeated_fixed64" ), 0));
  expect_eq(209  , reflection->getrepeatedint32 (message, f("repeated_sfixed32"), 0));
  expect_eq(210  , reflection->getrepeatedint64 (message, f("repeated_sfixed64"), 0));
  expect_eq(211  , reflection->getrepeatedfloat (message, f("repeated_float"   ), 0));
  expect_eq(212  , reflection->getrepeateddouble(message, f("repeated_double"  ), 0));
  expect_true(     reflection->getrepeatedbool  (message, f("repeated_bool"    ), 0));
  expect_eq("215", reflection->getrepeatedstring(message, f("repeated_string"  ), 0));
  expect_eq("216", reflection->getrepeatedstring(message, f("repeated_bytes"   ), 0));

  expect_eq("215", reflection->getrepeatedstringreference(message, f("repeated_string"), 0, &scratch));
  expect_eq("216", reflection->getrepeatedstringreference(message, f("repeated_bytes"), 0, &scratch));

  sub_message = &reflection->getrepeatedmessage(message, f("repeatedgroup"), 0);
  expect_eq(217, sub_message->getreflection()->getint32(*sub_message, repeated_group_a_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_nested_message"), 0);
  expect_eq(218, sub_message->getreflection()->getint32(*sub_message, nested_b_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_foreign_message"), 0);
  expect_eq(219, sub_message->getreflection()->getint32(*sub_message, foreign_c_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_import_message"), 0);
  expect_eq(220, sub_message->getreflection()->getint32(*sub_message, import_d_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_lazy_message"), 0);
  expect_eq(227, sub_message->getreflection()->getint32(*sub_message, nested_b_));

  expect_eq( nested_bar_, reflection->getrepeatedenum(message, f("repeated_nested_enum" ),0));
  expect_eq(foreign_bar_, reflection->getrepeatedenum(message, f("repeated_foreign_enum"),0));
  expect_eq( import_bar_, reflection->getrepeatedenum(message, f("repeated_import_enum" ),0));

  expect_eq("224", reflection->getrepeatedstring(message, f("repeated_string_piece"), 0));
  expect_eq("224", reflection->getrepeatedstringreference(
                        message, f("repeated_string_piece"), 0, &scratch));

  expect_eq("225", reflection->getrepeatedstring(message, f("repeated_cord"), 0));
  expect_eq("225", reflection->getrepeatedstringreference(
                        message, f("repeated_cord"), 0, &scratch));

  expect_eq(301  , reflection->getrepeatedint32 (message, f("repeated_int32"   ), 1));
  expect_eq(302  , reflection->getrepeatedint64 (message, f("repeated_int64"   ), 1));
  expect_eq(303  , reflection->getrepeateduint32(message, f("repeated_uint32"  ), 1));
  expect_eq(304  , reflection->getrepeateduint64(message, f("repeated_uint64"  ), 1));
  expect_eq(305  , reflection->getrepeatedint32 (message, f("repeated_sint32"  ), 1));
  expect_eq(306  , reflection->getrepeatedint64 (message, f("repeated_sint64"  ), 1));
  expect_eq(307  , reflection->getrepeateduint32(message, f("repeated_fixed32" ), 1));
  expect_eq(308  , reflection->getrepeateduint64(message, f("repeated_fixed64" ), 1));
  expect_eq(309  , reflection->getrepeatedint32 (message, f("repeated_sfixed32"), 1));
  expect_eq(310  , reflection->getrepeatedint64 (message, f("repeated_sfixed64"), 1));
  expect_eq(311  , reflection->getrepeatedfloat (message, f("repeated_float"   ), 1));
  expect_eq(312  , reflection->getrepeateddouble(message, f("repeated_double"  ), 1));
  expect_false(    reflection->getrepeatedbool  (message, f("repeated_bool"    ), 1));
  expect_eq("315", reflection->getrepeatedstring(message, f("repeated_string"  ), 1));
  expect_eq("316", reflection->getrepeatedstring(message, f("repeated_bytes"   ), 1));

  expect_eq("315", reflection->getrepeatedstringreference(message, f("repeated_string"),
                                                          1, &scratch));
  expect_eq("316", reflection->getrepeatedstringreference(message, f("repeated_bytes"),
                                                          1, &scratch));

  sub_message = &reflection->getrepeatedmessage(message, f("repeatedgroup"), 1);
  expect_eq(317, sub_message->getreflection()->getint32(*sub_message, repeated_group_a_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_nested_message"), 1);
  expect_eq(318, sub_message->getreflection()->getint32(*sub_message, nested_b_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_foreign_message"), 1);
  expect_eq(319, sub_message->getreflection()->getint32(*sub_message, foreign_c_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_import_message"), 1);
  expect_eq(320, sub_message->getreflection()->getint32(*sub_message, import_d_));
  sub_message = &reflection->getrepeatedmessage(message, f("repeated_lazy_message"), 1);
  expect_eq(327, sub_message->getreflection()->getint32(*sub_message, nested_b_));

  expect_eq( nested_baz_, reflection->getrepeatedenum(message, f("repeated_nested_enum" ),1));
  expect_eq(foreign_baz_, reflection->getrepeatedenum(message, f("repeated_foreign_enum"),1));
  expect_eq( import_baz_, reflection->getrepeatedenum(message, f("repeated_import_enum" ),1));

  expect_eq("324", reflection->getrepeatedstring(message, f("repeated_string_piece"), 1));
  expect_eq("324", reflection->getrepeatedstringreference(
                        message, f("repeated_string_piece"), 1, &scratch));

  expect_eq("325", reflection->getrepeatedstring(message, f("repeated_cord"), 1));
  expect_eq("325", reflection->getrepeatedstringreference(
                        message, f("repeated_cord"), 1, &scratch));
}

void testutil::reflectiontester::expectallfieldssetviareflection3(
    const message& message) {
  const reflection* reflection = message.getreflection();
  string scratch;

  // -----------------------------------------------------------------

  expect_true(reflection->hasfield(message, f("default_int32"   )));
  expect_true(reflection->hasfield(message, f("default_int64"   )));
  expect_true(reflection->hasfield(message, f("default_uint32"  )));
  expect_true(reflection->hasfield(message, f("default_uint64"  )));
  expect_true(reflection->hasfield(message, f("default_sint32"  )));
  expect_true(reflection->hasfield(message, f("default_sint64"  )));
  expect_true(reflection->hasfield(message, f("default_fixed32" )));
  expect_true(reflection->hasfield(message, f("default_fixed64" )));
  expect_true(reflection->hasfield(message, f("default_sfixed32")));
  expect_true(reflection->hasfield(message, f("default_sfixed64")));
  expect_true(reflection->hasfield(message, f("default_float"   )));
  expect_true(reflection->hasfield(message, f("default_double"  )));
  expect_true(reflection->hasfield(message, f("default_bool"    )));
  expect_true(reflection->hasfield(message, f("default_string"  )));
  expect_true(reflection->hasfield(message, f("default_bytes"   )));

  expect_true(reflection->hasfield(message, f("default_nested_enum" )));
  expect_true(reflection->hasfield(message, f("default_foreign_enum")));
  expect_true(reflection->hasfield(message, f("default_import_enum" )));

  expect_true(reflection->hasfield(message, f("default_string_piece")));
  expect_true(reflection->hasfield(message, f("default_cord")));

  expect_eq(401  , reflection->getint32 (message, f("default_int32"   )));
  expect_eq(402  , reflection->getint64 (message, f("default_int64"   )));
  expect_eq(403  , reflection->getuint32(message, f("default_uint32"  )));
  expect_eq(404  , reflection->getuint64(message, f("default_uint64"  )));
  expect_eq(405  , reflection->getint32 (message, f("default_sint32"  )));
  expect_eq(406  , reflection->getint64 (message, f("default_sint64"  )));
  expect_eq(407  , reflection->getuint32(message, f("default_fixed32" )));
  expect_eq(408  , reflection->getuint64(message, f("default_fixed64" )));
  expect_eq(409  , reflection->getint32 (message, f("default_sfixed32")));
  expect_eq(410  , reflection->getint64 (message, f("default_sfixed64")));
  expect_eq(411  , reflection->getfloat (message, f("default_float"   )));
  expect_eq(412  , reflection->getdouble(message, f("default_double"  )));
  expect_false(    reflection->getbool  (message, f("default_bool"    )));
  expect_eq("415", reflection->getstring(message, f("default_string"  )));
  expect_eq("416", reflection->getstring(message, f("default_bytes"   )));

  expect_eq("415", reflection->getstringreference(message, f("default_string"), &scratch));
  expect_eq("416", reflection->getstringreference(message, f("default_bytes" ), &scratch));

  expect_eq( nested_foo_, reflection->getenum(message, f("default_nested_enum" )));
  expect_eq(foreign_foo_, reflection->getenum(message, f("default_foreign_enum")));
  expect_eq( import_foo_, reflection->getenum(message, f("default_import_enum" )));

  expect_eq("424", reflection->getstring(message, f("default_string_piece")));
  expect_eq("424", reflection->getstringreference(message, f("default_string_piece"),
                                                  &scratch));

  expect_eq("425", reflection->getstring(message, f("default_cord")));
  expect_eq("425", reflection->getstringreference(message, f("default_cord"), &scratch));
}

void testutil::reflectiontester::expectpackedfieldssetviareflection(
    const message& message) {
  const reflection* reflection = message.getreflection();

  assert_eq(2, reflection->fieldsize(message, f("packed_int32"   )));
  assert_eq(2, reflection->fieldsize(message, f("packed_int64"   )));
  assert_eq(2, reflection->fieldsize(message, f("packed_uint32"  )));
  assert_eq(2, reflection->fieldsize(message, f("packed_uint64"  )));
  assert_eq(2, reflection->fieldsize(message, f("packed_sint32"  )));
  assert_eq(2, reflection->fieldsize(message, f("packed_sint64"  )));
  assert_eq(2, reflection->fieldsize(message, f("packed_fixed32" )));
  assert_eq(2, reflection->fieldsize(message, f("packed_fixed64" )));
  assert_eq(2, reflection->fieldsize(message, f("packed_sfixed32")));
  assert_eq(2, reflection->fieldsize(message, f("packed_sfixed64")));
  assert_eq(2, reflection->fieldsize(message, f("packed_float"   )));
  assert_eq(2, reflection->fieldsize(message, f("packed_double"  )));
  assert_eq(2, reflection->fieldsize(message, f("packed_bool"    )));
  assert_eq(2, reflection->fieldsize(message, f("packed_enum"    )));

  expect_eq(601  , reflection->getrepeatedint32 (message, f("packed_int32"   ), 0));
  expect_eq(602  , reflection->getrepeatedint64 (message, f("packed_int64"   ), 0));
  expect_eq(603  , reflection->getrepeateduint32(message, f("packed_uint32"  ), 0));
  expect_eq(604  , reflection->getrepeateduint64(message, f("packed_uint64"  ), 0));
  expect_eq(605  , reflection->getrepeatedint32 (message, f("packed_sint32"  ), 0));
  expect_eq(606  , reflection->getrepeatedint64 (message, f("packed_sint64"  ), 0));
  expect_eq(607  , reflection->getrepeateduint32(message, f("packed_fixed32" ), 0));
  expect_eq(608  , reflection->getrepeateduint64(message, f("packed_fixed64" ), 0));
  expect_eq(609  , reflection->getrepeatedint32 (message, f("packed_sfixed32"), 0));
  expect_eq(610  , reflection->getrepeatedint64 (message, f("packed_sfixed64"), 0));
  expect_eq(611  , reflection->getrepeatedfloat (message, f("packed_float"   ), 0));
  expect_eq(612  , reflection->getrepeateddouble(message, f("packed_double"  ), 0));
  expect_true(     reflection->getrepeatedbool  (message, f("packed_bool"    ), 0));
  expect_eq(foreign_bar_,
            reflection->getrepeatedenum(message, f("packed_enum"), 0));

  expect_eq(701  , reflection->getrepeatedint32 (message, f("packed_int32"   ), 1));
  expect_eq(702  , reflection->getrepeatedint64 (message, f("packed_int64"   ), 1));
  expect_eq(703  , reflection->getrepeateduint32(message, f("packed_uint32"  ), 1));
  expect_eq(704  , reflection->getrepeateduint64(message, f("packed_uint64"  ), 1));
  expect_eq(705  , reflection->getrepeatedint32 (message, f("packed_sint32"  ), 1));
  expect_eq(706  , reflection->getrepeatedint64 (message, f("packed_sint64"  ), 1));
  expect_eq(707  , reflection->getrepeateduint32(message, f("packed_fixed32" ), 1));
  expect_eq(708  , reflection->getrepeateduint64(message, f("packed_fixed64" ), 1));
  expect_eq(709  , reflection->getrepeatedint32 (message, f("packed_sfixed32"), 1));
  expect_eq(710  , reflection->getrepeatedint64 (message, f("packed_sfixed64"), 1));
  expect_eq(711  , reflection->getrepeatedfloat (message, f("packed_float"   ), 1));
  expect_eq(712  , reflection->getrepeateddouble(message, f("packed_double"  ), 1));
  expect_false(    reflection->getrepeatedbool  (message, f("packed_bool"    ), 1));
  expect_eq(foreign_baz_,
            reflection->getrepeatedenum(message, f("packed_enum"), 1));
}

// -------------------------------------------------------------------

void testutil::reflectiontester::expectclearviareflection(
    const message& message) {
  const reflection* reflection = message.getreflection();
  string scratch;
  const message* sub_message;

  // has_blah() should initially be false for all optional fields.
  expect_false(reflection->hasfield(message, f("optional_int32"   )));
  expect_false(reflection->hasfield(message, f("optional_int64"   )));
  expect_false(reflection->hasfield(message, f("optional_uint32"  )));
  expect_false(reflection->hasfield(message, f("optional_uint64"  )));
  expect_false(reflection->hasfield(message, f("optional_sint32"  )));
  expect_false(reflection->hasfield(message, f("optional_sint64"  )));
  expect_false(reflection->hasfield(message, f("optional_fixed32" )));
  expect_false(reflection->hasfield(message, f("optional_fixed64" )));
  expect_false(reflection->hasfield(message, f("optional_sfixed32")));
  expect_false(reflection->hasfield(message, f("optional_sfixed64")));
  expect_false(reflection->hasfield(message, f("optional_float"   )));
  expect_false(reflection->hasfield(message, f("optional_double"  )));
  expect_false(reflection->hasfield(message, f("optional_bool"    )));
  expect_false(reflection->hasfield(message, f("optional_string"  )));
  expect_false(reflection->hasfield(message, f("optional_bytes"   )));

  expect_false(reflection->hasfield(message, f("optionalgroup"           )));
  expect_false(reflection->hasfield(message, f("optional_nested_message" )));
  expect_false(reflection->hasfield(message, f("optional_foreign_message")));
  expect_false(reflection->hasfield(message, f("optional_import_message" )));
  expect_false(reflection->hasfield(message, f("optional_public_import_message")));
  expect_false(reflection->hasfield(message, f("optional_lazy_message")));

  expect_false(reflection->hasfield(message, f("optional_nested_enum" )));
  expect_false(reflection->hasfield(message, f("optional_foreign_enum")));
  expect_false(reflection->hasfield(message, f("optional_import_enum" )));

  expect_false(reflection->hasfield(message, f("optional_string_piece")));
  expect_false(reflection->hasfield(message, f("optional_cord")));

  // optional fields without defaults are set to zero or something like it.
  expect_eq(0    , reflection->getint32 (message, f("optional_int32"   )));
  expect_eq(0    , reflection->getint64 (message, f("optional_int64"   )));
  expect_eq(0    , reflection->getuint32(message, f("optional_uint32"  )));
  expect_eq(0    , reflection->getuint64(message, f("optional_uint64"  )));
  expect_eq(0    , reflection->getint32 (message, f("optional_sint32"  )));
  expect_eq(0    , reflection->getint64 (message, f("optional_sint64"  )));
  expect_eq(0    , reflection->getuint32(message, f("optional_fixed32" )));
  expect_eq(0    , reflection->getuint64(message, f("optional_fixed64" )));
  expect_eq(0    , reflection->getint32 (message, f("optional_sfixed32")));
  expect_eq(0    , reflection->getint64 (message, f("optional_sfixed64")));
  expect_eq(0    , reflection->getfloat (message, f("optional_float"   )));
  expect_eq(0    , reflection->getdouble(message, f("optional_double"  )));
  expect_false(    reflection->getbool  (message, f("optional_bool"    )));
  expect_eq(""   , reflection->getstring(message, f("optional_string"  )));
  expect_eq(""   , reflection->getstring(message, f("optional_bytes"   )));

  expect_eq("", reflection->getstringreference(message, f("optional_string"), &scratch));
  expect_eq("", reflection->getstringreference(message, f("optional_bytes" ), &scratch));

  // embedded messages should also be clear.
  sub_message = &reflection->getmessage(message, f("optionalgroup"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, group_a_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, group_a_));
  sub_message = &reflection->getmessage(message, f("optional_nested_message"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, nested_b_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, nested_b_));
  sub_message = &reflection->getmessage(message, f("optional_foreign_message"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, foreign_c_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, foreign_c_));
  sub_message = &reflection->getmessage(message, f("optional_import_message"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, import_d_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, import_d_));
  sub_message = &reflection->getmessage(message, f("optional_public_import_message"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, import_e_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, import_e_));
  sub_message = &reflection->getmessage(message, f("optional_lazy_message"));
  expect_false(sub_message->getreflection()->hasfield(*sub_message, nested_b_));
  expect_eq(0, sub_message->getreflection()->getint32(*sub_message, nested_b_));

  // enums without defaults are set to the first value in the enum.
  expect_eq( nested_foo_, reflection->getenum(message, f("optional_nested_enum" )));
  expect_eq(foreign_foo_, reflection->getenum(message, f("optional_foreign_enum")));
  expect_eq( import_foo_, reflection->getenum(message, f("optional_import_enum" )));

  expect_eq("", reflection->getstring(message, f("optional_string_piece")));
  expect_eq("", reflection->getstringreference(message, f("optional_string_piece"), &scratch));

  expect_eq("", reflection->getstring(message, f("optional_cord")));
  expect_eq("", reflection->getstringreference(message, f("optional_cord"), &scratch));

  // repeated fields are empty.
  expect_eq(0, reflection->fieldsize(message, f("repeated_int32"   )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_int64"   )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_uint32"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_uint64"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_sint32"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_sint64"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_fixed32" )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_fixed64" )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_sfixed32")));
  expect_eq(0, reflection->fieldsize(message, f("repeated_sfixed64")));
  expect_eq(0, reflection->fieldsize(message, f("repeated_float"   )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_double"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_bool"    )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_string"  )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_bytes"   )));

  expect_eq(0, reflection->fieldsize(message, f("repeatedgroup"           )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_nested_message" )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_foreign_message")));
  expect_eq(0, reflection->fieldsize(message, f("repeated_import_message" )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_lazy_message"   )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_nested_enum"    )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_foreign_enum"   )));
  expect_eq(0, reflection->fieldsize(message, f("repeated_import_enum"    )));

  expect_eq(0, reflection->fieldsize(message, f("repeated_string_piece")));
  expect_eq(0, reflection->fieldsize(message, f("repeated_cord")));

  // has_blah() should also be false for all default fields.
  expect_false(reflection->hasfield(message, f("default_int32"   )));
  expect_false(reflection->hasfield(message, f("default_int64"   )));
  expect_false(reflection->hasfield(message, f("default_uint32"  )));
  expect_false(reflection->hasfield(message, f("default_uint64"  )));
  expect_false(reflection->hasfield(message, f("default_sint32"  )));
  expect_false(reflection->hasfield(message, f("default_sint64"  )));
  expect_false(reflection->hasfield(message, f("default_fixed32" )));
  expect_false(reflection->hasfield(message, f("default_fixed64" )));
  expect_false(reflection->hasfield(message, f("default_sfixed32")));
  expect_false(reflection->hasfield(message, f("default_sfixed64")));
  expect_false(reflection->hasfield(message, f("default_float"   )));
  expect_false(reflection->hasfield(message, f("default_double"  )));
  expect_false(reflection->hasfield(message, f("default_bool"    )));
  expect_false(reflection->hasfield(message, f("default_string"  )));
  expect_false(reflection->hasfield(message, f("default_bytes"   )));

  expect_false(reflection->hasfield(message, f("default_nested_enum" )));
  expect_false(reflection->hasfield(message, f("default_foreign_enum")));
  expect_false(reflection->hasfield(message, f("default_import_enum" )));

  expect_false(reflection->hasfield(message, f("default_string_piece")));
  expect_false(reflection->hasfield(message, f("default_cord")));

  // fields with defaults have their default values (duh).
  expect_eq( 41    , reflection->getint32 (message, f("default_int32"   )));
  expect_eq( 42    , reflection->getint64 (message, f("default_int64"   )));
  expect_eq( 43    , reflection->getuint32(message, f("default_uint32"  )));
  expect_eq( 44    , reflection->getuint64(message, f("default_uint64"  )));
  expect_eq(-45    , reflection->getint32 (message, f("default_sint32"  )));
  expect_eq( 46    , reflection->getint64 (message, f("default_sint64"  )));
  expect_eq( 47    , reflection->getuint32(message, f("default_fixed32" )));
  expect_eq( 48    , reflection->getuint64(message, f("default_fixed64" )));
  expect_eq( 49    , reflection->getint32 (message, f("default_sfixed32")));
  expect_eq(-50    , reflection->getint64 (message, f("default_sfixed64")));
  expect_eq( 51.5  , reflection->getfloat (message, f("default_float"   )));
  expect_eq( 52e3  , reflection->getdouble(message, f("default_double"  )));
  expect_true(       reflection->getbool  (message, f("default_bool"    )));
  expect_eq("hello", reflection->getstring(message, f("default_string"  )));
  expect_eq("world", reflection->getstring(message, f("default_bytes"   )));

  expect_eq("hello", reflection->getstringreference(message, f("default_string"), &scratch));
  expect_eq("world", reflection->getstringreference(message, f("default_bytes" ), &scratch));

  expect_eq( nested_bar_, reflection->getenum(message, f("default_nested_enum" )));
  expect_eq(foreign_bar_, reflection->getenum(message, f("default_foreign_enum")));
  expect_eq( import_bar_, reflection->getenum(message, f("default_import_enum" )));

  expect_eq("abc", reflection->getstring(message, f("default_string_piece")));
  expect_eq("abc", reflection->getstringreference(message, f("default_string_piece"), &scratch));

  expect_eq("123", reflection->getstring(message, f("default_cord")));
  expect_eq("123", reflection->getstringreference(message, f("default_cord"), &scratch));
}

void testutil::reflectiontester::expectpackedclearviareflection(
    const message& message) {
  const reflection* reflection = message.getreflection();

  expect_eq(0, reflection->fieldsize(message, f("packed_int32"   )));
  expect_eq(0, reflection->fieldsize(message, f("packed_int64"   )));
  expect_eq(0, reflection->fieldsize(message, f("packed_uint32"  )));
  expect_eq(0, reflection->fieldsize(message, f("packed_uint64"  )));
  expect_eq(0, reflection->fieldsize(message, f("packed_sint32"  )));
  expect_eq(0, reflection->fieldsize(message, f("packed_sint64"  )));
  expect_eq(0, reflection->fieldsize(message, f("packed_fixed32" )));
  expect_eq(0, reflection->fieldsize(message, f("packed_fixed64" )));
  expect_eq(0, reflection->fieldsize(message, f("packed_sfixed32")));
  expect_eq(0, reflection->fieldsize(message, f("packed_sfixed64")));
  expect_eq(0, reflection->fieldsize(message, f("packed_float"   )));
  expect_eq(0, reflection->fieldsize(message, f("packed_double"  )));
  expect_eq(0, reflection->fieldsize(message, f("packed_bool"    )));
  expect_eq(0, reflection->fieldsize(message, f("packed_enum"    )));
}

// -------------------------------------------------------------------

void testutil::reflectiontester::modifyrepeatedfieldsviareflection(
    message* message) {
  const reflection* reflection = message->getreflection();
  message* sub_message;

  reflection->setrepeatedint32 (message, f("repeated_int32"   ), 1, 501);
  reflection->setrepeatedint64 (message, f("repeated_int64"   ), 1, 502);
  reflection->setrepeateduint32(message, f("repeated_uint32"  ), 1, 503);
  reflection->setrepeateduint64(message, f("repeated_uint64"  ), 1, 504);
  reflection->setrepeatedint32 (message, f("repeated_sint32"  ), 1, 505);
  reflection->setrepeatedint64 (message, f("repeated_sint64"  ), 1, 506);
  reflection->setrepeateduint32(message, f("repeated_fixed32" ), 1, 507);
  reflection->setrepeateduint64(message, f("repeated_fixed64" ), 1, 508);
  reflection->setrepeatedint32 (message, f("repeated_sfixed32"), 1, 509);
  reflection->setrepeatedint64 (message, f("repeated_sfixed64"), 1, 510);
  reflection->setrepeatedfloat (message, f("repeated_float"   ), 1, 511);
  reflection->setrepeateddouble(message, f("repeated_double"  ), 1, 512);
  reflection->setrepeatedbool  (message, f("repeated_bool"    ), 1, true);
  reflection->setrepeatedstring(message, f("repeated_string"  ), 1, "515");
  reflection->setrepeatedstring(message, f("repeated_bytes"   ), 1, "516");

  sub_message = reflection->mutablerepeatedmessage(message, f("repeatedgroup"), 1);
  sub_message->getreflection()->setint32(sub_message, repeated_group_a_, 517);
  sub_message = reflection->mutablerepeatedmessage(message, f("repeated_nested_message"), 1);
  sub_message->getreflection()->setint32(sub_message, nested_b_, 518);
  sub_message = reflection->mutablerepeatedmessage(message, f("repeated_foreign_message"), 1);
  sub_message->getreflection()->setint32(sub_message, foreign_c_, 519);
  sub_message = reflection->mutablerepeatedmessage(message, f("repeated_import_message"), 1);
  sub_message->getreflection()->setint32(sub_message, import_d_, 520);
  sub_message = reflection->mutablerepeatedmessage(message, f("repeated_lazy_message"), 1);
  sub_message->getreflection()->setint32(sub_message, nested_b_, 527);

  reflection->setrepeatedenum(message, f("repeated_nested_enum" ), 1,  nested_foo_);
  reflection->setrepeatedenum(message, f("repeated_foreign_enum"), 1, foreign_foo_);
  reflection->setrepeatedenum(message, f("repeated_import_enum" ), 1,  import_foo_);

  reflection->setrepeatedstring(message, f("repeated_string_piece"), 1, "524");
  reflection->setrepeatedstring(message, f("repeated_cord"), 1, "525");
}

void testutil::reflectiontester::modifypackedfieldsviareflection(
    message* message) {
  const reflection* reflection = message->getreflection();
  reflection->setrepeatedint32 (message, f("packed_int32"   ), 1, 801);
  reflection->setrepeatedint64 (message, f("packed_int64"   ), 1, 802);
  reflection->setrepeateduint32(message, f("packed_uint32"  ), 1, 803);
  reflection->setrepeateduint64(message, f("packed_uint64"  ), 1, 804);
  reflection->setrepeatedint32 (message, f("packed_sint32"  ), 1, 805);
  reflection->setrepeatedint64 (message, f("packed_sint64"  ), 1, 806);
  reflection->setrepeateduint32(message, f("packed_fixed32" ), 1, 807);
  reflection->setrepeateduint64(message, f("packed_fixed64" ), 1, 808);
  reflection->setrepeatedint32 (message, f("packed_sfixed32"), 1, 809);
  reflection->setrepeatedint64 (message, f("packed_sfixed64"), 1, 810);
  reflection->setrepeatedfloat (message, f("packed_float"   ), 1, 811);
  reflection->setrepeateddouble(message, f("packed_double"  ), 1, 812);
  reflection->setrepeatedbool  (message, f("packed_bool"    ), 1, true);
  reflection->setrepeatedenum  (message, f("packed_enum"    ), 1, foreign_foo_);
}

void testutil::reflectiontester::removelastrepeatedsviareflection(
    message* message) {
  const reflection* reflection = message->getreflection();

  vector<const fielddescriptor*> output;
  reflection->listfields(*message, &output);
  for (int i=0; i<output.size(); ++i) {
    const fielddescriptor* field = output[i];
    if (!field->is_repeated()) continue;

    reflection->removelast(message, field);
  }
}

void testutil::reflectiontester::releaselastrepeatedsviareflection(
    message* message, bool expect_extensions_notnull) {
  const reflection* reflection = message->getreflection();

  vector<const fielddescriptor*> output;
  reflection->listfields(*message, &output);
  for (int i=0; i<output.size(); ++i) {
    const fielddescriptor* field = output[i];
    if (!field->is_repeated()) continue;
    if (field->cpp_type() != fielddescriptor::cpptype_message) continue;

    message* released = reflection->releaselast(message, field);
    if (!field->is_extension() || expect_extensions_notnull) {
      assert_true(released != null) << "releaselast returned null for: "
                                    << field->name();
    }
    delete released;
  }
}

void testutil::reflectiontester::swaprepeatedsviareflection(message* message) {
  const reflection* reflection = message->getreflection();

  vector<const fielddescriptor*> output;
  reflection->listfields(*message, &output);
  for (int i=0; i<output.size(); ++i) {
    const fielddescriptor* field = output[i];
    if (!field->is_repeated()) continue;

    reflection->swapelements(message, field, 0, 1);
  }
}

void testutil::reflectiontester::expectmessagesreleasedviareflection(
    message* message,
    testutil::reflectiontester::messagereleasestate expected_release_state) {
  const reflection* reflection = message->getreflection();

  static const char* fields[] = {
    "optionalgroup",
    "optional_nested_message",
    "optional_foreign_message",
    "optional_import_message",
  };
  for (int i = 0; i < google_arraysize(fields); i++) {
    const message& sub_message = reflection->getmessage(*message, f(fields[i]));
    message* released = reflection->releasemessage(message, f(fields[i]));
    switch (expected_release_state) {
      case is_null:
        expect_true(released == null);
        break;
      case not_null:
        expect_true(released != null);
        expect_eq(&sub_message, released);
        break;
      case can_be_null:
        break;
    }
    delete released;
    expect_false(reflection->hasfield(*message, f(fields[i])));
  }
}

}  // namespace protobuf
}  // namespace google
