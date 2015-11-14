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

// author: jschorr@google.com (joseph schorr)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <math.h>
#include <stdlib.h>
#include <limits>

#include <google/protobuf/text_format.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_mset.pb.h>
#include <google/protobuf/test_util.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

namespace google {
namespace protobuf {

// can't use an anonymous namespace here due to brokenness of tru64 compiler.
namespace text_format_unittest {

inline bool isnan(double value) {
  // nan is never equal to anything, even itself.
  return value != value;
}

// a basic string with different escapable characters for testing.
const string kescapeteststring =
  "\"a string with ' characters \n and \r newlines and \t tabs and \001 "
  "slashes \\ and  multiple   spaces";

// a representation of the above string with all the characters escaped.
const string kescapeteststringescaped =
  "\"\\\"a string with \\' characters \\n and \\r newlines "
  "and \\t tabs and \\001 slashes \\\\ and  multiple   spaces\"";

class textformattest : public testing::test {
 public:
  static void setuptestcase() {
    file::readfiletostringordie(
        testsourcedir()
        + "/google/protobuf/testdata/text_format_unittest_data.txt",
        &static_proto_debug_string_);
  }

  textformattest() : proto_debug_string_(static_proto_debug_string_) {}

 protected:
  // debug string read from text_format_unittest_data.txt.
  const string proto_debug_string_;
  unittest::testalltypes proto_;

 private:
  static string static_proto_debug_string_;
};
string textformattest::static_proto_debug_string_;

class textformatextensionstest : public testing::test {
 public:
  static void setuptestcase() {
    file::readfiletostringordie(
        testsourcedir()
        + "/google/protobuf/testdata/"
          "text_format_unittest_extensions_data.txt",
        &static_proto_debug_string_);
  }

  textformatextensionstest()
      : proto_debug_string_(static_proto_debug_string_) {}

 protected:
  // debug string read from text_format_unittest_data.txt.
  const string proto_debug_string_;
  unittest::testallextensions proto_;

