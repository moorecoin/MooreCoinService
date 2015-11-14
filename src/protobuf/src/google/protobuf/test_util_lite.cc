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

#include <google/protobuf/test_util_lite.h>
#include <google/protobuf/stubs/common.h>


#define expect_true google_check
#define assert_true google_check
#define expect_false(cond) google_check(!(cond))
#define expect_eq google_check_eq
#define assert_eq google_check_eq

namespace google {
namespace protobuf {

void testutillite::setallfields(unittest::testalltypeslite* message) {
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

  message->set_optional_nested_enum (unittest::testalltypeslite::baz );
  message->set_optional_foreign_enum(unittest::foreign_lite_baz      );
  message->set_optional_import_enum (unittest_import::import_lite_baz);


  // -----------------------------------------------------------------

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

  message->add_repeated_nested_enum (unittest::testalltypeslite::bar );
  message->add_repeated_foreign_enum(unittest::foreign_lite_bar      );
  message->add_repeated_import_enum (unittest_import::import_lite_bar);


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

  message->add_repeated_nested_enum (unittest::testalltypeslite::baz );
  message->add_repeated_foreign_enum(unittest::foreign_lite_baz      );
  message->add_repeated_import_enum (unittest_import::import_lite_baz);


  // -----------------------------------------------------------------

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

  message->set_default_nested_enum (unittest::testalltypeslite::foo );
  message->set_default_foreign_enum(unittest::foreign_lite_foo      );
  message->set_default_import_enum (unittest_import::import_lite_foo);

}

// -------------------------------------------------------------------

void testutillite::modifyrepeatedfields(unittest::testalltypeslite* message) {
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

  message->set_repeated_nested_enum (1, unittest::testalltypeslite::foo );
  message->set_repeated_foreign_enum(1, unittest::foreign_lite_foo      );
  message->set_repeated_import_enum (1, unittest_import::import_lite_foo);

}

// -------------------------------------------------------------------

void testutillite::expectallfieldsset(
    const unittest::testalltypeslite& message) {
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
  expect_eq(true , message.optional_bool    ());
  expect_eq("115", message.optional_string  ());
  expect_eq("116", message.optional_bytes   ());

  expect_eq(117, message.optionalgroup                 ().a());
  expect_eq(118, message.optional_nested_message       ().bb());
  expect_eq(119, message.optional_foreign_message      ().c());
  expect_eq(120, message.optional_import_message       ().d());
  expect_eq(126, message.optional_public_import_message().e());
  expect_eq(127, message.optional_lazy_message         ().bb());

  expect_eq(unittest::testalltypeslite::baz , message.optional_nested_enum ());
  expect_eq(unittest::foreign_lite_baz      , message.optional_foreign_enum());
  expect_eq(unittest_import::import_lite_baz, message.optional_import_enum ());


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
  expect_eq(true , message.repeated_bool    (0));
  expect_eq("215", message.repeated_string  (0));
  expect_eq("216", message.repeated_bytes   (0));

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(227, message.repeated_lazy_message   (0).bb());


  expect_eq(unittest::testalltypeslite::bar , message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_lite_bar      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_lite_bar, message.repeated_import_enum (0));

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
  expect_eq(false, message.repeated_bool    (1));
  expect_eq("315", message.repeated_string  (1));
  expect_eq("316", message.repeated_bytes   (1));

  expect_eq(317, message.repeatedgroup           (1).a());
  expect_eq(318, message.repeated_nested_message (1).bb());
  expect_eq(319, message.repeated_foreign_message(1).c());
  expect_eq(320, message.repeated_import_message (1).d());
  expect_eq(327, message.repeated_lazy_message   (1).bb());

  expect_eq(unittest::testalltypeslite::baz , message.repeated_nested_enum (1));
  expect_eq(unittest::foreign_lite_baz      , message.repeated_foreign_enum(1));
  expect_eq(unittest_import::import_lite_baz, message.repeated_import_enum (1));


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
  expect_eq(false, message.default_bool    ());
  expect_eq("415", message.default_string  ());
  expect_eq("416", message.default_bytes   ());

  expect_eq(unittest::testalltypeslite::foo , message.default_nested_enum ());
  expect_eq(unittest::foreign_lite_foo      , message.default_foreign_enum());
  expect_eq(unittest_import::import_lite_foo, message.default_import_enum ());

}

// -------------------------------------------------------------------

void testutillite::expectclear(const unittest::testalltypeslite& message) {
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
  expect_eq(false, message.optional_bool    ());
  expect_eq(""   , message.optional_string  ());
  expect_eq(""   , message.optional_bytes   ());

  // embedded messages should also be clear.
  expect_false(message.optionalgroup                 ().has_a());
  expect_false(message.optional_nested_message       ().has_bb());
  expect_false(message.optional_foreign_message      ().has_c());
  expect_false(message.optional_import_message       ().has_d());
  expect_false(message.optional_public_import_message().has_e());
  expect_false(message.optional_lazy_message         ().has_bb());

  expect_eq(0, message.optionalgroup           ().a());
  expect_eq(0, message.optional_nested_message ().bb());
  expect_eq(0, message.optional_foreign_message().c());
  expect_eq(0, message.optional_import_message ().d());

  // enums without defaults are set to the first value in the enum.
  expect_eq(unittest::testalltypeslite::foo , message.optional_nested_enum ());
  expect_eq(unittest::foreign_lite_foo      , message.optional_foreign_enum());
  expect_eq(unittest_import::import_lite_foo, message.optional_import_enum ());


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
  expect_eq(true   , message.default_bool    ());
  expect_eq("hello", message.default_string  ());
  expect_eq("world", message.default_bytes   ());

  expect_eq(unittest::testalltypeslite::bar , message.default_nested_enum ());
  expect_eq(unittest::foreign_lite_bar      , message.default_foreign_enum());
  expect_eq(unittest_import::import_lite_bar, message.default_import_enum ());

}

// -------------------------------------------------------------------

void testutillite::expectrepeatedfieldsmodified(
    const unittest::testalltypeslite& message) {
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
  expect_eq(true , message.repeated_bool    (0));
  expect_eq("215", message.repeated_string  (0));
  expect_eq("216", message.repeated_bytes   (0));

  expect_eq(217, message.repeatedgroup           (0).a());
  expect_eq(218, message.repeated_nested_message (0).bb());
  expect_eq(219, message.repeated_foreign_message(0).c());
  expect_eq(220, message.repeated_import_message (0).d());
  expect_eq(227, message.repeated_lazy_message   (0).bb());

  expect_eq(unittest::testalltypeslite::bar , message.repeated_nested_enum (0));
  expect_eq(unittest::foreign_lite_bar      , message.repeated_foreign_enum(0));
  expect_eq(unittest_import::import_lite_bar, message.repeated_import_enum (0));


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
  expect_eq(true , message.repeated_bool    (1));
  expect_eq("515", message.repeated_string  (1));
  expect_eq("516", message.repeated_bytes   (1));

  expect_eq(517, message.repeatedgroup           (1).a());
  expect_eq(518, message.repeated_nested_message (1).bb());
  expect_eq(519, message.repeated_foreign_message(1).c());
  expect_eq(520, message.repeated_import_message (1).d());
  expect_eq(527, message.repeated_lazy_message   (1).bb());

  expect_eq(unittest::testalltypeslite::foo , message.repeated_nested_enum (1));
  expect_eq(unittest::foreign_lite_foo      , message.repeated_foreign_enum(1));
  expect_eq(unittest_import::import_lite_foo, message.repeated_import_enum (1));

}

// -------------------------------------------------------------------

void testutillite::setpackedfields(unittest::testpackedtypeslite* message) {
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
  message->add_packed_enum    (unittest::foreign_lite_bar);
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
  message->add_packed_enum    (unittest::foreign_lite_baz);
}

// -------------------------------------------------------------------

void testutillite::modifypackedfields(unittest::testpackedtypeslite* message) {
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
  message->set_packed_enum    (1, unittest::foreign_lite_foo);
}

// -------------------------------------------------------------------

void testutillite::expectpackedfieldsset(
    const unittest::testpackedtypeslite& message) {
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
  expect_eq(true , message.packed_bool    (0));
  expect_eq(unittest::foreign_lite_bar, message.packed_enum(0));

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
  expect_eq(false, message.packed_bool    (1));
  expect_eq(unittest::foreign_lite_baz, message.packed_enum(1));
}

// -------------------------------------------------------------------

void testutillite::expectpackedclear(
    const unittest::testpackedtypeslite& message) {
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

void testutillite::expectpackedfieldsmodified(
    const unittest::testpackedtypeslite& message) {
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
  expect_eq(true , message.packed_bool    (0));
  expect_eq(unittest::foreign_lite_bar, message.packed_enum(0));
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
  expect_eq(true , message.packed_bool    (1));
  expect_eq(unittest::foreign_lite_foo, message.packed_enum(1));
}

// ===================================================================
// extensions
//
// all this code is exactly equivalent to the above code except that it's
// manipulating extension fields instead of normal ones.
//
// i gave up on the 80-char limit here.  sorry.

void testutillite::setallextensions(unittest::testallextensionslite* message) {
  message->setextension(unittest::optional_int32_extension_lite   , 101);
  message->setextension(unittest::optional_int64_extension_lite   , 102);
  message->setextension(unittest::optional_uint32_extension_lite  , 103);
  message->setextension(unittest::optional_uint64_extension_lite  , 104);
  message->setextension(unittest::optional_sint32_extension_lite  , 105);
  message->setextension(unittest::optional_sint64_extension_lite  , 106);
  message->setextension(unittest::optional_fixed32_extension_lite , 107);
  message->setextension(unittest::optional_fixed64_extension_lite , 108);
  message->setextension(unittest::optional_sfixed32_extension_lite, 109);
  message->setextension(unittest::optional_sfixed64_extension_lite, 110);
  message->setextension(unittest::optional_float_extension_lite   , 111);
  message->setextension(unittest::optional_double_extension_lite  , 112);
  message->setextension(unittest::optional_bool_extension_lite    , true);
  message->setextension(unittest::optional_string_extension_lite  , "115");
  message->setextension(unittest::optional_bytes_extension_lite   , "116");

  message->mutableextension(unittest::optionalgroup_extension_lite                 )->set_a(117);
  message->mutableextension(unittest::optional_nested_message_extension_lite       )->set_bb(118);
  message->mutableextension(unittest::optional_foreign_message_extension_lite      )->set_c(119);
  message->mutableextension(unittest::optional_import_message_extension_lite       )->set_d(120);
  message->mutableextension(unittest::optional_public_import_message_extension_lite)->set_e(126);
  message->mutableextension(unittest::optional_lazy_message_extension_lite         )->set_bb(127);

  message->setextension(unittest::optional_nested_enum_extension_lite , unittest::testalltypeslite::baz );
  message->setextension(unittest::optional_foreign_enum_extension_lite, unittest::foreign_lite_baz      );
  message->setextension(unittest::optional_import_enum_extension_lite , unittest_import::import_lite_baz);


  // -----------------------------------------------------------------

  message->addextension(unittest::repeated_int32_extension_lite   , 201);
  message->addextension(unittest::repeated_int64_extension_lite   , 202);
  message->addextension(unittest::repeated_uint32_extension_lite  , 203);
  message->addextension(unittest::repeated_uint64_extension_lite  , 204);
  message->addextension(unittest::repeated_sint32_extension_lite  , 205);
  message->addextension(unittest::repeated_sint64_extension_lite  , 206);
  message->addextension(unittest::repeated_fixed32_extension_lite , 207);
  message->addextension(unittest::repeated_fixed64_extension_lite , 208);
  message->addextension(unittest::repeated_sfixed32_extension_lite, 209);
  message->addextension(unittest::repeated_sfixed64_extension_lite, 210);
  message->addextension(unittest::repeated_float_extension_lite   , 211);
  message->addextension(unittest::repeated_double_extension_lite  , 212);
  message->addextension(unittest::repeated_bool_extension_lite    , true);
  message->addextension(unittest::repeated_string_extension_lite  , "215");
  message->addextension(unittest::repeated_bytes_extension_lite   , "216");

  message->addextension(unittest::repeatedgroup_extension_lite           )->set_a(217);
  message->addextension(unittest::repeated_nested_message_extension_lite )->set_bb(218);
  message->addextension(unittest::repeated_foreign_message_extension_lite)->set_c(219);
  message->addextension(unittest::repeated_import_message_extension_lite )->set_d(220);
  message->addextension(unittest::repeated_lazy_message_extension_lite   )->set_bb(227);

  message->addextension(unittest::repeated_nested_enum_extension_lite , unittest::testalltypeslite::bar );
  message->addextension(unittest::repeated_foreign_enum_extension_lite, unittest::foreign_lite_bar      );
  message->addextension(unittest::repeated_import_enum_extension_lite , unittest_import::import_lite_bar);


  // add a second one of each field.
  message->addextension(unittest::repeated_int32_extension_lite   , 301);
  message->addextension(unittest::repeated_int64_extension_lite   , 302);
  message->addextension(unittest::repeated_uint32_extension_lite  , 303);
  message->addextension(unittest::repeated_uint64_extension_lite  , 304);
  message->addextension(unittest::repeated_sint32_extension_lite  , 305);
  message->addextension(unittest::repeated_sint64_extension_lite  , 306);
  message->addextension(unittest::repeated_fixed32_extension_lite , 307);
  message->addextension(unittest::repeated_fixed64_extension_lite , 308);
  message->addextension(unittest::repeated_sfixed32_extension_lite, 309);
  message->addextension(unittest::repeated_sfixed64_extension_lite, 310);
  message->addextension(unittest::repeated_float_extension_lite   , 311);
  message->addextension(unittest::repeated_double_extension_lite  , 312);
  message->addextension(unittest::repeated_bool_extension_lite    , false);
  message->addextension(unittest::repeated_string_extension_lite  , "315");
  message->addextension(unittest::repeated_bytes_extension_lite   , "316");

  message->addextension(unittest::repeatedgroup_extension_lite           )->set_a(317);
  message->addextension(unittest::repeated_nested_message_extension_lite )->set_bb(318);
  message->addextension(unittest::repeated_foreign_message_extension_lite)->set_c(319);
  message->addextension(unittest::repeated_import_message_extension_lite )->set_d(320);
  message->addextension(unittest::repeated_lazy_message_extension_lite   )->set_bb(327);

  message->addextension(unittest::repeated_nested_enum_extension_lite , unittest::testalltypeslite::baz );
  message->addextension(unittest::repeated_foreign_enum_extension_lite, unittest::foreign_lite_baz      );
  message->addextension(unittest::repeated_import_enum_extension_lite , unittest_import::import_lite_baz);


  // -----------------------------------------------------------------

  message->setextension(unittest::default_int32_extension_lite   , 401);
  message->setextension(unittest::default_int64_extension_lite   , 402);
  message->setextension(unittest::default_uint32_extension_lite  , 403);
  message->setextension(unittest::default_uint64_extension_lite  , 404);
  message->setextension(unittest::default_sint32_extension_lite  , 405);
  message->setextension(unittest::default_sint64_extension_lite  , 406);
  message->setextension(unittest::default_fixed32_extension_lite , 407);
  message->setextension(unittest::default_fixed64_extension_lite , 408);
  message->setextension(unittest::default_sfixed32_extension_lite, 409);
  message->setextension(unittest::default_sfixed64_extension_lite, 410);
  message->setextension(unittest::default_float_extension_lite   , 411);
  message->setextension(unittest::default_double_extension_lite  , 412);
  message->setextension(unittest::default_bool_extension_lite    , false);
  message->setextension(unittest::default_string_extension_lite  , "415");
  message->setextension(unittest::default_bytes_extension_lite   , "416");

  message->setextension(unittest::default_nested_enum_extension_lite , unittest::testalltypeslite::foo );
  message->setextension(unittest::default_foreign_enum_extension_lite, unittest::foreign_lite_foo      );
  message->setextension(unittest::default_import_enum_extension_lite , unittest_import::import_lite_foo);

}

// -------------------------------------------------------------------

void testutillite::modifyrepeatedextensions(
    unittest::testallextensionslite* message) {
  message->setextension(unittest::repeated_int32_extension_lite   , 1, 501);
  message->setextension(unittest::repeated_int64_extension_lite   , 1, 502);
  message->setextension(unittest::repeated_uint32_extension_lite  , 1, 503);
  message->setextension(unittest::repeated_uint64_extension_lite  , 1, 504);
  message->setextension(unittest::repeated_sint32_extension_lite  , 1, 505);
  message->setextension(unittest::repeated_sint64_extension_lite  , 1, 506);
  message->setextension(unittest::repeated_fixed32_extension_lite , 1, 507);
  message->setextension(unittest::repeated_fixed64_extension_lite , 1, 508);
  message->setextension(unittest::repeated_sfixed32_extension_lite, 1, 509);
  message->setextension(unittest::repeated_sfixed64_extension_lite, 1, 510);
  message->setextension(unittest::repeated_float_extension_lite   , 1, 511);
  message->setextension(unittest::repeated_double_extension_lite  , 1, 512);
  message->setextension(unittest::repeated_bool_extension_lite    , 1, true);
  message->setextension(unittest::repeated_string_extension_lite  , 1, "515");
  message->setextension(unittest::repeated_bytes_extension_lite   , 1, "516");

  message->mutableextension(unittest::repeatedgroup_extension_lite           , 1)->set_a(517);
  message->mutableextension(unittest::repeated_nested_message_extension_lite , 1)->set_bb(518);
  message->mutableextension(unittest::repeated_foreign_message_extension_lite, 1)->set_c(519);
  message->mutableextension(unittest::repeated_import_message_extension_lite , 1)->set_d(520);
  message->mutableextension(unittest::repeated_lazy_message_extension_lite   , 1)->set_bb(527);

  message->setextension(unittest::repeated_nested_enum_extension_lite , 1, unittest::testalltypeslite::foo );
  message->setextension(unittest::repeated_foreign_enum_extension_lite, 1, unittest::foreign_lite_foo      );
  message->setextension(unittest::repeated_import_enum_extension_lite , 1, unittest_import::import_lite_foo);

}

// -------------------------------------------------------------------

void testutillite::expectallextensionsset(
    const unittest::testallextensionslite& message) {
  expect_true(message.hasextension(unittest::optional_int32_extension_lite   ));
  expect_true(message.hasextension(unittest::optional_int64_extension_lite   ));
  expect_true(message.hasextension(unittest::optional_uint32_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_uint64_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_sint32_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_sint64_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_fixed32_extension_lite ));
  expect_true(message.hasextension(unittest::optional_fixed64_extension_lite ));
  expect_true(message.hasextension(unittest::optional_sfixed32_extension_lite));
  expect_true(message.hasextension(unittest::optional_sfixed64_extension_lite));
  expect_true(message.hasextension(unittest::optional_float_extension_lite   ));
  expect_true(message.hasextension(unittest::optional_double_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_bool_extension_lite    ));
  expect_true(message.hasextension(unittest::optional_string_extension_lite  ));
  expect_true(message.hasextension(unittest::optional_bytes_extension_lite   ));

  expect_true(message.hasextension(unittest::optionalgroup_extension_lite                 ));
  expect_true(message.hasextension(unittest::optional_nested_message_extension_lite       ));
  expect_true(message.hasextension(unittest::optional_foreign_message_extension_lite      ));
  expect_true(message.hasextension(unittest::optional_import_message_extension_lite       ));
  expect_true(message.hasextension(unittest::optional_public_import_message_extension_lite));
  expect_true(message.hasextension(unittest::optional_lazy_message_extension_lite         ));

  expect_true(message.getextension(unittest::optionalgroup_extension_lite                 ).has_a());
  expect_true(message.getextension(unittest::optional_nested_message_extension_lite       ).has_bb());
  expect_true(message.getextension(unittest::optional_foreign_message_extension_lite      ).has_c());
  expect_true(message.getextension(unittest::optional_import_message_extension_lite       ).has_d());
  expect_true(message.getextension(unittest::optional_public_import_message_extension_lite).has_e());
  expect_true(message.getextension(unittest::optional_lazy_message_extension_lite         ).has_bb());

  expect_true(message.hasextension(unittest::optional_nested_enum_extension_lite ));
  expect_true(message.hasextension(unittest::optional_foreign_enum_extension_lite));
  expect_true(message.hasextension(unittest::optional_import_enum_extension_lite ));


  expect_eq(101  , message.getextension(unittest::optional_int32_extension_lite   ));
  expect_eq(102  , message.getextension(unittest::optional_int64_extension_lite   ));
  expect_eq(103  , message.getextension(unittest::optional_uint32_extension_lite  ));
  expect_eq(104  , message.getextension(unittest::optional_uint64_extension_lite  ));
  expect_eq(105  , message.getextension(unittest::optional_sint32_extension_lite  ));
  expect_eq(106  , message.getextension(unittest::optional_sint64_extension_lite  ));
  expect_eq(107  , message.getextension(unittest::optional_fixed32_extension_lite ));
  expect_eq(108  , message.getextension(unittest::optional_fixed64_extension_lite ));
  expect_eq(109  , message.getextension(unittest::optional_sfixed32_extension_lite));
  expect_eq(110  , message.getextension(unittest::optional_sfixed64_extension_lite));
  expect_eq(111  , message.getextension(unittest::optional_float_extension_lite   ));
  expect_eq(112  , message.getextension(unittest::optional_double_extension_lite  ));
  expect_eq(true , message.getextension(unittest::optional_bool_extension_lite    ));
  expect_eq("115", message.getextension(unittest::optional_string_extension_lite  ));
  expect_eq("116", message.getextension(unittest::optional_bytes_extension_lite   ));

  expect_eq(117, message.getextension(unittest::optionalgroup_extension_lite                 ).a());
  expect_eq(118, message.getextension(unittest::optional_nested_message_extension_lite       ).bb());
  expect_eq(119, message.getextension(unittest::optional_foreign_message_extension_lite      ).c());
  expect_eq(120, message.getextension(unittest::optional_import_message_extension_lite       ).d());
  expect_eq(126, message.getextension(unittest::optional_public_import_message_extension_lite).e());
  expect_eq(127, message.getextension(unittest::optional_lazy_message_extension_lite         ).bb());

  expect_eq(unittest::testalltypeslite::baz , message.getextension(unittest::optional_nested_enum_extension_lite ));
  expect_eq(unittest::foreign_lite_baz      , message.getextension(unittest::optional_foreign_enum_extension_lite));
  expect_eq(unittest_import::import_lite_baz, message.getextension(unittest::optional_import_enum_extension_lite ));


  // -----------------------------------------------------------------

  assert_eq(2, message.extensionsize(unittest::repeated_int32_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_int64_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed32_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed64_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed32_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed64_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_float_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_double_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bool_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::repeated_string_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bytes_extension_lite   ));

  assert_eq(2, message.extensionsize(unittest::repeatedgroup_extension_lite           ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_message_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_message_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_import_message_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_lazy_message_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_enum_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_enum_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_import_enum_extension_lite    ));


  expect_eq(201  , message.getextension(unittest::repeated_int32_extension_lite   , 0));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension_lite   , 0));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension_lite  , 0));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension_lite  , 0));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension_lite  , 0));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension_lite  , 0));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension_lite , 0));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension_lite , 0));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension_lite, 0));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension_lite, 0));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension_lite   , 0));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension_lite  , 0));
  expect_eq(true , message.getextension(unittest::repeated_bool_extension_lite    , 0));
  expect_eq("215", message.getextension(unittest::repeated_string_extension_lite  , 0));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension_lite   , 0));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension_lite           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension_lite , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension_lite, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension_lite , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension_lite   , 0).bb());

  expect_eq(unittest::testalltypeslite::bar , message.getextension(unittest::repeated_nested_enum_extension_lite , 0));
  expect_eq(unittest::foreign_lite_bar      , message.getextension(unittest::repeated_foreign_enum_extension_lite, 0));
  expect_eq(unittest_import::import_lite_bar, message.getextension(unittest::repeated_import_enum_extension_lite , 0));


  expect_eq(301  , message.getextension(unittest::repeated_int32_extension_lite   , 1));
  expect_eq(302  , message.getextension(unittest::repeated_int64_extension_lite   , 1));
  expect_eq(303  , message.getextension(unittest::repeated_uint32_extension_lite  , 1));
  expect_eq(304  , message.getextension(unittest::repeated_uint64_extension_lite  , 1));
  expect_eq(305  , message.getextension(unittest::repeated_sint32_extension_lite  , 1));
  expect_eq(306  , message.getextension(unittest::repeated_sint64_extension_lite  , 1));
  expect_eq(307  , message.getextension(unittest::repeated_fixed32_extension_lite , 1));
  expect_eq(308  , message.getextension(unittest::repeated_fixed64_extension_lite , 1));
  expect_eq(309  , message.getextension(unittest::repeated_sfixed32_extension_lite, 1));
  expect_eq(310  , message.getextension(unittest::repeated_sfixed64_extension_lite, 1));
  expect_eq(311  , message.getextension(unittest::repeated_float_extension_lite   , 1));
  expect_eq(312  , message.getextension(unittest::repeated_double_extension_lite  , 1));
  expect_eq(false, message.getextension(unittest::repeated_bool_extension_lite    , 1));
  expect_eq("315", message.getextension(unittest::repeated_string_extension_lite  , 1));
  expect_eq("316", message.getextension(unittest::repeated_bytes_extension_lite   , 1));

  expect_eq(317, message.getextension(unittest::repeatedgroup_extension_lite           , 1).a());
  expect_eq(318, message.getextension(unittest::repeated_nested_message_extension_lite , 1).bb());
  expect_eq(319, message.getextension(unittest::repeated_foreign_message_extension_lite, 1).c());
  expect_eq(320, message.getextension(unittest::repeated_import_message_extension_lite , 1).d());
  expect_eq(327, message.getextension(unittest::repeated_lazy_message_extension_lite   , 1).bb());

  expect_eq(unittest::testalltypeslite::baz , message.getextension(unittest::repeated_nested_enum_extension_lite , 1));
  expect_eq(unittest::foreign_lite_baz      , message.getextension(unittest::repeated_foreign_enum_extension_lite, 1));
  expect_eq(unittest_import::import_lite_baz, message.getextension(unittest::repeated_import_enum_extension_lite , 1));


  // -----------------------------------------------------------------

  expect_true(message.hasextension(unittest::default_int32_extension_lite   ));
  expect_true(message.hasextension(unittest::default_int64_extension_lite   ));
  expect_true(message.hasextension(unittest::default_uint32_extension_lite  ));
  expect_true(message.hasextension(unittest::default_uint64_extension_lite  ));
  expect_true(message.hasextension(unittest::default_sint32_extension_lite  ));
  expect_true(message.hasextension(unittest::default_sint64_extension_lite  ));
  expect_true(message.hasextension(unittest::default_fixed32_extension_lite ));
  expect_true(message.hasextension(unittest::default_fixed64_extension_lite ));
  expect_true(message.hasextension(unittest::default_sfixed32_extension_lite));
  expect_true(message.hasextension(unittest::default_sfixed64_extension_lite));
  expect_true(message.hasextension(unittest::default_float_extension_lite   ));
  expect_true(message.hasextension(unittest::default_double_extension_lite  ));
  expect_true(message.hasextension(unittest::default_bool_extension_lite    ));
  expect_true(message.hasextension(unittest::default_string_extension_lite  ));
  expect_true(message.hasextension(unittest::default_bytes_extension_lite   ));

  expect_true(message.hasextension(unittest::default_nested_enum_extension_lite ));
  expect_true(message.hasextension(unittest::default_foreign_enum_extension_lite));
  expect_true(message.hasextension(unittest::default_import_enum_extension_lite ));


  expect_eq(401  , message.getextension(unittest::default_int32_extension_lite   ));
  expect_eq(402  , message.getextension(unittest::default_int64_extension_lite   ));
  expect_eq(403  , message.getextension(unittest::default_uint32_extension_lite  ));
  expect_eq(404  , message.getextension(unittest::default_uint64_extension_lite  ));
  expect_eq(405  , message.getextension(unittest::default_sint32_extension_lite  ));
  expect_eq(406  , message.getextension(unittest::default_sint64_extension_lite  ));
  expect_eq(407  , message.getextension(unittest::default_fixed32_extension_lite ));
  expect_eq(408  , message.getextension(unittest::default_fixed64_extension_lite ));
  expect_eq(409  , message.getextension(unittest::default_sfixed32_extension_lite));
  expect_eq(410  , message.getextension(unittest::default_sfixed64_extension_lite));
  expect_eq(411  , message.getextension(unittest::default_float_extension_lite   ));
  expect_eq(412  , message.getextension(unittest::default_double_extension_lite  ));
  expect_eq(false, message.getextension(unittest::default_bool_extension_lite    ));
  expect_eq("415", message.getextension(unittest::default_string_extension_lite  ));
  expect_eq("416", message.getextension(unittest::default_bytes_extension_lite   ));

  expect_eq(unittest::testalltypeslite::foo , message.getextension(unittest::default_nested_enum_extension_lite ));
  expect_eq(unittest::foreign_lite_foo      , message.getextension(unittest::default_foreign_enum_extension_lite));
  expect_eq(unittest_import::import_lite_foo, message.getextension(unittest::default_import_enum_extension_lite ));

}