 private:
  static string static_proto_debug_string_;
};
string textformatextensionstest::static_proto_debug_string_;


test_f(textformattest, basic) {
  testutil::setallfields(&proto_);
  expect_eq(proto_debug_string_, proto_.debugstring());
}

test_f(textformatextensionstest, extensions) {
  testutil::setallextensions(&proto_);
  expect_eq(proto_debug_string_, proto_.debugstring());
}

test_f(textformattest, shortdebugstring) {
  proto_.set_optional_int32(1);
  proto_.set_optional_string("hello");
  proto_.mutable_optional_nested_message()->set_bb(2);
  proto_.mutable_optional_foreign_message();

  expect_eq("optional_int32: 1 optional_string: \"hello\" "
            "optional_nested_message { bb: 2 } "
            "optional_foreign_message { }",
            proto_.shortdebugstring());
}

test_f(textformattest, shortprimitiverepeateds) {
  proto_.set_optional_int32(123);
  proto_.add_repeated_int32(456);
  proto_.add_repeated_int32(789);
  proto_.add_repeated_string("foo");
  proto_.add_repeated_string("bar");
  proto_.add_repeated_nested_message()->set_bb(2);
  proto_.add_repeated_nested_message()->set_bb(3);
  proto_.add_repeated_nested_enum(unittest::testalltypes::foo);
  proto_.add_repeated_nested_enum(unittest::testalltypes::bar);

  textformat::printer printer;
  printer.setuseshortrepeatedprimitives(true);
  string text;
  printer.printtostring(proto_, &text);

  expect_eq("optional_int32: 123\n"
            "repeated_int32: [456, 789]\n"
            "repeated_string: \"foo\"\n"
            "repeated_string: \"bar\"\n"
            "repeated_nested_message {\n  bb: 2\n}\n"
            "repeated_nested_message {\n  bb: 3\n}\n"
            "repeated_nested_enum: [foo, bar]\n",
            text);

  // try in single-line mode.
  printer.setsinglelinemode(true);
  printer.printtostring(proto_, &text);

  expect_eq("optional_int32: 123 "
            "repeated_int32: [456, 789] "
            "repeated_string: \"foo\" "
            "repeated_string: \"bar\" "
            "repeated_nested_message { bb: 2 } "
            "repeated_nested_message { bb: 3 } "
            "repeated_nested_enum: [foo, bar] ",
            text);
}


test_f(textformattest, stringescape) {
  // set the string value to test.
  proto_.set_optional_string(kescapeteststring);

  // get the debugstring from the proto.
  string debug_string = proto_.debugstring();
  string utf8_debug_string = proto_.utf8debugstring();

  // hardcode a correct value to test against.
  string correct_string = "optional_string: "
      + kescapeteststringescaped
       + "\n";

  // compare.
  expect_eq(correct_string, debug_string);
  // utf-8 string is the same as non-utf-8 because
  // the protocol buffer contains no utf-8 text.
  expect_eq(correct_string, utf8_debug_string);

  string expected_short_debug_string = "optional_string: "
      + kescapeteststringescaped;
  expect_eq(expected_short_debug_string, proto_.shortdebugstring());
}

test_f(textformattest, utf8debugstring) {
  // set the string value to test.
  proto_.set_optional_string("\350\260\267\346\255\214");

  // get the debugstring from the proto.
  string debug_string = proto_.debugstring();
  string utf8_debug_string = proto_.utf8debugstring();

  // hardcode a correct value to test against.
  string correct_utf8_string = "optional_string: "
      "\"\350\260\267\346\255\214\""
      "\n";
  string correct_string = "optional_string: "
      "\"\\350\\260\\267\\346\\255\\214\""
      "\n";

  // compare.
  expect_eq(correct_utf8_string, utf8_debug_string);
  expect_eq(correct_string, debug_string);
}

test_f(textformattest, printunknownfields) {
  // test printing of unknown fields in a message.

  unittest::testemptymessage message;
  unknownfieldset* unknown_fields = message.mutable_unknown_fields();

  unknown_fields->addvarint(5, 1);
  unknown_fields->addfixed32(5, 2);
  unknown_fields->addfixed64(5, 3);
  unknown_fields->addlengthdelimited(5, "4");
  unknown_fields->addgroup(5)->addvarint(10, 5);

  unknown_fields->addvarint(8, 1);
  unknown_fields->addvarint(8, 2);
  unknown_fields->addvarint(8, 3);

  expect_eq(
    "5: 1\n"
    "5: 0x00000002\n"
    "5: 0x0000000000000003\n"
    "5: \"4\"\n"
    "5 {\n"
    "  10: 5\n"
    "}\n"
    "8: 1\n"
    "8: 2\n"
    "8: 3\n",
    message.debugstring());
}

test_f(textformattest, printunknownmessage) {
  // test heuristic printing of messages in an unknownfieldset.

  protobuf_unittest::testalltypes message;

  // cases which should not be interpreted as sub-messages.

  // 'a' is a valid fixed64 tag, so for the string to be parseable as a message
  // it should be followed by 8 bytes.  since this string only has two
  // subsequent bytes, it should be treated as a string.
  message.add_repeated_string("abc");

  // 'd' happens to be a valid endgroup tag.  so,
  // unknownfieldset::mergefromcodedstream() will successfully parse "def", but
  // the consumedentiremessage() check should fail.
  message.add_repeated_string("def");

  // a zero-length string should never be interpreted as a message even though
  // it is technically valid as one.
  message.add_repeated_string("");

  // case which should be interpreted as a sub-message.

  // an actual nested message with content should always be interpreted as a
  // nested message.
  message.add_repeated_nested_message()->set_bb(123);

  string data;
  message.serializetostring(&data);

  string text;
  unknownfieldset unknown_fields;
  expect_true(unknown_fields.parsefromstring(data));
  expect_true(textformat::printunknownfieldstostring(unknown_fields, &text));
  expect_eq(
    "44: \"abc\"\n"
    "44: \"def\"\n"
    "44: \"\"\n"
    "48 {\n"
    "  1: 123\n"
    "}\n",
    text);
}

test_f(textformattest, printmessagewithindent) {
  // test adding an initial indent to printing.

  protobuf_unittest::testalltypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");
  message.add_repeated_nested_message()->set_bb(123);

  string text;
  textformat::printer printer;
  printer.setinitialindentlevel(1);
  expect_true(printer.printtostring(message, &text));
  expect_eq(
    "  repeated_string: \"abc\"\n"
    "  repeated_string: \"def\"\n"
    "  repeated_nested_message {\n"
    "    bb: 123\n"
    "  }\n",
    text);
}

test_f(textformattest, printmessagesingleline) {
  // test printing a message on a single line.

  protobuf_unittest::testalltypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");
  message.add_repeated_nested_message()->set_bb(123);

  string text;
  textformat::printer printer;
  printer.setinitialindentlevel(1);
  printer.setsinglelinemode(true);
  expect_true(printer.printtostring(message, &text));
  expect_eq(
    "  repeated_string: \"abc\" repeated_string: \"def\" "
    "repeated_nested_message { bb: 123 } ",
    text);
}

test_f(textformattest, printbuffertoosmall) {
  // test printing a message to a buffer that is too small.

  protobuf_unittest::testalltypes message;

  message.add_repeated_string("abc");
  message.add_repeated_string("def");

  char buffer[1] = "";
  io::arrayoutputstream output_stream(buffer, 1);
  expect_false(textformat::print(message, &output_stream));
  expect_eq(buffer[0], 'r');
  expect_eq(output_stream.bytecount(), 1);
}

test_f(textformattest, parsebasic) {
  io::arrayinputstream input_stream(proto_debug_string_.data(),
                                    proto_debug_string_.size());
  textformat::parse(&input_stream, &proto_);
  testutil::expectallfieldsset(proto_);
}

test_f(textformatextensionstest, parseextensions) {
  io::arrayinputstream input_stream(proto_debug_string_.data(),
                                    proto_debug_string_.size());
  textformat::parse(&input_stream, &proto_);
  testutil::expectallextensionsset(proto_);
}

test_f(textformattest, parseenumfieldfromnumber) {
  // create a parse string with a numerical value for an enum field.
  string parse_string = strings::substitute("optional_nested_enum: $0",
                                            unittest::testalltypes::baz);
  expect_true(textformat::parsefromstring(parse_string, &proto_));
  expect_true(proto_.has_optional_nested_enum());
  expect_eq(unittest::testalltypes::baz, proto_.optional_nested_enum());
}

test_f(textformattest, parseenumfieldfromnegativenumber) {
  assert_lt(unittest::sparse_e, 0);
  string parse_string = strings::substitute("sparse_enum: $0",
                                            unittest::sparse_e);
  unittest::sparseenummessage proto;
  expect_true(textformat::parsefromstring(parse_string, &proto));
  expect_true(proto.has_sparse_enum());
  expect_eq(unittest::sparse_e, proto.sparse_enum());
}

test_f(textformattest, parsestringescape) {
  // create a parse string with escpaed characters in it.
  string parse_string = "optional_string: "
      + kescapeteststringescaped
      + "\n";

  io::arrayinputstream input_stream(parse_string.data(),
                                    parse_string.size());
  textformat::parse(&input_stream, &proto_);

  // compare.
  expect_eq(kescapeteststring, proto_.optional_string());
}

test_f(textformattest, parseconcatenatedstring) {
  // create a parse string with multiple parts on one line.
  string parse_string = "optional_string: \"foo\" \"bar\"\n";

  io::arrayinputstream input_stream1(parse_string.data(),
                                    parse_string.size());
  textformat::parse(&input_stream1, &proto_);

  // compare.
  expect_eq("foobar", proto_.optional_string());

  // create a parse string with multiple parts on seperate lines.
  parse_string = "optional_string: \"foo\"\n"
                 "\"bar\"\n";

  io::arrayinputstream input_stream2(parse_string.data(),
                                    parse_string.size());
  textformat::parse(&input_stream2, &proto_);

  // compare.
  expect_eq("foobar", proto_.optional_string());
}

test_f(textformattest, parsefloatwithsuffix) {
  // test that we can parse a floating-point value with 'f' appended to the
  // end.  this is needed for backwards-compatibility with proto1.

  // have it parse a float with the 'f' suffix.
  string parse_string = "optional_float: 1.0f\n";

  io::arrayinputstream input_stream(parse_string.data(),
                                    parse_string.size());

  textformat::parse(&input_stream, &proto_);

  // compare.
  expect_eq(1.0, proto_.optional_float());
}

test_f(textformattest, parseshortrepeatedform) {
  string parse_string =
      // mixed short-form and long-form are simply concatenated.
      "repeated_int32: 1\n"
      "repeated_int32: [456, 789]\n"
      "repeated_nested_enum: [  foo ,bar, # comment\n"
      "                         3]\n"
      // note that while the printer won't print repeated strings in short-form,
      // the parser will accept them.
      "repeated_string: [ \"foo\", 'bar' ]\n";

  assert_true(textformat::parsefromstring(parse_string, &proto_));

  assert_eq(3, proto_.repeated_int32_size());
  expect_eq(1, proto_.repeated_int32(0));
  expect_eq(456, proto_.repeated_int32(1));
  expect_eq(789, proto_.repeated_int32(2));

  assert_eq(3, proto_.repeated_nested_enum_size());
  expect_eq(unittest::testalltypes::foo, proto_.repeated_nested_enum(0));
  expect_eq(unittest::testalltypes::bar, proto_.repeated_nested_enum(1));
  expect_eq(unittest::testalltypes::baz, proto_.repeated_nested_enum(2));

  assert_eq(2, proto_.repeated_string_size());
  expect_eq("foo", proto_.repeated_string(0));
  expect_eq("bar", proto_.repeated_string(1));
}

test_f(textformattest, comments) {
  // test that comments are ignored.

  string parse_string = "optional_int32: 1  # a comment\n"
                        "optional_int64: 2  # another comment";

  io::arrayinputstream input_stream(parse_string.data(),
                                    parse_string.size());

  textformat::parse(&input_stream, &proto_);

  // compare.
  expect_eq(1, proto_.optional_int32());
  expect_eq(2, proto_.optional_int64());
}

test_f(textformattest, optionalcolon) {
  // test that we can place a ':' after the field name of a nested message,
  // even though we don't have to.

  string parse_string = "optional_nested_message: { bb: 1}\n";

  io::arrayinputstream input_stream(parse_string.data(),
                                    parse_string.size());

  textformat::parse(&input_stream, &proto_);

  // compare.
  expect_true(proto_.has_optional_nested_message());
  expect_eq(1, proto_.optional_nested_message().bb());
}

// some platforms (e.g. windows) insist on padding the exponent to three
// digits when one or two would be just fine.
static string removeredundantzeros(string text) {
  text = stringreplace(text, "e+0", "e+", true);
  text = stringreplace(text, "e-0", "e-", true);
  return text;
}

test_f(textformattest, printexotic) {
  unittest::testalltypes message;

  // note:  in c, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and
  //   9223372036854775808 is outside the range of int64.  however, it is not
  //   outside the range of uint64.  confusingly, this means that everything
  //   works if we make the literal unsigned, even though we are negating it.
  message.add_repeated_int64(-google_ulonglong(9223372036854775808));
  message.add_repeated_uint64(google_ulonglong(18446744073709551615));
  message.add_repeated_double(123.456);
  message.add_repeated_double(1.23e21);
  message.add_repeated_double(1.23e-18);
  message.add_repeated_double(std::numeric_limits<double>::infinity());
  message.add_repeated_double(-std::numeric_limits<double>::infinity());
  message.add_repeated_double(std::numeric_limits<double>::quiet_nan());
  message.add_repeated_string(string("\000\001\a\b\f\n\r\t\v\\\'\"", 12));

  // fun story:  we used to use 1.23e22 instead of 1.23e21 above, but this
  //   seemed to trigger an odd case on mingw/gcc 3.4.5 where gcc's parsing of
  //   the value differed from strtod()'s parsing.  that is to say, the
  //   following assertion fails on mingw:
  //     assert(1.23e22 == strtod("1.23e22", null));
  //   as a result, simpledtoa() would print the value as
  //   "1.2300000000000001e+22" to make sure strtod() produce the exact same
  //   result.  our goal is to test runtime parsing, not compile-time parsing,
  //   so this wasn't our problem.  it was found that using 1.23e21 did not
  //   have this problem, so we switched to that instead.

  expect_eq(
    "repeated_int64: -9223372036854775808\n"
    "repeated_uint64: 18446744073709551615\n"
    "repeated_double: 123.456\n"
    "repeated_double: 1.23e+21\n"
    "repeated_double: 1.23e-18\n"
    "repeated_double: inf\n"
    "repeated_double: -inf\n"
    "repeated_double: nan\n"
    "repeated_string: \"\\000\\001\\007\\010\\014\\n\\r\\t\\013\\\\\\'\\\"\"\n",
    removeredundantzeros(message.debugstring()));
}

test_f(textformattest, printfloatprecision) {
  unittest::testalltypes message;

  message.add_repeated_float(1.2);
  message.add_repeated_float(1.23);
  message.add_repeated_float(1.234);
  message.add_repeated_float(1.2345);
  message.add_repeated_float(1.23456);
  message.add_repeated_float(1.2e10);
  message.add_repeated_float(1.23e10);
  message.add_repeated_float(1.234e10);
  message.add_repeated_float(1.2345e10);
  message.add_repeated_float(1.23456e10);
  message.add_repeated_double(1.2);
  message.add_repeated_double(1.23);
  message.add_repeated_double(1.234);
  message.add_repeated_double(1.2345);
  message.add_repeated_double(1.23456);
  message.add_repeated_double(1.234567);
  message.add_repeated_double(1.2345678);
  message.add_repeated_double(1.23456789);
  message.add_repeated_double(1.234567898);
  message.add_repeated_double(1.2345678987);
  message.add_repeated_double(1.23456789876);
  message.add_repeated_double(1.234567898765);
  message.add_repeated_double(1.2345678987654);
  message.add_repeated_double(1.23456789876543);
  message.add_repeated_double(1.2e100);
  message.add_repeated_double(1.23e100);
  message.add_repeated_double(1.234e100);
  message.add_repeated_double(1.2345e100);
  message.add_repeated_double(1.23456e100);
  message.add_repeated_double(1.234567e100);
  message.add_repeated_double(1.2345678e100);
  message.add_repeated_double(1.23456789e100);
  message.add_repeated_double(1.234567898e100);
  message.add_repeated_double(1.2345678987e100);
  message.add_repeated_double(1.23456789876e100);
  message.add_repeated_double(1.234567898765e100);
  message.add_repeated_double(1.2345678987654e100);
  message.add_repeated_double(1.23456789876543e100);

  expect_eq(
    "repeated_float: 1.2\n"
    "repeated_float: 1.23\n"
    "repeated_float: 1.234\n"
    "repeated_float: 1.2345\n"
    "repeated_float: 1.23456\n"
    "repeated_float: 1.2e+10\n"
    "repeated_float: 1.23e+10\n"
    "repeated_float: 1.234e+10\n"
    "repeated_float: 1.2345e+10\n"
    "repeated_float: 1.23456e+10\n"
    "repeated_double: 1.2\n"
    "repeated_double: 1.23\n"
    "repeated_double: 1.234\n"
    "repeated_double: 1.2345\n"
    "repeated_double: 1.23456\n"
    "repeated_double: 1.234567\n"
    "repeated_double: 1.2345678\n"
    "repeated_double: 1.23456789\n"
    "repeated_double: 1.234567898\n"
    "repeated_double: 1.2345678987\n"
    "repeated_double: 1.23456789876\n"
    "repeated_double: 1.234567898765\n"
    "repeated_double: 1.2345678987654\n"
    "repeated_double: 1.23456789876543\n"
    "repeated_double: 1.2e+100\n"
    "repeated_double: 1.23e+100\n"
    "repeated_double: 1.234e+100\n"
    "repeated_double: 1.2345e+100\n"
    "repeated_double: 1.23456e+100\n"
    "repeated_double: 1.234567e+100\n"
    "repeated_double: 1.2345678e+100\n"
    "repeated_double: 1.23456789e+100\n"
    "repeated_double: 1.234567898e+100\n"
    "repeated_double: 1.2345678987e+100\n"
    "repeated_double: 1.23456789876e+100\n"
    "repeated_double: 1.234567898765e+100\n"
    "repeated_double: 1.2345678987654e+100\n"
    "repeated_double: 1.23456789876543e+100\n",
    removeredundantzeros(message.debugstring()));
}


test_f(textformattest, allowpartial) {
  unittest::testrequired message;
  textformat::parser parser;
  parser.allowpartialmessage(true);
  expect_true(parser.parsefromstring("a: 1", &message));
  expect_eq(1, message.a());
  expect_false(message.has_b());
  expect_false(message.has_c());
}

test_f(textformattest, parseexotic) {
  unittest::testalltypes message;
  assert_true(textformat::parsefromstring(
    "repeated_int32: -1\n"
    "repeated_int32: -2147483648\n"
    "repeated_int64: -1\n"
    "repeated_int64: -9223372036854775808\n"
    "repeated_uint32: 4294967295\n"
    "repeated_uint32: 2147483648\n"
    "repeated_uint64: 18446744073709551615\n"
    "repeated_uint64: 9223372036854775808\n"
    "repeated_double: 123.0\n"
    "repeated_double: 123.5\n"
    "repeated_double: 0.125\n"
    "repeated_double: 1.23e17\n"
    "repeated_double: 1.235e+22\n"
    "repeated_double: 1.235e-18\n"
    "repeated_double: 123.456789\n"
    "repeated_double: inf\n"
    "repeated_double: infinity\n"
    "repeated_double: -inf\n"
    "repeated_double: -infinity\n"
    "repeated_double: nan\n"
    "repeated_double: nan\n"
    "repeated_string: \"\\000\\001\\a\\b\\f\\n\\r\\t\\v\\\\\\'\\\"\"\n",
    &message));

  assert_eq(2, message.repeated_int32_size());
  expect_eq(-1, message.repeated_int32(0));
  // note:  in c, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and 2147483648 is
  //   outside the range of int32.  however, it is not outside the range of
  //   uint32.  confusingly, this means that everything works if we make the
  //   literal unsigned, even though we are negating it.
  expect_eq(-2147483648u, message.repeated_int32(1));

  assert_eq(2, message.repeated_int64_size());
  expect_eq(-1, message.repeated_int64(0));
  // note:  in c, a negative integer literal is actually the unary negation
  //   operator being applied to a positive integer literal, and
  //   9223372036854775808 is outside the range of int64.  however, it is not
  //   outside the range of uint64.  confusingly, this means that everything
  //   works if we make the literal unsigned, even though we are negating it.
  expect_eq(-google_ulonglong(9223372036854775808), message.repeated_int64(1));

  assert_eq(2, message.repeated_uint32_size());
  expect_eq(4294967295u, message.repeated_uint32(0));
  expect_eq(2147483648u, message.repeated_uint32(1));

  assert_eq(2, message.repeated_uint64_size());
  expect_eq(google_ulonglong(18446744073709551615), message.repeated_uint64(0));
  expect_eq(google_ulonglong(9223372036854775808), message.repeated_uint64(1));

  assert_eq(13, message.repeated_double_size());
  expect_eq(123.0     , message.repeated_double(0));
  expect_eq(123.5     , message.repeated_double(1));
  expect_eq(0.125     , message.repeated_double(2));
  expect_eq(1.23e17   , message.repeated_double(3));
  expect_eq(1.235e22  , message.repeated_double(4));
  expect_eq(1.235e-18 , message.repeated_double(5));
  expect_eq(123.456789, message.repeated_double(6));
  expect_eq(message.repeated_double(7), numeric_limits<double>::infinity());
  expect_eq(message.repeated_double(8), numeric_limits<double>::infinity());
  expect_eq(message.repeated_double(9), -numeric_limits<double>::infinity());
  expect_eq(message.repeated_double(10), -numeric_limits<double>::infinity());
  expect_true(isnan(message.repeated_double(11)));
  expect_true(isnan(message.repeated_double(12)));

  // note:  since these string literals have \0's in them, we must explicitly
  //   pass their sizes to string's constructor.
  assert_eq(1, message.repeated_string_size());
  expect_eq(string("\000\001\a\b\f\n\r\t\v\\\'\"", 12),
            message.repeated_string(0));
}

class textformatparsertest : public testing::test {
 protected:
  void expectfailure(const string& input, const string& message, int line,
                     int col) {
    scoped_ptr<unittest::testalltypes> proto(new unittest::testalltypes);
    expectfailure(input, message, line, col, proto.get());
  }