// -------------------------------------------------------------------

void testutillite::expectextensionsclear(
    const unittest::testallextensionslite& message) {
  string serialized;
  assert_true(message.serializetostring(&serialized));
  expect_eq("", serialized);
  expect_eq(0, message.bytesize());

  // has_blah() should initially be false for all optional fields.
  expect_false(message.hasextension(unittest::optional_int32_extension_lite   ));
  expect_false(message.hasextension(unittest::optional_int64_extension_lite   ));
  expect_false(message.hasextension(unittest::optional_uint32_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_uint64_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_sint32_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_sint64_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_fixed32_extension_lite ));
  expect_false(message.hasextension(unittest::optional_fixed64_extension_lite ));
  expect_false(message.hasextension(unittest::optional_sfixed32_extension_lite));
  expect_false(message.hasextension(unittest::optional_sfixed64_extension_lite));
  expect_false(message.hasextension(unittest::optional_float_extension_lite   ));
  expect_false(message.hasextension(unittest::optional_double_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_bool_extension_lite    ));
  expect_false(message.hasextension(unittest::optional_string_extension_lite  ));
  expect_false(message.hasextension(unittest::optional_bytes_extension_lite   ));

  expect_false(message.hasextension(unittest::optionalgroup_extension_lite                 ));
  expect_false(message.hasextension(unittest::optional_nested_message_extension_lite       ));
  expect_false(message.hasextension(unittest::optional_foreign_message_extension_lite      ));
  expect_false(message.hasextension(unittest::optional_import_message_extension_lite       ));
  expect_false(message.hasextension(unittest::optional_public_import_message_extension_lite));
  expect_false(message.hasextension(unittest::optional_lazy_message_extension_lite         ));

  expect_false(message.hasextension(unittest::optional_nested_enum_extension_lite ));
  expect_false(message.hasextension(unittest::optional_foreign_enum_extension_lite));
  expect_false(message.hasextension(unittest::optional_import_enum_extension_lite ));


  // optional fields without defaults are set to zero or something like it.
  expect_eq(0    , message.getextension(unittest::optional_int32_extension_lite   ));
  expect_eq(0    , message.getextension(unittest::optional_int64_extension_lite   ));
  expect_eq(0    , message.getextension(unittest::optional_uint32_extension_lite  ));
  expect_eq(0    , message.getextension(unittest::optional_uint64_extension_lite  ));
  expect_eq(0    , message.getextension(unittest::optional_sint32_extension_lite  ));
  expect_eq(0    , message.getextension(unittest::optional_sint64_extension_lite  ));
  expect_eq(0    , message.getextension(unittest::optional_fixed32_extension_lite ));
  expect_eq(0    , message.getextension(unittest::optional_fixed64_extension_lite ));
  expect_eq(0    , message.getextension(unittest::optional_sfixed32_extension_lite));
  expect_eq(0    , message.getextension(unittest::optional_sfixed64_extension_lite));
  expect_eq(0    , message.getextension(unittest::optional_float_extension_lite   ));
  expect_eq(0    , message.getextension(unittest::optional_double_extension_lite  ));
  expect_eq(false, message.getextension(unittest::optional_bool_extension_lite    ));
  expect_eq(""   , message.getextension(unittest::optional_string_extension_lite  ));
  expect_eq(""   , message.getextension(unittest::optional_bytes_extension_lite   ));

  // embedded messages should also be clear.
  expect_false(message.getextension(unittest::optionalgroup_extension_lite                 ).has_a());
  expect_false(message.getextension(unittest::optional_nested_message_extension_lite       ).has_bb());
  expect_false(message.getextension(unittest::optional_foreign_message_extension_lite      ).has_c());
  expect_false(message.getextension(unittest::optional_import_message_extension_lite       ).has_d());
  expect_false(message.getextension(unittest::optional_public_import_message_extension_lite).has_e());
  expect_false(message.getextension(unittest::optional_lazy_message_extension_lite         ).has_bb());

  expect_eq(0, message.getextension(unittest::optionalgroup_extension_lite                 ).a());
  expect_eq(0, message.getextension(unittest::optional_nested_message_extension_lite       ).bb());
  expect_eq(0, message.getextension(unittest::optional_foreign_message_extension_lite      ).c());
  expect_eq(0, message.getextension(unittest::optional_import_message_extension_lite       ).d());
  expect_eq(0, message.getextension(unittest::optional_public_import_message_extension_lite).e());
  expect_eq(0, message.getextension(unittest::optional_lazy_message_extension_lite         ).bb());

  // enums without defaults are set to the first value in the enum.
  expect_eq(unittest::testalltypeslite::foo , message.getextension(unittest::optional_nested_enum_extension_lite ));
  expect_eq(unittest::foreign_lite_foo      , message.getextension(unittest::optional_foreign_enum_extension_lite));
  expect_eq(unittest_import::import_lite_foo, message.getextension(unittest::optional_import_enum_extension_lite ));


  // repeated fields are empty.
  expect_eq(0, message.extensionsize(unittest::repeated_int32_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::repeated_int64_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::repeated_uint32_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_uint64_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_sint32_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_sint64_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_fixed32_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::repeated_fixed64_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::repeated_sfixed32_extension_lite));
  expect_eq(0, message.extensionsize(unittest::repeated_sfixed64_extension_lite));
  expect_eq(0, message.extensionsize(unittest::repeated_float_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::repeated_double_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_bool_extension_lite    ));
  expect_eq(0, message.extensionsize(unittest::repeated_string_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::repeated_bytes_extension_lite   ));

  expect_eq(0, message.extensionsize(unittest::repeatedgroup_extension_lite           ));
  expect_eq(0, message.extensionsize(unittest::repeated_nested_message_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::repeated_foreign_message_extension_lite));
  expect_eq(0, message.extensionsize(unittest::repeated_import_message_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::repeated_lazy_message_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::repeated_nested_enum_extension_lite    ));
  expect_eq(0, message.extensionsize(unittest::repeated_foreign_enum_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::repeated_import_enum_extension_lite    ));


  // has_blah() should also be false for all default fields.
  expect_false(message.hasextension(unittest::default_int32_extension_lite   ));
  expect_false(message.hasextension(unittest::default_int64_extension_lite   ));
  expect_false(message.hasextension(unittest::default_uint32_extension_lite  ));
  expect_false(message.hasextension(unittest::default_uint64_extension_lite  ));
  expect_false(message.hasextension(unittest::default_sint32_extension_lite  ));
  expect_false(message.hasextension(unittest::default_sint64_extension_lite  ));
  expect_false(message.hasextension(unittest::default_fixed32_extension_lite ));
  expect_false(message.hasextension(unittest::default_fixed64_extension_lite ));
  expect_false(message.hasextension(unittest::default_sfixed32_extension_lite));
  expect_false(message.hasextension(unittest::default_sfixed64_extension_lite));
  expect_false(message.hasextension(unittest::default_float_extension_lite   ));
  expect_false(message.hasextension(unittest::default_double_extension_lite  ));
  expect_false(message.hasextension(unittest::default_bool_extension_lite    ));
  expect_false(message.hasextension(unittest::default_string_extension_lite  ));
  expect_false(message.hasextension(unittest::default_bytes_extension_lite   ));

  expect_false(message.hasextension(unittest::default_nested_enum_extension_lite ));
  expect_false(message.hasextension(unittest::default_foreign_enum_extension_lite));
  expect_false(message.hasextension(unittest::default_import_enum_extension_lite ));


  // fields with defaults have their default values (duh).
  expect_eq( 41    , message.getextension(unittest::default_int32_extension_lite   ));
  expect_eq( 42    , message.getextension(unittest::default_int64_extension_lite   ));
  expect_eq( 43    , message.getextension(unittest::default_uint32_extension_lite  ));
  expect_eq( 44    , message.getextension(unittest::default_uint64_extension_lite  ));
  expect_eq(-45    , message.getextension(unittest::default_sint32_extension_lite  ));
  expect_eq( 46    , message.getextension(unittest::default_sint64_extension_lite  ));
  expect_eq( 47    , message.getextension(unittest::default_fixed32_extension_lite ));
  expect_eq( 48    , message.getextension(unittest::default_fixed64_extension_lite ));
  expect_eq( 49    , message.getextension(unittest::default_sfixed32_extension_lite));
  expect_eq(-50    , message.getextension(unittest::default_sfixed64_extension_lite));
  expect_eq( 51.5  , message.getextension(unittest::default_float_extension_lite   ));
  expect_eq( 52e3  , message.getextension(unittest::default_double_extension_lite  ));
  expect_eq(true   , message.getextension(unittest::default_bool_extension_lite    ));
  expect_eq("hello", message.getextension(unittest::default_string_extension_lite  ));
  expect_eq("world", message.getextension(unittest::default_bytes_extension_lite   ));

  expect_eq(unittest::testalltypeslite::bar , message.getextension(unittest::default_nested_enum_extension_lite ));
  expect_eq(unittest::foreign_lite_bar      , message.getextension(unittest::default_foreign_enum_extension_lite));
  expect_eq(unittest_import::import_lite_bar, message.getextension(unittest::default_import_enum_extension_lite ));

}

// -------------------------------------------------------------------

void testutillite::expectrepeatedextensionsmodified(
    const unittest::testallextensionslite& message) {
  // modifyrepeatedfields only sets the second repeated element of each
  // field.  in addition to verifying this, we also verify that the first
  // element and size were *not* modified.
  assert_eq(2, message.extensionsize(unittest::repeated_int32_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_int64_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_uint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_sint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed32_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_fixed64_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed32_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_sfixed64_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_float_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_double_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bool_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::repeated_string_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::repeated_bytes_extension_lite   ));

  assert_eq(2, message.extensionsize(unittest::repeatedgroup_extension_lite           ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_message_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_message_extension_lite));
  assert_eq(2, message.extensionsize(unittest::repeated_import_message_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::repeated_lazy_message_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_nested_enum_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::repeated_foreign_enum_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::repeated_import_enum_extension_lite    ));


  expect_eq(201  , message.getextension(unittest::repeated_int32_extension_lite   , 0));
  expect_eq(202  , message.getextension(unittest::repeated_int64_extension_lite   , 0));
  expect_eq(203  , message.getextension(unittest::repeated_uint32_extension_lite  , 0));
  expect_eq(204  , message.getextension(unittest::repeated_uint64_extension_lite  , 0));
  expect_eq(205  , message.getextension(unittest::repeated_sint32_extension_lite  , 0));
  expect_eq(206  , message.getextension(unittest::repeated_sint64_extension_lite  , 0));
  expect_eq(207  , message.getextension(unittest::repeated_fixed32_extension_lite , 0));
  expect_eq(208  , message.getextension(unittest::repeated_fixed64_extension_lite , 0));
  expect_eq(209  , message.getextension(unittest::repeated_sfixed32_extension_lite, 0));
  expect_eq(210  , message.getextension(unittest::repeated_sfixed64_extension_lite, 0));
  expect_eq(211  , message.getextension(unittest::repeated_float_extension_lite   , 0));
  expect_eq(212  , message.getextension(unittest::repeated_double_extension_lite  , 0));
  expect_eq(true , message.getextension(unittest::repeated_bool_extension_lite    , 0));
  expect_eq("215", message.getextension(unittest::repeated_string_extension_lite  , 0));
  expect_eq("216", message.getextension(unittest::repeated_bytes_extension_lite   , 0));

  expect_eq(217, message.getextension(unittest::repeatedgroup_extension_lite           , 0).a());
  expect_eq(218, message.getextension(unittest::repeated_nested_message_extension_lite , 0).bb());
  expect_eq(219, message.getextension(unittest::repeated_foreign_message_extension_lite, 0).c());
  expect_eq(220, message.getextension(unittest::repeated_import_message_extension_lite , 0).d());
  expect_eq(227, message.getextension(unittest::repeated_lazy_message_extension_lite   , 0).bb());

  expect_eq(unittest::testalltypeslite::bar , message.getextension(unittest::repeated_nested_enum_extension_lite , 0));
  expect_eq(unittest::foreign_lite_bar      , message.getextension(unittest::repeated_foreign_enum_extension_lite, 0));
  expect_eq(unittest_import::import_lite_bar, message.getextension(unittest::repeated_import_enum_extension_lite , 0));


  // actually verify the second (modified) elements now.
  expect_eq(501  , message.getextension(unittest::repeated_int32_extension_lite   , 1));
  expect_eq(502  , message.getextension(unittest::repeated_int64_extension_lite   , 1));
  expect_eq(503  , message.getextension(unittest::repeated_uint32_extension_lite  , 1));
  expect_eq(504  , message.getextension(unittest::repeated_uint64_extension_lite  , 1));
  expect_eq(505  , message.getextension(unittest::repeated_sint32_extension_lite  , 1));
  expect_eq(506  , message.getextension(unittest::repeated_sint64_extension_lite  , 1));
  expect_eq(507  , message.getextension(unittest::repeated_fixed32_extension_lite , 1));
  expect_eq(508  , message.getextension(unittest::repeated_fixed64_extension_lite , 1));
  expect_eq(509  , message.getextension(unittest::repeated_sfixed32_extension_lite, 1));
  expect_eq(510  , message.getextension(unittest::repeated_sfixed64_extension_lite, 1));
  expect_eq(511  , message.getextension(unittest::repeated_float_extension_lite   , 1));
  expect_eq(512  , message.getextension(unittest::repeated_double_extension_lite  , 1));
  expect_eq(true , message.getextension(unittest::repeated_bool_extension_lite    , 1));
  expect_eq("515", message.getextension(unittest::repeated_string_extension_lite  , 1));
  expect_eq("516", message.getextension(unittest::repeated_bytes_extension_lite   , 1));

  expect_eq(517, message.getextension(unittest::repeatedgroup_extension_lite           , 1).a());
  expect_eq(518, message.getextension(unittest::repeated_nested_message_extension_lite , 1).bb());
  expect_eq(519, message.getextension(unittest::repeated_foreign_message_extension_lite, 1).c());
  expect_eq(520, message.getextension(unittest::repeated_import_message_extension_lite , 1).d());
  expect_eq(527, message.getextension(unittest::repeated_lazy_message_extension_lite   , 1).bb());

  expect_eq(unittest::testalltypeslite::foo , message.getextension(unittest::repeated_nested_enum_extension_lite , 1));
  expect_eq(unittest::foreign_lite_foo      , message.getextension(unittest::repeated_foreign_enum_extension_lite, 1));
  expect_eq(unittest_import::import_lite_foo, message.getextension(unittest::repeated_import_enum_extension_lite , 1));

}