  void expectfailure(const string& input, const string& message, int line,
                     int col, message* proto) {
    expectmessage(input, message, line, col, proto, false);
  }

  void expectmessage(const string& input, const string& message, int line,
                     int col, message* proto, bool expected_result) {
    textformat::parser parser;
    mockerrorcollector error_collector;
    parser.recorderrorsto(&error_collector);
    expect_eq(parser.parsefromstring(input, proto), expected_result);
    expect_eq(simpleitoa(line) + ":" + simpleitoa(col) + ": " + message + "\n",
              error_collector.text_);
  }

  void expectsuccessandtree(const string& input, message* proto,
                            textformat::parseinfotree* info_tree) {
    textformat::parser parser;
    mockerrorcollector error_collector;
    parser.recorderrorsto(&error_collector);
    parser.writelocationsto(info_tree);

    expect_true(parser.parsefromstring(input, proto));
  }

  void expectlocation(textformat::parseinfotree* tree,
                      const descriptor* d, const string& field_name,
                      int index, int line, int column) {
    textformat::parselocation location = tree->getlocation(
        d->findfieldbyname(field_name), index);
    expect_eq(line, location.line);
    expect_eq(column, location.column);
  }

  // an error collector which simply concatenates all its errors into a big
  // block of text which can be checked.
  class mockerrorcollector : public io::errorcollector {
   public:
    mockerrorcollector() {}
    ~mockerrorcollector() {}