// -------------------------------------------------------------------

void testutillite::setpackedextensions(
    unittest::testpackedextensionslite* message) {
  message->addextension(unittest::packed_int32_extension_lite   , 601);
  message->addextension(unittest::packed_int64_extension_lite   , 602);
  message->addextension(unittest::packed_uint32_extension_lite  , 603);
  message->addextension(unittest::packed_uint64_extension_lite  , 604);
  message->addextension(unittest::packed_sint32_extension_lite  , 605);
  message->addextension(unittest::packed_sint64_extension_lite  , 606);
  message->addextension(unittest::packed_fixed32_extension_lite , 607);
  message->addextension(unittest::packed_fixed64_extension_lite , 608);
  message->addextension(unittest::packed_sfixed32_extension_lite, 609);
  message->addextension(unittest::packed_sfixed64_extension_lite, 610);
  message->addextension(unittest::packed_float_extension_lite   , 611);
  message->addextension(unittest::packed_double_extension_lite  , 612);
  message->addextension(unittest::packed_bool_extension_lite    , true);
  message->addextension(unittest::packed_enum_extension_lite, unittest::foreign_lite_bar);
  // add a second one of each field
  message->addextension(unittest::packed_int32_extension_lite   , 701);
  message->addextension(unittest::packed_int64_extension_lite   , 702);
  message->addextension(unittest::packed_uint32_extension_lite  , 703);
  message->addextension(unittest::packed_uint64_extension_lite  , 704);
  message->addextension(unittest::packed_sint32_extension_lite  , 705);
  message->addextension(unittest::packed_sint64_extension_lite  , 706);
  message->addextension(unittest::packed_fixed32_extension_lite , 707);
  message->addextension(unittest::packed_fixed64_extension_lite , 708);
  message->addextension(unittest::packed_sfixed32_extension_lite, 709);
  message->addextension(unittest::packed_sfixed64_extension_lite, 710);
  message->addextension(unittest::packed_float_extension_lite   , 711);
  message->addextension(unittest::packed_double_extension_lite  , 712);
  message->addextension(unittest::packed_bool_extension_lite    , false);
  message->addextension(unittest::packed_enum_extension_lite, unittest::foreign_lite_baz);
}

// -------------------------------------------------------------------

void testutillite::modifypackedextensions(
    unittest::testpackedextensionslite* message) {
  message->setextension(unittest::packed_int32_extension_lite   , 1, 801);
  message->setextension(unittest::packed_int64_extension_lite   , 1, 802);
  message->setextension(unittest::packed_uint32_extension_lite  , 1, 803);
  message->setextension(unittest::packed_uint64_extension_lite  , 1, 804);
  message->setextension(unittest::packed_sint32_extension_lite  , 1, 805);
  message->setextension(unittest::packed_sint64_extension_lite  , 1, 806);
  message->setextension(unittest::packed_fixed32_extension_lite , 1, 807);
  message->setextension(unittest::packed_fixed64_extension_lite , 1, 808);
  message->setextension(unittest::packed_sfixed32_extension_lite, 1, 809);
  message->setextension(unittest::packed_sfixed64_extension_lite, 1, 810);
  message->setextension(unittest::packed_float_extension_lite   , 1, 811);
  message->setextension(unittest::packed_double_extension_lite  , 1, 812);
  message->setextension(unittest::packed_bool_extension_lite    , 1, true);
  message->setextension(unittest::packed_enum_extension_lite    , 1,
                        unittest::foreign_lite_foo);
}

// -------------------------------------------------------------------