    string text_;

    // implements errorcollector -------------------------------------
    void adderror(int line, int column, const string& message) {
      strings::substituteandappend(&text_, "$0:$1: $2\n",
                                   line + 1, column + 1, message);
    }

    void addwarning(int line, int column, const string& message) {
      adderror(line, column, "warning:" + message);
    }
  };
};

test_f(textformatparsertest, parseinfotreebuilding) {
  scoped_ptr<unittest::testalltypes> message(new unittest::testalltypes);
  const descriptor* d = message->getdescriptor();

  string stringdata =
      "optional_int32: 1\n"
      "optional_int64: 2\n"
      "  optional_double: 2.4\n"
      "repeated_int32: 5\n"
      "repeated_int32: 10\n"
      "optional_nested_message <\n"
      "  bb: 78\n"
      ">\n"
      "repeated_nested_message <\n"
      "  bb: 79\n"
      ">\n"
      "repeated_nested_message <\n"
      "  bb: 80\n"
      ">";


  textformat::parseinfotree tree;
  expectsuccessandtree(stringdata, message.get(), &tree);

  // verify that the tree has the correct positions.
  expectlocation(&tree, d, "optional_int32", -1, 0, 0);
  expectlocation(&tree, d, "optional_int64", -1, 1, 0);
  expectlocation(&tree, d, "optional_double", -1, 2, 2);

  expectlocation(&tree, d, "repeated_int32", 0, 3, 0);
  expectlocation(&tree, d, "repeated_int32", 1, 4, 0);

  expectlocation(&tree, d, "optional_nested_message", -1, 5, 0);
  expectlocation(&tree, d, "repeated_nested_message", 0, 8, 0);
  expectlocation(&tree, d, "repeated_nested_message", 1, 11, 0);

  // check for fields not set. for an invalid field, the location returned
  // should be -1, -1.
  expectlocation(&tree, d, "repeated_int64", 0, -1, -1);
  expectlocation(&tree, d, "repeated_int32", 6, -1, -1);
  expectlocation(&tree, d, "some_unknown_field", -1, -1, -1);

  // verify inside the nested message.
  const fielddescriptor* nested_field =
      d->findfieldbyname("optional_nested_message");

  textformat::parseinfotree* nested_tree =
      tree.gettreefornested(nested_field, -1);
  expectlocation(nested_tree, nested_field->message_type(), "bb", -1, 6, 2);

  // verify inside another nested message.
  nested_field = d->findfieldbyname("repeated_nested_message");
  nested_tree = tree.gettreefornested(nested_field, 0);
  expectlocation(nested_tree, nested_field->message_type(), "bb", -1, 9, 2);

  nested_tree = tree.gettreefornested(nested_field, 1);
  expectlocation(nested_tree, nested_field->message_type(), "bb", -1, 12, 2);

  // verify a null tree for an unknown nested field.
  textformat::parseinfotree* unknown_nested_tree =
      tree.gettreefornested(nested_field, 2);

  expect_eq(null, unknown_nested_tree);
}

test_f(textformatparsertest, parsefieldvaluefromstring) {
  scoped_ptr<unittest::testalltypes> message(new unittest::testalltypes);
  const descriptor* d = message->getdescriptor();

#define expect_field(name, value, valuestring) \
  expect_true(textformat::parsefieldvaluefromstring( \
    valuestring, d->findfieldbyname("optional_" #name), message.get())); \
  expect_eq(value, message->optional_##name()); \
  expect_true(message->has_optional_##name());

#define expect_bool_field(name, value, valuestring) \
  expect_true(textformat::parsefieldvaluefromstring( \
    valuestring, d->findfieldbyname("optional_" #name), message.get())); \
  expect_true(message->optional_##name() == value); \
  expect_true(message->has_optional_##name());

#define expect_float_field(name, value, valuestring) \
  expect_true(textformat::parsefieldvaluefromstring( \
    valuestring, d->findfieldbyname("optional_" #name), message.get())); \
  expect_float_eq(value, message->optional_##name()); \
  expect_true(message->has_optional_##name());

#define expect_double_field(name, value, valuestring) \
  expect_true(textformat::parsefieldvaluefromstring( \
    valuestring, d->findfieldbyname("optional_" #name), message.get())); \
  expect_double_eq(value, message->optional_##name()); \
  expect_true(message->has_optional_##name());

#define expect_invalid(name, valuestring) \
  expect_false(textformat::parsefieldvaluefromstring( \
    valuestring, d->findfieldbyname("optional_" #name), message.get()));

  // int32
  expect_field(int32, 1, "1");
  expect_field(int32, -1, "-1");
  expect_field(int32, 0x1234, "0x1234");
  expect_invalid(int32, "a");
  expect_invalid(int32, "999999999999999999999999999999999999");
  expect_invalid(int32, "1,2");

  // int64
  expect_field(int64, 1, "1");
  expect_field(int64, -1, "-1");
  expect_field(int64, 0x1234567812345678ll, "0x1234567812345678");
  expect_invalid(int64, "a");
  expect_invalid(int64, "999999999999999999999999999999999999");
  expect_invalid(int64, "1,2");

  // uint64
  expect_field(uint64, 1, "1");
  expect_field(uint64, 0xf234567812345678ull, "0xf234567812345678");
  expect_invalid(uint64, "-1");
  expect_invalid(uint64, "a");
  expect_invalid(uint64, "999999999999999999999999999999999999");
  expect_invalid(uint64, "1,2");

  // fixed32
  expect_field(fixed32, 1, "1");
  expect_field(fixed32, 0x12345678, "0x12345678");
  expect_invalid(fixed32, "-1");
  expect_invalid(fixed32, "a");
  expect_invalid(fixed32, "999999999999999999999999999999999999");
  expect_invalid(fixed32, "1,2");

  // fixed64
  expect_field(fixed64, 1, "1");
  expect_field(fixed64, 0x1234567812345678ull, "0x1234567812345678");
  expect_invalid(fixed64, "-1");
  expect_invalid(fixed64, "a");
  expect_invalid(fixed64, "999999999999999999999999999999999999");
  expect_invalid(fixed64, "1,2");

  // bool
  expect_bool_field(bool, true, "true");
  expect_bool_field(bool, false, "false");
  expect_bool_field(bool, true, "1");
  expect_bool_field(bool, true, "t");
  expect_bool_field(bool, false, "0");
  expect_bool_field(bool, false, "f");
  expect_invalid(bool, "2");
  expect_invalid(bool, "-0");
  expect_invalid(bool, "on");
  expect_invalid(bool, "a");
  expect_invalid(bool, "true");

  // float
  expect_field(float, 1, "1");
  expect_float_field(float, 1.5, "1.5");
  expect_float_field(float, 1.5e3, "1.5e3");
  expect_float_field(float, -4.55, "-4.55");
  expect_invalid(float, "a");
  expect_invalid(float, "1,2");

  // double
  expect_field(double, 1, "1");
  expect_field(double, -1, "-1");
  expect_double_field(double, 2.3, "2.3");
  expect_double_field(double, 3e5, "3e5");
  expect_invalid(double, "a");
  expect_invalid(double, "1,2");

  // string
  expect_field(string, "hello", "\"hello\"");
  expect_field(string, "-1.87", "'-1.87'");
  expect_invalid(string, "hello");  // without quote for value

  // enum
  expect_field(nested_enum, unittest::testalltypes::bar, "bar");
  expect_field(nested_enum, unittest::testalltypes::baz,
               simpleitoa(unittest::testalltypes::baz));
  expect_invalid(nested_enum, "foobar");

  // message
  expect_true(textformat::parsefieldvaluefromstring(
    "<bb:12>", d->findfieldbyname("optional_nested_message"), message.get()));
  expect_eq(12, message->optional_nested_message().bb()); \
  expect_true(message->has_optional_nested_message());
  expect_invalid(nested_message, "any");

#undef expect_field
#undef expect_bool_field
#undef expect_float_field
#undef expect_double_field
#undef expect_invalid
}


test_f(textformatparsertest, invalidtoken) {
  expectfailure("optional_bool: true\n-5\n", "expected identifier.",
                2, 1);

  expectfailure("optional_bool: true!\n", "expected identifier.", 1, 20);
  expectfailure("\"some string\"", "expected identifier.", 1, 1);
}

test_f(textformatparsertest, invalidfieldname) {
  expectfailure(
      "invalid_field: somevalue\n",
      "message type \"protobuf_unittest.testalltypes\" has no field named "
      "\"invalid_field\".",
      1, 14);
}

test_f(textformatparsertest, invalidcapitalization) {
  // we require that group names be exactly as they appear in the .proto.
  expectfailure(
      "optionalgroup {\na: 15\n}\n",
      "message type \"protobuf_unittest.testalltypes\" has no field named "
      "\"optionalgroup\".",
      1, 15);
  expectfailure(
      "optionalgroup {\na: 15\n}\n",
      "message type \"protobuf_unittest.testalltypes\" has no field named "
      "\"optionalgroup\".",
      1, 15);
  expectfailure(
      "optional_double: 10.0\n",
      "message type \"protobuf_unittest.testalltypes\" has no field named "
      "\"optional_double\".",
      1, 16);
}

test_f(textformatparsertest, invalidfieldvalues) {
  // invalid values for a double/float field.
  expectfailure("optional_double: \"hello\"\n", "expected double.", 1, 18);
  expectfailure("optional_double: true\n", "expected double.", 1, 18);
  expectfailure("optional_double: !\n", "expected double.", 1, 18);
  expectfailure("optional_double {\n  \n}\n", "expected \":\", found \"{\".",
                1, 17);

  // invalid values for a signed integer field.
  expectfailure("optional_int32: \"hello\"\n", "expected integer.", 1, 17);
  expectfailure("optional_int32: true\n", "expected integer.", 1, 17);
  expectfailure("optional_int32: 4.5\n", "expected integer.", 1, 17);
  expectfailure("optional_int32: !\n", "expected integer.", 1, 17);
  expectfailure("optional_int32 {\n \n}\n", "expected \":\", found \"{\".",
                1, 16);
  expectfailure("optional_int32: 0x80000000\n",
                "integer out of range.", 1, 17);
  expectfailure("optional_int64: 0x8000000000000000\n",
                "integer out of range.", 1, 17);
  expectfailure("optional_int32: -0x80000001\n",
                "integer out of range.", 1, 18);
  expectfailure("optional_int64: -0x8000000000000001\n",
                "integer out of range.", 1, 18);

  // invalid values for an unsigned integer field.
  expectfailure("optional_uint64: \"hello\"\n", "expected integer.", 1, 18);
  expectfailure("optional_uint64: true\n", "expected integer.", 1, 18);
  expectfailure("optional_uint64: 4.5\n", "expected integer.", 1, 18);
  expectfailure("optional_uint64: -5\n", "expected integer.", 1, 18);
  expectfailure("optional_uint64: !\n", "expected integer.", 1, 18);
  expectfailure("optional_uint64 {\n \n}\n", "expected \":\", found \"{\".",
                1, 17);
  expectfailure("optional_uint32: 0x100000000\n",
                "integer out of range.", 1, 18);
  expectfailure("optional_uint64: 0x10000000000000000\n",
                "integer out of range.", 1, 18);

  // invalid values for a boolean field.
  expectfailure("optional_bool: \"hello\"\n", "expected identifier.", 1, 16);
  expectfailure("optional_bool: 5\n", "integer out of range.", 1, 16);
  expectfailure("optional_bool: -7.5\n", "expected identifier.", 1, 16);
  expectfailure("optional_bool: !\n", "expected identifier.", 1, 16);

  expectfailure(
      "optional_bool: meh\n",
      "invalid value for boolean field \"optional_bool\". value: \"meh\".",
      2, 1);

  expectfailure("optional_bool {\n \n}\n", "expected \":\", found \"{\".",
                1, 15);

  // invalid values for a string field.
  expectfailure("optional_string: true\n", "expected string.", 1, 18);
  expectfailure("optional_string: 5\n", "expected string.", 1, 18);
  expectfailure("optional_string: -7.5\n", "expected string.", 1, 18);
  expectfailure("optional_string: !\n", "expected string.", 1, 18);
  expectfailure("optional_string {\n \n}\n", "expected \":\", found \"{\".",
                1, 17);

  // invalid values for an enumeration field.
  expectfailure("optional_nested_enum: \"hello\"\n",
                "expected integer or identifier.", 1, 23);

  // valid token, but enum value is not defined.
  expectfailure("optional_nested_enum: 5\n",
                "unknown enumeration value of \"5\" for field "
                "\"optional_nested_enum\".", 2, 1);
  // we consume the negative sign, so the error position starts one character
  // later.
  expectfailure("optional_nested_enum: -7.5\n", "expected integer.", 1, 24);
  expectfailure("optional_nested_enum: !\n",
                "expected integer or identifier.", 1, 23);

  expectfailure(
      "optional_nested_enum: grah\n",
      "unknown enumeration value of \"grah\" for field "
      "\"optional_nested_enum\".", 2, 1);

  expectfailure(
      "optional_nested_enum {\n \n}\n",
      "expected \":\", found \"{\".", 1, 22);
}

test_f(textformatparsertest, messagedelimeters) {
  // non-matching delimeters.
  expectfailure("optionalgroup <\n \n}\n", "expected \">\", found \"}\".",
                3, 1);

  // invalid delimeters.
  expectfailure("optionalgroup [\n \n]\n", "expected \"{\", found \"[\".",
                1, 15);

  // unending message.
  expectfailure("optional_nested_message {\n \nbb: 118\n",
                "expected identifier.",
                4, 1);
}

test_f(textformatparsertest, unknownextension) {
  // non-matching delimeters.
  expectfailure("[blahblah]: 123",
                "extension \"blahblah\" is not defined or is not an "
                "extension of \"protobuf_unittest.testalltypes\".",
                1, 11);
}

test_f(textformatparsertest, missingrequired) {
  unittest::testrequired message;
  expectfailure("a: 1",
                "message missing required fields: b, c",
                0, 1, &message);
}

test_f(textformatparsertest, parseduplicaterequired) {
  unittest::testrequired message;
  expectfailure("a: 1 b: 2 c: 3 a: 1",
                "non-repeated field \"a\" is specified multiple times.",
                1, 17, &message);
}

test_f(textformatparsertest, parseduplicateoptional) {
  unittest::foreignmessage message;
  expectfailure("c: 1 c: 2",
                "non-repeated field \"c\" is specified multiple times.",
                1, 7, &message);
}

test_f(textformatparsertest, mergeduplicaterequired) {
  unittest::testrequired message;
  textformat::parser parser;
  expect_true(parser.mergefromstring("a: 1 b: 2 c: 3 a: 4", &message));
  expect_eq(4, message.a());
}

test_f(textformatparsertest, mergeduplicateoptional) {
  unittest::foreignmessage message;
  textformat::parser parser;
  expect_true(parser.mergefromstring("c: 1 c: 2", &message));
  expect_eq(2, message.c());
}

test_f(textformatparsertest, explicitdelimiters) {
  unittest::testrequired message;
  expect_true(textformat::parsefromstring("a:1,b:2;c:3", &message));
  expect_eq(1, message.a());
  expect_eq(2, message.b());
  expect_eq(3, message.c());
}

test_f(textformatparsertest, printerrorstostderr) {
  vector<string> errors;

  {
    scopedmemorylog log;
    unittest::testalltypes proto;
    expect_false(textformat::parsefromstring("no_such_field: 1", &proto));
    errors = log.getmessages(error);
  }

  assert_eq(1, errors.size());
  expect_eq("error parsing text-format protobuf_unittest.testalltypes: "
            "1:14: message type \"protobuf_unittest.testalltypes\" has no field "
            "named \"no_such_field\".",
            errors[0]);
}

test_f(textformatparsertest, failsontokenizationerror) {
  vector<string> errors;

  {
    scopedmemorylog log;
    unittest::testalltypes proto;
    expect_false(textformat::parsefromstring("\020", &proto));
    errors = log.getmessages(error);
  }

  assert_eq(1, errors.size());
  expect_eq("error parsing text-format protobuf_unittest.testalltypes: "
            "1:1: invalid control characters encountered in text.",
            errors[0]);
}

test_f(textformatparsertest, parsedeprecatedfield) {
  unittest::testdeprecatedfields message;
  expectmessage("deprecated_int32: 42",
                "warning:text format contains deprecated field "
                "\"deprecated_int32\"", 1, 21, &message, true);
}

class textformatmessagesettest : public testing::test {
 protected:
  static const char proto_debug_string_[];
};
const char textformatmessagesettest::proto_debug_string_[] =
"message_set {\n"
"  [protobuf_unittest.testmessagesetextension1] {\n"
"    i: 23\n"
"  }\n"
"  [protobuf_unittest.testmessagesetextension2] {\n"
"    str: \"foo\"\n"
"  }\n"
"}\n";


test_f(textformatmessagesettest, serialize) {
  protobuf_unittest::testmessagesetcontainer proto;
  protobuf_unittest::testmessagesetextension1* item_a =
    proto.mutable_message_set()->mutableextension(
      protobuf_unittest::testmessagesetextension1::message_set_extension);
  item_a->set_i(23);
  protobuf_unittest::testmessagesetextension2* item_b =
    proto.mutable_message_set()->mutableextension(
      protobuf_unittest::testmessagesetextension2::message_set_extension);
  item_b->set_str("foo");
  expect_eq(proto_debug_string_, proto.debugstring());
}

test_f(textformatmessagesettest, deserialize) {
  protobuf_unittest::testmessagesetcontainer proto;
  assert_true(textformat::parsefromstring(proto_debug_string_, &proto));
  expect_eq(23, proto.message_set().getextension(
    protobuf_unittest::testmessagesetextension1::message_set_extension).i());
  expect_eq("foo", proto.message_set().getextension(
    protobuf_unittest::testmessagesetextension2::message_set_extension).str());

  // ensure that these are the only entries present.
  vector<const fielddescriptor*> descriptors;
  proto.message_set().getreflection()->listfields(
    proto.message_set(), &descriptors);
  expect_eq(2, descriptors.size());
}


}  // namespace text_format_unittest
}  // namespace protobuf
}  // namespace google