void testutillite::expectpackedextensionsset(
    const unittest::testpackedextensionslite& message) {
  assert_eq(2, message.extensionsize(unittest::packed_int32_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_int64_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_uint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_uint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed32_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed64_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed32_extension_lite));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed64_extension_lite));
  assert_eq(2, message.extensionsize(unittest::packed_float_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_double_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_bool_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::packed_enum_extension_lite    ));

  expect_eq(601  , message.getextension(unittest::packed_int32_extension_lite   , 0));
  expect_eq(602  , message.getextension(unittest::packed_int64_extension_lite   , 0));
  expect_eq(603  , message.getextension(unittest::packed_uint32_extension_lite  , 0));
  expect_eq(604  , message.getextension(unittest::packed_uint64_extension_lite  , 0));
  expect_eq(605  , message.getextension(unittest::packed_sint32_extension_lite  , 0));
  expect_eq(606  , message.getextension(unittest::packed_sint64_extension_lite  , 0));
  expect_eq(607  , message.getextension(unittest::packed_fixed32_extension_lite , 0));
  expect_eq(608  , message.getextension(unittest::packed_fixed64_extension_lite , 0));
  expect_eq(609  , message.getextension(unittest::packed_sfixed32_extension_lite, 0));
  expect_eq(610  , message.getextension(unittest::packed_sfixed64_extension_lite, 0));
  expect_eq(611  , message.getextension(unittest::packed_float_extension_lite   , 0));
  expect_eq(612  , message.getextension(unittest::packed_double_extension_lite  , 0));
  expect_eq(true , message.getextension(unittest::packed_bool_extension_lite    , 0));
  expect_eq(unittest::foreign_lite_bar,
            message.getextension(unittest::packed_enum_extension_lite, 0));
  expect_eq(701  , message.getextension(unittest::packed_int32_extension_lite   , 1));
  expect_eq(702  , message.getextension(unittest::packed_int64_extension_lite   , 1));
  expect_eq(703  , message.getextension(unittest::packed_uint32_extension_lite  , 1));
  expect_eq(704  , message.getextension(unittest::packed_uint64_extension_lite  , 1));
  expect_eq(705  , message.getextension(unittest::packed_sint32_extension_lite  , 1));
  expect_eq(706  , message.getextension(unittest::packed_sint64_extension_lite  , 1));
  expect_eq(707  , message.getextension(unittest::packed_fixed32_extension_lite , 1));
  expect_eq(708  , message.getextension(unittest::packed_fixed64_extension_lite , 1));
  expect_eq(709  , message.getextension(unittest::packed_sfixed32_extension_lite, 1));
  expect_eq(710  , message.getextension(unittest::packed_sfixed64_extension_lite, 1));
  expect_eq(711  , message.getextension(unittest::packed_float_extension_lite   , 1));
  expect_eq(712  , message.getextension(unittest::packed_double_extension_lite  , 1));
  expect_eq(false, message.getextension(unittest::packed_bool_extension_lite    , 1));
  expect_eq(unittest::foreign_lite_baz,
            message.getextension(unittest::packed_enum_extension_lite, 1));
}

// -------------------------------------------------------------------

void testutillite::expectpackedextensionsclear(
    const unittest::testpackedextensionslite& message) {
  expect_eq(0, message.extensionsize(unittest::packed_int32_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::packed_int64_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::packed_uint32_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::packed_uint64_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::packed_sint32_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::packed_sint64_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::packed_fixed32_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::packed_fixed64_extension_lite ));
  expect_eq(0, message.extensionsize(unittest::packed_sfixed32_extension_lite));
  expect_eq(0, message.extensionsize(unittest::packed_sfixed64_extension_lite));
  expect_eq(0, message.extensionsize(unittest::packed_float_extension_lite   ));
  expect_eq(0, message.extensionsize(unittest::packed_double_extension_lite  ));
  expect_eq(0, message.extensionsize(unittest::packed_bool_extension_lite    ));
  expect_eq(0, message.extensionsize(unittest::packed_enum_extension_lite    ));
}

// -------------------------------------------------------------------

void testutillite::expectpackedextensionsmodified(
    const unittest::testpackedextensionslite& message) {
  assert_eq(2, message.extensionsize(unittest::packed_int32_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_int64_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_uint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_uint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint32_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_sint64_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed32_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::packed_fixed64_extension_lite ));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed32_extension_lite));
  assert_eq(2, message.extensionsize(unittest::packed_sfixed64_extension_lite));
  assert_eq(2, message.extensionsize(unittest::packed_float_extension_lite   ));
  assert_eq(2, message.extensionsize(unittest::packed_double_extension_lite  ));
  assert_eq(2, message.extensionsize(unittest::packed_bool_extension_lite    ));
  assert_eq(2, message.extensionsize(unittest::packed_enum_extension_lite    ));
  expect_eq(601  , message.getextension(unittest::packed_int32_extension_lite   , 0));
  expect_eq(602  , message.getextension(unittest::packed_int64_extension_lite   , 0));
  expect_eq(603  , message.getextension(unittest::packed_uint32_extension_lite  , 0));
  expect_eq(604  , message.getextension(unittest::packed_uint64_extension_lite  , 0));
  expect_eq(605  , message.getextension(unittest::packed_sint32_extension_lite  , 0));
  expect_eq(606  , message.getextension(unittest::packed_sint64_extension_lite  , 0));
  expect_eq(607  , message.getextension(unittest::packed_fixed32_extension_lite , 0));
  expect_eq(608  , message.getextension(unittest::packed_fixed64_extension_lite , 0));
  expect_eq(609  , message.getextension(unittest::packed_sfixed32_extension_lite, 0));
  expect_eq(610  , message.getextension(unittest::packed_sfixed64_extension_lite, 0));
  expect_eq(611  , message.getextension(unittest::packed_float_extension_lite   , 0));
  expect_eq(612  , message.getextension(unittest::packed_double_extension_lite  , 0));
  expect_eq(true , message.getextension(unittest::packed_bool_extension_lite    , 0));
  expect_eq(unittest::foreign_lite_bar,
            message.getextension(unittest::packed_enum_extension_lite, 0));

  // actually verify the second (modified) elements now.
  expect_eq(801  , message.getextension(unittest::packed_int32_extension_lite   , 1));
  expect_eq(802  , message.getextension(unittest::packed_int64_extension_lite   , 1));
  expect_eq(803  , message.getextension(unittest::packed_uint32_extension_lite  , 1));
  expect_eq(804  , message.getextension(unittest::packed_uint64_extension_lite  , 1));
  expect_eq(805  , message.getextension(unittest::packed_sint32_extension_lite  , 1));
  expect_eq(806  , message.getextension(unittest::packed_sint64_extension_lite  , 1));
  expect_eq(807  , message.getextension(unittest::packed_fixed32_extension_lite , 1));
  expect_eq(808  , message.getextension(unittest::packed_fixed64_extension_lite , 1));
  expect_eq(809  , message.getextension(unittest::packed_sfixed32_extension_lite, 1));
  expect_eq(810  , message.getextension(unittest::packed_sfixed64_extension_lite, 1));
  expect_eq(811  , message.getextension(unittest::packed_float_extension_lite   , 1));
  expect_eq(812  , message.getextension(unittest::packed_double_extension_lite  , 1));
  expect_eq(true , message.getextension(unittest::packed_bool_extension_lite    , 1));
  expect_eq(unittest::foreign_lite_foo,
            message.getextension(unittest::packed_enum_extension_lite, 1));
}

}  // namespace protobuf
}  // namespace google
