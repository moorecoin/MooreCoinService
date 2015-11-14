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

#include <vector>
#include <algorithm>
#include <map>

#include <google/protobuf/compiler/parser.h>

#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_custom_options.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/map-util.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {

namespace {

class mockerrorcollector : public io::errorcollector {
 public:
  mockerrorcollector() {}
  ~mockerrorcollector() {}

  string text_;

  // implements errorcollector ---------------------------------------
  void adderror(int line, int column, const string& message) {
    strings::substituteandappend(&text_, "$0:$1: $2\n",
                                 line, column, message);
  }
};

class mockvalidationerrorcollector : public descriptorpool::errorcollector {
 public:
  mockvalidationerrorcollector(const sourcelocationtable& source_locations,
                               io::errorcollector* wrapped_collector)
    : source_locations_(source_locations),
      wrapped_collector_(wrapped_collector) {}
  ~mockvalidationerrorcollector() {}

  // implements errorcollector ---------------------------------------
  void adderror(const string& filename,
                const string& element_name,
                const message* descriptor,
                errorlocation location,
                const string& message) {
    int line, column;
    source_locations_.find(descriptor, location, &line, &column);
    wrapped_collector_->adderror(line, column, message);
  }

 private:
  const sourcelocationtable& source_locations_;
  io::errorcollector* wrapped_collector_;
};

class parsertest : public testing::test {
 protected:
  parsertest()
    : require_syntax_identifier_(false) {}

  // set up the parser to parse the given text.
  void setupparser(const char* text) {
    raw_input_.reset(new io::arrayinputstream(text, strlen(text)));
    input_.reset(new io::tokenizer(raw_input_.get(), &error_collector_));
    parser_.reset(new parser());
    parser_->recorderrorsto(&error_collector_);
    parser_->setrequiresyntaxidentifier(require_syntax_identifier_);
  }

  // parse the input and expect that the resulting filedescriptorproto matches
  // the given output.  the output is a filedescriptorproto in protocol buffer
  // text format.
  void expectparsesto(const char* input, const char* output) {
    setupparser(input);
    filedescriptorproto actual, expected;

    parser_->parse(input_.get(), &actual);
    expect_eq(io::tokenizer::type_end, input_->current().type);
    assert_eq("", error_collector_.text_);

    // we don't cover sourcecodeinfo in these tests.
    actual.clear_source_code_info();

    // parse the ascii representation in order to canonicalize it.  we could
    // just compare directly to actual.debugstring(), but that would require
    // that the caller precisely match the formatting that debugstring()
    // produces.
    assert_true(textformat::parsefromstring(output, &expected));

    // compare by comparing debug strings.
    // todo(kenton):  use differencer, once it is available.
    expect_eq(expected.debugstring(), actual.debugstring());
  }

  // parse the text and expect that the given errors are reported.
  void expecthaserrors(const char* text, const char* expected_errors) {
    expecthasearlyexiterrors(text, expected_errors);
    expect_eq(io::tokenizer::type_end, input_->current().type);
  }

  // same as above but does not expect that the parser parses the complete
  // input.
  void expecthasearlyexiterrors(const char* text, const char* expected_errors) {
    setupparser(text);
    filedescriptorproto file;
    parser_->parse(input_.get(), &file);
    expect_eq(expected_errors, error_collector_.text_);
  }

  // parse the text as a file and validate it (with a descriptorpool), and
  // expect that the validation step reports the given errors.
  void expecthasvalidationerrors(const char* text,
                                 const char* expected_errors) {
    setupparser(text);
    sourcelocationtable source_locations;
    parser_->recordsourcelocationsto(&source_locations);

    filedescriptorproto file;
    file.set_name("foo.proto");
    parser_->parse(input_.get(), &file);
    expect_eq(io::tokenizer::type_end, input_->current().type);
    assert_eq("", error_collector_.text_);

    mockvalidationerrorcollector validation_error_collector(
      source_locations, &error_collector_);
    expect_true(pool_.buildfilecollectingerrors(
      file, &validation_error_collector) == null);
    expect_eq(expected_errors, error_collector_.text_);
  }

  mockerrorcollector error_collector_;
  descriptorpool pool_;

  scoped_ptr<io::zerocopyinputstream> raw_input_;
  scoped_ptr<io::tokenizer> input_;
  scoped_ptr<parser> parser_;
  bool require_syntax_identifier_;
};

// ===================================================================

test_f(parsertest, stopaftersyntaxidentifier) {
  setupparser(
    "// blah\n"
    "syntax = \"foobar\";\n"
    "this line will not be parsed\n");
  parser_->setstopaftersyntaxidentifier(true);
  expect_true(parser_->parse(input_.get(), null));
  expect_eq("", error_collector_.text_);
  expect_eq("foobar", parser_->getsyntaxidentifier());
}

test_f(parsertest, stopafteromittedsyntaxidentifier) {
  setupparser(
    "// blah\n"
    "this line will not be parsed\n");
  parser_->setstopaftersyntaxidentifier(true);
  expect_true(parser_->parse(input_.get(), null));
  expect_eq("", error_collector_.text_);
  expect_eq("", parser_->getsyntaxidentifier());
}

test_f(parsertest, stopaftersyntaxidentifierwitherrors) {
  setupparser(
    "// blah\n"
    "syntax = error;\n");
  parser_->setstopaftersyntaxidentifier(true);
  expect_false(parser_->parse(input_.get(), null));
  expect_eq("1:9: expected syntax identifier.\n", error_collector_.text_);
}

// ===================================================================

typedef parsertest parsemessagetest;

test_f(parsemessagetest, simplemessage) {
  expectparsesto(
    "message testmessage {\n"
    "  required int32 foo = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32 number:1 }"
    "}");
}

test_f(parsemessagetest, implicitsyntaxidentifier) {
  require_syntax_identifier_ = false;
  expectparsesto(
    "message testmessage {\n"
    "  required int32 foo = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32 number:1 }"
    "}");
  expect_eq("proto2", parser_->getsyntaxidentifier());
}

test_f(parsemessagetest, explicitsyntaxidentifier) {
  expectparsesto(
    "syntax = \"proto2\";\n"
    "message testmessage {\n"
    "  required int32 foo = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32 number:1 }"
    "}");
  expect_eq("proto2", parser_->getsyntaxidentifier());
}

test_f(parsemessagetest, explicitrequiredsyntaxidentifier) {
  require_syntax_identifier_ = true;
  expectparsesto(
    "syntax = \"proto2\";\n"
    "message testmessage {\n"
    "  required int32 foo = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32 number:1 }"
    "}");
  expect_eq("proto2", parser_->getsyntaxidentifier());
}

test_f(parsemessagetest, simplefields) {
  expectparsesto(
    "message testmessage {\n"
    "  required int32 foo = 15;\n"
    "  optional int32 bar = 34;\n"
    "  repeated int32 baz = 3;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32 number:15 }"
    "  field { name:\"bar\" label:label_optional type:type_int32 number:34 }"
    "  field { name:\"baz\" label:label_repeated type:type_int32 number:3  }"
    "}");
}

test_f(parsemessagetest, primitivefieldtypes) {
  expectparsesto(
    "message testmessage {\n"
    "  required int32    foo = 1;\n"
    "  required int64    foo = 1;\n"
    "  required uint32   foo = 1;\n"
    "  required uint64   foo = 1;\n"
    "  required sint32   foo = 1;\n"
    "  required sint64   foo = 1;\n"
    "  required fixed32  foo = 1;\n"
    "  required fixed64  foo = 1;\n"
    "  required sfixed32 foo = 1;\n"
    "  required sfixed64 foo = 1;\n"
    "  required float    foo = 1;\n"
    "  required double   foo = 1;\n"
    "  required string   foo = 1;\n"
    "  required bytes    foo = 1;\n"
    "  required bool     foo = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name:\"foo\" label:label_required type:type_int32    number:1 }"
    "  field { name:\"foo\" label:label_required type:type_int64    number:1 }"
    "  field { name:\"foo\" label:label_required type:type_uint32   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_uint64   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_sint32   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_sint64   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_fixed32  number:1 }"
    "  field { name:\"foo\" label:label_required type:type_fixed64  number:1 }"
    "  field { name:\"foo\" label:label_required type:type_sfixed32 number:1 }"
    "  field { name:\"foo\" label:label_required type:type_sfixed64 number:1 }"
    "  field { name:\"foo\" label:label_required type:type_float    number:1 }"
    "  field { name:\"foo\" label:label_required type:type_double   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_string   number:1 }"
    "  field { name:\"foo\" label:label_required type:type_bytes    number:1 }"
    "  field { name:\"foo\" label:label_required type:type_bool     number:1 }"
    "}");
}

test_f(parsemessagetest, fielddefaults) {
  expectparsesto(
    "message testmessage {\n"
    "  required int32  foo = 1 [default=  1  ];\n"
    "  required int32  foo = 1 [default= -2  ];\n"
    "  required int64  foo = 1 [default=  3  ];\n"
    "  required int64  foo = 1 [default= -4  ];\n"
    "  required uint32 foo = 1 [default=  5  ];\n"
    "  required uint64 foo = 1 [default=  6  ];\n"
    "  required float  foo = 1 [default=  7.5];\n"
    "  required float  foo = 1 [default= -8.5];\n"
    "  required float  foo = 1 [default=  9  ];\n"
    "  required double foo = 1 [default= 10.5];\n"
    "  required double foo = 1 [default=-11.5];\n"
    "  required double foo = 1 [default= 12  ];\n"
    "  required double foo = 1 [default= inf ];\n"
    "  required double foo = 1 [default=-inf ];\n"
    "  required double foo = 1 [default= nan ];\n"
    "  required string foo = 1 [default='13\\001'];\n"
    "  required string foo = 1 [default='a' \"b\" \n \"c\"];\n"
    "  required bytes  foo = 1 [default='14\\002'];\n"
    "  required bytes  foo = 1 [default='a' \"b\" \n 'c'];\n"
    "  required bool   foo = 1 [default=true ];\n"
    "  required foo    foo = 1 [default=foo  ];\n"

    "  required int32  foo = 1 [default= 0x7fffffff];\n"
    "  required int32  foo = 1 [default=-0x80000000];\n"
    "  required uint32 foo = 1 [default= 0xffffffff];\n"
    "  required int64  foo = 1 [default= 0x7fffffffffffffff];\n"
    "  required int64  foo = 1 [default=-0x8000000000000000];\n"
    "  required uint64 foo = 1 [default= 0xffffffffffffffff];\n"
    "  required double foo = 1 [default= 0xabcd];\n"
    "}\n",

#define etc "name:\"foo\" label:label_required number:1"
    "message_type {"
    "  name: \"testmessage\""
    "  field { type:type_int32   default_value:\"1\"         "etc" }"
    "  field { type:type_int32   default_value:\"-2\"        "etc" }"
    "  field { type:type_int64   default_value:\"3\"         "etc" }"
    "  field { type:type_int64   default_value:\"-4\"        "etc" }"
    "  field { type:type_uint32  default_value:\"5\"         "etc" }"
    "  field { type:type_uint64  default_value:\"6\"         "etc" }"
    "  field { type:type_float   default_value:\"7.5\"       "etc" }"
    "  field { type:type_float   default_value:\"-8.5\"      "etc" }"
    "  field { type:type_float   default_value:\"9\"         "etc" }"
    "  field { type:type_double  default_value:\"10.5\"      "etc" }"
    "  field { type:type_double  default_value:\"-11.5\"     "etc" }"
    "  field { type:type_double  default_value:\"12\"        "etc" }"
    "  field { type:type_double  default_value:\"inf\"       "etc" }"
    "  field { type:type_double  default_value:\"-inf\"      "etc" }"
    "  field { type:type_double  default_value:\"nan\"       "etc" }"
    "  field { type:type_string  default_value:\"13\\001\"   "etc" }"
    "  field { type:type_string  default_value:\"abc\"       "etc" }"
    "  field { type:type_bytes   default_value:\"14\\\\002\" "etc" }"
    "  field { type:type_bytes   default_value:\"abc\"       "etc" }"
    "  field { type:type_bool    default_value:\"true\"      "etc" }"
    "  field { type_name:\"foo\" default_value:\"foo\"       "etc" }"

    "  field { type:type_int32   default_value:\"2147483647\"           "etc" }"
    "  field { type:type_int32   default_value:\"-2147483648\"          "etc" }"
    "  field { type:type_uint32  default_value:\"4294967295\"           "etc" }"
    "  field { type:type_int64   default_value:\"9223372036854775807\"  "etc" }"
    "  field { type:type_int64   default_value:\"-9223372036854775808\" "etc" }"
    "  field { type:type_uint64  default_value:\"18446744073709551615\" "etc" }"
    "  field { type:type_double  default_value:\"43981\"                "etc" }"
    "}");
#undef etc
}

test_f(parsemessagetest, fieldoptions) {
  expectparsesto(
    "message testmessage {\n"
    "  optional string foo = 1\n"
    "      [ctype=cord, (foo)=7, foo.(.bar.baz).qux.quux.(corge)=-33, \n"
    "       (quux)=\"x\040y\", (baz.qux)=hey];\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  field { name: \"foo\" label: label_optional type: type_string number: 1"
    "          options { uninterpreted_option: { name { name_part: \"ctype\" "
    "                                                   is_extension: false } "
    "                                            identifier_value: \"cord\"  }"
    "                    uninterpreted_option: { name { name_part: \"foo\" "
    "                                                   is_extension: true } "
    "                                            positive_int_value: 7  }"
    "                    uninterpreted_option: { name { name_part: \"foo\" "
    "                                                   is_extension: false } "
    "                                            name { name_part: \".bar.baz\""
    "                                                   is_extension: true } "
    "                                            name { name_part: \"qux\" "
    "                                                   is_extension: false } "
    "                                            name { name_part: \"quux\" "
    "                                                   is_extension: false } "
    "                                            name { name_part: \"corge\" "
    "                                                   is_extension: true } "
    "                                            negative_int_value: -33 }"
    "                    uninterpreted_option: { name { name_part: \"quux\" "
    "                                                   is_extension: true } "
    "                                            string_value: \"x y\" }"
    "                    uninterpreted_option: { name { name_part: \"baz.qux\" "
    "                                                   is_extension: true } "
    "                                            identifier_value: \"hey\" }"
    "          }"
    "  }"
    "}");
}

test_f(parsemessagetest, group) {
  expectparsesto(
    "message testmessage {\n"
    "  optional group testgroup = 1 {};\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  nested_type { name: \"testgroup\" }"
    "  field { name:\"testgroup\" label:label_optional number:1"
    "          type:type_group type_name: \"testgroup\" }"
    "}");
}

test_f(parsemessagetest, nestedmessage) {
  expectparsesto(
    "message testmessage {\n"
    "  message nested {}\n"
    "  optional nested test_nested = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  nested_type { name: \"nested\" }"
    "  field { name:\"test_nested\" label:label_optional number:1"
    "          type_name: \"nested\" }"
    "}");
}

test_f(parsemessagetest, nestedenum) {
  expectparsesto(
    "message testmessage {\n"
    "  enum nestedenum {}\n"
    "  optional nestedenum test_enum = 1;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  enum_type { name: \"nestedenum\" }"
    "  field { name:\"test_enum\" label:label_optional number:1"
    "          type_name: \"nestedenum\" }"
    "}");
}

test_f(parsemessagetest, extensionrange) {
  expectparsesto(
    "message testmessage {\n"
    "  extensions 10 to 19;\n"
    "  extensions 30 to max;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  extension_range { start:10 end:20        }"
    "  extension_range { start:30 end:536870912 }"
    "}");
}

test_f(parsemessagetest, compoundextensionrange) {
  expectparsesto(
    "message testmessage {\n"
    "  extensions 2, 15, 9 to 11, 100 to max, 3;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  extension_range { start:2   end:3         }"
    "  extension_range { start:15  end:16        }"
    "  extension_range { start:9   end:12        }"
    "  extension_range { start:100 end:536870912 }"
    "  extension_range { start:3   end:4         }"
    "}");
}

test_f(parsemessagetest, largermaxformessagesetwireformatmessages) {
  // messages using the message_set_wire_format option can accept larger
  // extension numbers, as the numbers are not encoded as int32 field values
  // rather than tags.
  expectparsesto(
    "message testmessage {\n"
    "  extensions 4 to max;\n"
    "  option message_set_wire_format = true;\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "    extension_range { start:4 end: 0x7fffffff }"
    "  options {\n"
    "    uninterpreted_option { \n"
    "      name {\n"
    "        name_part: \"message_set_wire_format\"\n"
    "        is_extension: false\n"
    "      }\n"
    "      identifier_value: \"true\"\n"
    "    }\n"
    "  }\n"
    "}");
}

test_f(parsemessagetest, extensions) {
  expectparsesto(
    "extend extendee1 { optional int32 foo = 12; }\n"
    "extend extendee2 { repeated testmessage bar = 22; }\n",

    "extension { name:\"foo\" label:label_optional type:type_int32 number:12"
    "            extendee: \"extendee1\" } "
    "extension { name:\"bar\" label:label_repeated number:22"
    "            type_name:\"testmessage\" extendee: \"extendee2\" }");
}

test_f(parsemessagetest, extensionsinmessagescope) {
  expectparsesto(
    "message testmessage {\n"
    "  extend extendee1 { optional int32 foo = 12; }\n"
    "  extend extendee2 { repeated testmessage bar = 22; }\n"
    "}\n",

    "message_type {"
    "  name: \"testmessage\""
    "  extension { name:\"foo\" label:label_optional type:type_int32 number:12"
    "              extendee: \"extendee1\" }"
    "  extension { name:\"bar\" label:label_repeated number:22"
    "              type_name:\"testmessage\" extendee: \"extendee2\" }"
    "}");
}

test_f(parsemessagetest, multipleextensionsoneextendee) {
  expectparsesto(
    "extend extendee1 {\n"
    "  optional int32 foo = 12;\n"
    "  repeated testmessage bar = 22;\n"
    "}\n",

    "extension { name:\"foo\" label:label_optional type:type_int32 number:12"
    "            extendee: \"extendee1\" } "
    "extension { name:\"bar\" label:label_repeated number:22"
    "            type_name:\"testmessage\" extendee: \"extendee1\" }");
}

// ===================================================================

typedef parsertest parseenumtest;

test_f(parseenumtest, simpleenum) {
  expectparsesto(
    "enum testenum {\n"
    "  foo = 0;\n"
    "}\n",

    "enum_type {"
    "  name: \"testenum\""
    "  value { name:\"foo\" number:0 }"
    "}");
}

test_f(parseenumtest, values) {
  expectparsesto(
    "enum testenum {\n"
    "  foo = 13;\n"
    "  bar = -10;\n"
    "  baz = 500;\n"
    "  hex_max = 0x7fffffff;\n"
    "  hex_min = -0x80000000;\n"
    "  int_max = 2147483647;\n"
    "  int_min = -2147483648;\n"
    "}\n",

    "enum_type {"
    "  name: \"testenum\""
    "  value { name:\"foo\" number:13 }"
    "  value { name:\"bar\" number:-10 }"
    "  value { name:\"baz\" number:500 }"
    "  value { name:\"hex_max\" number:2147483647 }"
    "  value { name:\"hex_min\" number:-2147483648 }"
    "  value { name:\"int_max\" number:2147483647 }"
    "  value { name:\"int_min\" number:-2147483648 }"
    "}");
}

test_f(parseenumtest, valueoptions) {
  expectparsesto(
    "enum testenum {\n"
    "  foo = 13;\n"
    "  bar = -10 [ (something.text) = 'abc' ];\n"
    "  baz = 500 [ (something.text) = 'def', other = 1 ];\n"
    "}\n",

    "enum_type {"
    "  name: \"testenum\""
    "  value { name: \"foo\" number: 13 }"
    "  value { name: \"bar\" number: -10 "
    "    options { "
    "      uninterpreted_option { "
    "        name { name_part: \"something.text\" is_extension: true } "
    "        string_value: \"abc\" "
    "      } "
    "    } "
    "  } "
    "  value { name: \"baz\" number: 500 "
    "    options { "
    "      uninterpreted_option { "
    "        name { name_part: \"something.text\" is_extension: true } "
    "        string_value: \"def\" "
    "      } "
    "      uninterpreted_option { "
    "        name { name_part: \"other\" is_extension: false } "
    "        positive_int_value: 1 "
    "      } "
    "    } "
    "  } "
    "}");
}

// ===================================================================

typedef parsertest parseservicetest;

test_f(parseservicetest, simpleservice) {
  expectparsesto(
    "service testservice {\n"
    "  rpc foo(in) returns (out);\n"
    "}\n",

    "service {"
    "  name: \"testservice\""
    "  method { name:\"foo\" input_type:\"in\" output_type:\"out\" }"
    "}");
}

test_f(parseservicetest, methodsandstreams) {
  expectparsesto(
    "service testservice {\n"
    "  rpc foo(in1) returns (out1);\n"
    "  rpc bar(in2) returns (out2);\n"
    "  rpc baz(in3) returns (out3);\n"
    "}\n",

    "service {"
    "  name: \"testservice\""
    "  method { name:\"foo\" input_type:\"in1\" output_type:\"out1\" }"
    "  method { name:\"bar\" input_type:\"in2\" output_type:\"out2\" }"
    "  method { name:\"baz\" input_type:\"in3\" output_type:\"out3\" }"
    "}");
}

// ===================================================================
// imports and packages

typedef parsertest parsemisctest;

test_f(parsemisctest, parseimport) {
  expectparsesto(
    "import \"foo/bar/baz.proto\";\n",
    "dependency: \"foo/bar/baz.proto\"");
}

test_f(parsemisctest, parsemultipleimports) {
  expectparsesto(
    "import \"foo.proto\";\n"
    "import \"bar.proto\";\n"
    "import \"baz.proto\";\n",
    "dependency: \"foo.proto\""
    "dependency: \"bar.proto\""
    "dependency: \"baz.proto\"");
}

test_f(parsemisctest, parsepublicimports) {
  expectparsesto(
    "import \"foo.proto\";\n"
    "import public \"bar.proto\";\n"
    "import \"baz.proto\";\n"
    "import public \"qux.proto\";\n",
    "dependency: \"foo.proto\""
    "dependency: \"bar.proto\""
    "dependency: \"baz.proto\""
    "dependency: \"qux.proto\""
    "public_dependency: 1 "
    "public_dependency: 3 ");
}

test_f(parsemisctest, parsepackage) {
  expectparsesto(
    "package foo.bar.baz;\n",
    "package: \"foo.bar.baz\"");
}

test_f(parsemisctest, parsepackagewithspaces) {
  expectparsesto(
    "package foo   .   bar.  \n"
    "  baz;\n",
    "package: \"foo.bar.baz\"");
}

// ===================================================================
// options

test_f(parsemisctest, parsefileoptions) {
  expectparsesto(
    "option java_package = \"com.google.foo\";\n"
    "option optimize_for = code_size;",

    "options {"
    "uninterpreted_option { name { name_part: \"java_package\" "
    "                              is_extension: false }"
    "                       string_value: \"com.google.foo\"} "
    "uninterpreted_option { name { name_part: \"optimize_for\" "
    "                              is_extension: false }"
    "                       identifier_value: \"code_size\" } "
    "}");
}

// ===================================================================
// error tests
//
// there are a very large number of possible errors that the parser could
// report, so it's infeasible to test every single one of them.  instead,
// we test each unique call to adderror() in parser.h.  this does not mean
// we are testing every possible error that parser can generate because
// each variant of the consume() helper only counts as one unique call to
// adderror().

typedef parsertest parseerrortest;

test_f(parseerrortest, missingsyntaxidentifier) {
  require_syntax_identifier_ = true;
  expecthasearlyexiterrors(
    "message testmessage {}",
    "0:0: file must begin with 'syntax = \"proto2\";'.\n");
  expect_eq("", parser_->getsyntaxidentifier());
}

test_f(parseerrortest, unknownsyntaxidentifier) {
  expecthasearlyexiterrors(
    "syntax = \"no_such_syntax\";",
    "0:9: unrecognized syntax identifier \"no_such_syntax\".  this parser "
      "only recognizes \"proto2\".\n");
  expect_eq("no_such_syntax", parser_->getsyntaxidentifier());
}

test_f(parseerrortest, simplesyntaxerror) {
  expecthaserrors(
    "message testmessage @#$ { blah }",
    "0:20: expected \"{\".\n");
  expect_eq("proto2", parser_->getsyntaxidentifier());
}

test_f(parseerrortest, expectedtoplevel) {
  expecthaserrors(
    "blah;",
    "0:0: expected top-level statement (e.g. \"message\").\n");
}

test_f(parseerrortest, unmatchedclosebrace) {
  // this used to cause an infinite loop.  doh.
  expecthaserrors(
    "}",
    "0:0: expected top-level statement (e.g. \"message\").\n"
    "0:0: unmatched \"}\".\n");
}

// -------------------------------------------------------------------
// message errors

test_f(parseerrortest, messagemissingname) {
  expecthaserrors(
    "message {}",
    "0:8: expected message name.\n");
}

test_f(parseerrortest, messagemissingbody) {
  expecthaserrors(
    "message testmessage;",
    "0:19: expected \"{\".\n");
}

test_f(parseerrortest, eofinmessage) {
  expecthaserrors(
    "message testmessage {",
    "0:21: reached end of input in message definition (missing '}').\n");
}

test_f(parseerrortest, missingfieldnumber) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional int32 foo;\n"
    "}\n",
    "1:20: missing field number.\n");
}

test_f(parseerrortest, expectedfieldnumber) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional int32 foo = ;\n"
    "}\n",
    "1:23: expected field number.\n");
}

test_f(parseerrortest, fieldnumberoutofrange) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional int32 foo = 0x100000000;\n"
    "}\n",
    "1:23: integer out of range.\n");
}

test_f(parseerrortest, missinglabel) {
  expecthaserrors(
    "message testmessage {\n"
    "  int32 foo = 1;\n"
    "}\n",
    "1:2: expected \"required\", \"optional\", or \"repeated\".\n");
}

test_f(parseerrortest, expectedoptionname) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [];\n"
    "}\n",
    "1:27: expected identifier.\n");
}

test_f(parseerrortest, nonextensionoptionnamebeginningwithdot) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [.foo=1];\n"
    "}\n",
    "1:27: expected identifier.\n");
}

test_f(parseerrortest, defaultvaluetypemismatch) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [default=true];\n"
    "}\n",
    "1:35: expected integer.\n");
}

test_f(parseerrortest, defaultvaluenotboolean) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional bool foo = 1 [default=blah];\n"
    "}\n",
    "1:33: expected \"true\" or \"false\".\n");
}

test_f(parseerrortest, defaultvaluenotstring) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional string foo = 1 [default=1];\n"
    "}\n",
    "1:35: expected string.\n");
}

test_f(parseerrortest, defaultvalueunsignednegative) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [default=-1];\n"
    "}\n",
    "1:36: unsigned field can't have negative default value.\n");
}

test_f(parseerrortest, defaultvaluetoolarge) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional int32  foo = 1 [default= 0x80000000];\n"
    "  optional int32  foo = 1 [default=-0x80000001];\n"
    "  optional uint32 foo = 1 [default= 0x100000000];\n"
    "  optional int64  foo = 1 [default= 0x80000000000000000];\n"
    "  optional int64  foo = 1 [default=-0x80000000000000001];\n"
    "  optional uint64 foo = 1 [default= 0x100000000000000000];\n"
    "}\n",
    "1:36: integer out of range.\n"
    "2:36: integer out of range.\n"
    "3:36: integer out of range.\n"
    "4:36: integer out of range.\n"
    "5:36: integer out of range.\n"
    "6:36: integer out of range.\n");
}

test_f(parseerrortest, enumvalueoutofrange) {
  expecthaserrors(
    "enum testenum {\n"
    "  hex_too_big   =  0x80000000;\n"
    "  hex_too_small = -0x80000001;\n"
    "  int_too_big   =  2147483648;\n"
    "  int_too_small = -2147483649;\n"
    "}\n",
    "1:19: integer out of range.\n"
    "2:19: integer out of range.\n"
    "3:19: integer out of range.\n"
    "4:19: integer out of range.\n");
}

test_f(parseerrortest, defaultvaluemissing) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [default=];\n"
    "}\n",
    "1:35: expected integer.\n");
}

test_f(parseerrortest, defaultvalueforgroup) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional group foo = 1 [default=blah] {}\n"
    "}\n",
    "1:34: messages can't have default values.\n");
}

test_f(parseerrortest, duplicatedefaultvalue) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional uint32 foo = 1 [default=1,default=2];\n"
    "}\n",
    "1:37: already set option \"default\".\n");
}

test_f(parseerrortest, groupnotcapitalized) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional group foo = 1 {}\n"
    "}\n",
    "1:17: group names must start with a capital letter.\n");
}

test_f(parseerrortest, groupmissingbody) {
  expecthaserrors(
    "message testmessage {\n"
    "  optional group foo = 1;\n"
    "}\n",
    "1:24: missing group body.\n");
}

test_f(parseerrortest, extendingprimitive) {
  expecthaserrors(
    "extend int32 { optional string foo = 4; }\n",
    "0:7: expected message type.\n");
}

test_f(parseerrortest, errorinextension) {
  expecthaserrors(
    "message foo { extensions 100 to 199; }\n"
    "extend foo { optional string foo; }\n",
    "1:32: missing field number.\n");
}

test_f(parseerrortest, multipleparseerrors) {
  // when a statement has a parse error, the parser should be able to continue
  // parsing at the next statement.
  expecthaserrors(
    "message testmessage {\n"
    "  optional int32 foo;\n"
    "  !invalid statement ending in a block { blah blah { blah } blah }\n"
    "  optional int32 bar = 3 {}\n"
    "}\n",
    "1:20: missing field number.\n"
    "2:2: expected \"required\", \"optional\", or \"repeated\".\n"
    "2:2: expected type name.\n"
    "3:25: expected \";\".\n");
}

test_f(parseerrortest, eofinaggregatevalue) {
  expecthaserrors(
      "option (fileopt) = { i:100\n",
      "1:0: unexpected end of stream while parsing aggregate value.\n");
}

// -------------------------------------------------------------------
// enum errors

test_f(parseerrortest, eofinenum) {
  expecthaserrors(
    "enum testenum {",
    "0:15: reached end of input in enum definition (missing '}').\n");
}

test_f(parseerrortest, enumvaluemissingnumber) {
  expecthaserrors(
    "enum testenum {\n"
    "  foo;\n"
    "}\n",
    "1:5: missing numeric value for enum constant.\n");
}

// -------------------------------------------------------------------
// service errors

test_f(parseerrortest, eofinservice) {
  expecthaserrors(
    "service testservice {",
    "0:21: reached end of input in service definition (missing '}').\n");
}

test_f(parseerrortest, servicemethodprimitiveparams) {
  expecthaserrors(
    "service testservice {\n"
    "  rpc foo(int32) returns (string);\n"
    "}\n",
    "1:10: expected message type.\n"
    "1:26: expected message type.\n");
}


test_f(parseerrortest, eofinmethodoptions) {
  expecthaserrors(
    "service testservice {\n"
    "  rpc foo(bar) returns(bar) {",
    "1:29: reached end of input in method options (missing '}').\n"
    "1:29: reached end of input in service definition (missing '}').\n");
}


test_f(parseerrortest, primitivemethodinput) {
  expecthaserrors(
    "service testservice {\n"
    "  rpc foo(int32) returns(bar);\n"
    "}\n",
    "1:10: expected message type.\n");
}


test_f(parseerrortest, methodoptiontypeerror) {
  // this used to cause an infinite loop.
  expecthaserrors(
    "message baz {}\n"
    "service foo {\n"
    "  rpc bar(baz) returns(baz) { option invalid syntax; }\n"
    "}\n",
    "2:45: expected \"=\".\n");
}


// -------------------------------------------------------------------
// import and package errors

test_f(parseerrortest, importnotquoted) {
  expecthaserrors(
    "import foo;\n",
    "0:7: expected a string naming the file to import.\n");
}

test_f(parseerrortest, multiplepackagesinfile) {
  expecthaserrors(
    "package foo;\n"
    "package bar;\n",
    "1:0: multiple package definitions.\n");
}

// ===================================================================
// test that errors detected by descriptorpool correctly report line and
// column numbers.  we have one test for every call to recordlocation() in
// parser.cc.

typedef parsertest parservalidationerrortest;

test_f(parservalidationerrortest, packagenameerror) {
  // create another file which defines symbol "foo".
  filedescriptorproto other_file;
  other_file.set_name("bar.proto");
  other_file.add_message_type()->set_name("foo");
  expect_true(pool_.buildfile(other_file) != null);

  // now try to define it as a package.
  expecthasvalidationerrors(
    "package foo.bar;",
    "0:8: \"foo\" is already defined (as something other than a package) "
      "in file \"bar.proto\".\n");
}

test_f(parservalidationerrortest, messagenameerror) {
  expecthasvalidationerrors(
    "message foo {}\n"
    "message foo {}\n",
    "1:8: \"foo\" is already defined.\n");
}

test_f(parservalidationerrortest, fieldnameerror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  optional int32 bar = 1;\n"
    "  optional int32 bar = 2;\n"
    "}\n",
    "2:17: \"bar\" is already defined in \"foo\".\n");
}

test_f(parservalidationerrortest, fieldtypeerror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  optional baz bar = 1;\n"
    "}\n",
    "1:11: \"baz\" is not defined.\n");
}

test_f(parservalidationerrortest, fieldnumbererror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  optional int32 bar = 0;\n"
    "}\n",
    "1:23: field numbers must be positive integers.\n");
}

test_f(parservalidationerrortest, fieldextendeeerror) {
  expecthasvalidationerrors(
    "extend baz { optional int32 bar = 1; }\n",
    "0:7: \"baz\" is not defined.\n");
}

test_f(parservalidationerrortest, fielddefaultvalueerror) {
  expecthasvalidationerrors(
    "enum baz { qux = 1; }\n"
    "message foo {\n"
    "  optional baz bar = 1 [default=no_such_value];\n"
    "}\n",
    "2:32: enum type \"baz\" has no value named \"no_such_value\".\n");
}

test_f(parservalidationerrortest, fileoptionnameerror) {
  expecthasvalidationerrors(
    "option foo = 5;",
    "0:7: option \"foo\" unknown.\n");
}

test_f(parservalidationerrortest, fileoptionvalueerror) {
  expecthasvalidationerrors(
    "option java_outer_classname = 5;",
    "0:30: value must be quoted string for string option "
    "\"google.protobuf.fileoptions.java_outer_classname\".\n");
}

test_f(parservalidationerrortest, fieldoptionnameerror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  optional bool bar = 1 [foo=1];\n"
    "}\n",
    "1:25: option \"foo\" unknown.\n");
}

test_f(parservalidationerrortest, fieldoptionvalueerror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  optional int32 bar = 1 [ctype=1];\n"
    "}\n",
    "1:32: value must be identifier for enum-valued option "
    "\"google.protobuf.fieldoptions.ctype\".\n");
}

test_f(parservalidationerrortest, extensionrangenumbererror) {
  expecthasvalidationerrors(
    "message foo {\n"
    "  extensions 0;\n"
    "}\n",
    "1:13: extension numbers must be positive integers.\n");
}

test_f(parservalidationerrortest, enumnameerror) {
  expecthasvalidationerrors(
    "enum foo {a = 1;}\n"
    "enum foo {b = 1;}\n",
    "1:5: \"foo\" is already defined.\n");
}

test_f(parservalidationerrortest, enumvaluenameerror) {
  expecthasvalidationerrors(
    "enum foo {\n"
    "  bar = 1;\n"
    "  bar = 1;\n"
    "}\n",
    "2:2: \"bar\" is already defined.\n");
}

test_f(parservalidationerrortest, servicenameerror) {
  expecthasvalidationerrors(
    "service foo {}\n"
    "service foo {}\n",
    "1:8: \"foo\" is already defined.\n");
}

test_f(parservalidationerrortest, methodnameerror) {
  expecthasvalidationerrors(
    "message baz {}\n"
    "service foo {\n"
    "  rpc bar(baz) returns(baz);\n"
    "  rpc bar(baz) returns(baz);\n"
    "}\n",
    "3:6: \"bar\" is already defined in \"foo\".\n");
}


test_f(parservalidationerrortest, methodinputtypeerror) {
  expecthasvalidationerrors(
    "message baz {}\n"
    "service foo {\n"
    "  rpc bar(qux) returns(baz);\n"
    "}\n",
    "2:10: \"qux\" is not defined.\n");
}


test_f(parservalidationerrortest, methodoutputtypeerror) {
  expecthasvalidationerrors(
    "message baz {}\n"
    "service foo {\n"
    "  rpc bar(baz) returns(qux);\n"
    "}\n",
    "2:23: \"qux\" is not defined.\n");
}


// ===================================================================
// test that the output from filedescriptor::debugstring() (and all other
// descriptor types) is parseable, and results in the same descriptor
// definitions again afoter parsing (not, however, that the order of messages
// cannot be guaranteed to be the same)

typedef parsertest parsedecriptordebugtest;

class comparedescriptornames {
 public:
  bool operator()(const descriptorproto* left, const descriptorproto* right) {
    return left->name() < right->name();
  }
};

// sorts nested descriptorprotos of a descriptoproto, by name.
void sortmessages(descriptorproto *descriptor_proto) {
  int size = descriptor_proto->nested_type_size();
  // recursively sort; we can't guarantee the order of nested messages either
  for (int i = 0; i < size; ++i) {
    sortmessages(descriptor_proto->mutable_nested_type(i));
  }
  descriptorproto **data =
    descriptor_proto->mutable_nested_type()->mutable_data();
  sort(data, data + size, comparedescriptornames());
}

// sorts descriptorprotos belonging to a filedescriptorproto, by name.
void sortmessages(filedescriptorproto *file_descriptor_proto) {
  int size = file_descriptor_proto->message_type_size();
  // recursively sort; we can't guarantee the order of nested messages either
  for (int i = 0; i < size; ++i) {
    sortmessages(file_descriptor_proto->mutable_message_type(i));
  }
  descriptorproto **data =
    file_descriptor_proto->mutable_message_type()->mutable_data();
  sort(data, data + size, comparedescriptornames());
}

test_f(parsedecriptordebugtest, testalldescriptortypes) {
  const filedescriptor* original_file =
     protobuf_unittest::testalltypes::descriptor()->file();
  filedescriptorproto expected;
  original_file->copyto(&expected);

  // get the debugstring of the unittest.proto filedecriptor, which includes
  // all other descriptor types
  string debug_string = original_file->debugstring();

  // parse the debug string
  setupparser(debug_string.c_str());
  filedescriptorproto parsed;
  parser_->parse(input_.get(), &parsed);
  expect_eq(io::tokenizer::type_end, input_->current().type);
  assert_eq("", error_collector_.text_);

  // we now have a filedescriptorproto, but to compare with the expected we
  // need to link to a filedecriptor, then output back to a proto. we'll
  // also need to give it the same name as the original.
  parsed.set_name("google/protobuf/unittest.proto");
  // we need the imported dependency before we can build our parsed proto
  const filedescriptor* public_import =
      protobuf_unittest_import::publicimportmessage::descriptor()->file();
  filedescriptorproto public_import_proto;
  public_import->copyto(&public_import_proto);
  assert_true(pool_.buildfile(public_import_proto) != null);
  const filedescriptor* import =
       protobuf_unittest_import::importmessage::descriptor()->file();
  filedescriptorproto import_proto;
  import->copyto(&import_proto);
  assert_true(pool_.buildfile(import_proto) != null);
  const filedescriptor* actual = pool_.buildfile(parsed);
  parsed.clear();
  actual->copyto(&parsed);
  assert_true(actual != null);

  // the messages might be in different orders, making them hard to compare.
  // so, sort the messages in the descriptor protos (including nested messages,
  // recursively).
  sortmessages(&expected);
  sortmessages(&parsed);

  // i really wanted to use stringdiff here for the debug output on fail,
  // but the strings are too long for it, and if i increase its max size,
  // we get a memory allocation failure :(
  expect_eq(expected.debugstring(), parsed.debugstring());
}

test_f(parsedecriptordebugtest, testcustomoptions) {
  const filedescriptor* original_file =
     protobuf_unittest::aggregatemessage::descriptor()->file();
  filedescriptorproto expected;
  original_file->copyto(&expected);

  string debug_string = original_file->debugstring();

  // parse the debug string
  setupparser(debug_string.c_str());
  filedescriptorproto parsed;
  parser_->parse(input_.get(), &parsed);
  expect_eq(io::tokenizer::type_end, input_->current().type);
  assert_eq("", error_collector_.text_);

  // we now have a filedescriptorproto, but to compare with the expected we
  // need to link to a filedecriptor, then output back to a proto. we'll
  // also need to give it the same name as the original.
  parsed.set_name(original_file->name());

  // unittest_custom_options.proto depends on descriptor.proto.
  const filedescriptor* import = filedescriptorproto::descriptor()->file();
  filedescriptorproto import_proto;
  import->copyto(&import_proto);
  assert_true(pool_.buildfile(import_proto) != null);
  const filedescriptor* actual = pool_.buildfile(parsed);
  assert_true(actual != null);
  parsed.clear();
  actual->copyto(&parsed);

  // the messages might be in different orders, making them hard to compare.
  // so, sort the messages in the descriptor protos (including nested messages,
  // recursively).
  sortmessages(&expected);
  sortmessages(&parsed);

  expect_eq(expected.debugstring(), parsed.debugstring());
}

// ===================================================================
// sourcecodeinfo tests.

// follows a path -- as defined by sourcecodeinfo.location.path -- from a
// message to a particular sub-field.
// * if the target is itself a message, sets *output_message to point at it,
//   *output_field to null, and *output_index to -1.
// * otherwise, if the target is an element of a repeated field, sets
//   *output_message to the containing message, *output_field to the descriptor
//   of the field, and *output_index to the index of the element.
// * otherwise, the target is a field (possibly a repeated field, but not any
//   one element).  sets *output_message to the containing message,
//   *output_field to the descriptor of the field, and *output_index to -1.
// returns true if the path was valid, false otherwise.  a gtest failure is
// recorded before returning false.
bool followpath(const message& root,
                const int* path_begin, const int* path_end,
                const message** output_message,
                const fielddescriptor** output_field,
                int* output_index) {
  if (path_begin == path_end) {
    // path refers to this whole message.
    *output_message = &root;
    *output_field = null;
    *output_index = -1;
    return true;
  }

  const descriptor* descriptor = root.getdescriptor();
  const reflection* reflection = root.getreflection();

  const fielddescriptor* field = descriptor->findfieldbynumber(*path_begin);

  if (field == null) {
    add_failure() << descriptor->name() << " has no field number: "
                  << *path_begin;
    return false;
  }

  ++path_begin;

  if (field->is_repeated()) {
    if (path_begin == path_end) {
      // path refers to the whole repeated field.
      *output_message = &root;
      *output_field = field;
      *output_index = -1;
      return true;
    }

    int index = *path_begin++;
    int size = reflection->fieldsize(root, field);

    if (index >= size) {
      add_failure() << descriptor->name() << "." << field->name()
                    << " has size " << size << ", but path contained index: "
                    << index;
      return false;
    }

    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      // descend into child message.
      const message& child = reflection->getrepeatedmessage(root, field, index);
      return followpath(child, path_begin, path_end,
                        output_message, output_field, output_index);
    } else if (path_begin == path_end) {
      // path refers to this element.
      *output_message = &root;
      *output_field = field;
      *output_index = index;
      return true;
    } else {
      add_failure() << descriptor->name() << "." << field->name()
                    << " is not a message; cannot descend into it.";
      return false;
    }
  } else {
    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      const message& child = reflection->getmessage(root, field);
      return followpath(child, path_begin, path_end,
                        output_message, output_field, output_index);
    } else if (path_begin == path_end) {
      // path refers to this field.
      *output_message = &root;
      *output_field = field;
      *output_index = -1;
      return true;
    } else {
      add_failure() << descriptor->name() << "." << field->name()
                    << " is not a message; cannot descend into it.";
      return false;
    }
  }
}

// check if two spans are equal.
bool comparespans(const repeatedfield<int>& span1,
                  const repeatedfield<int>& span2) {
  if (span1.size() != span2.size()) return false;
  for (int i = 0; i < span1.size(); i++) {
    if (span1.get(i) != span2.get(i)) return false;
  }
  return true;
}

// test fixture for source info tests, which check that source locations are
// recorded correctly in filedescriptorproto.source_code_info.location.
class sourceinfotest : public parsertest {
 protected:
  // the parsed file (initialized by parse()).
  filedescriptorproto file_;

  // parse the given text as a .proto file and populate the spans_ map with
  // all the source location spans in its sourcecodeinfo table.
  bool parse(const char* text) {
    extractmarkers(text);
    setupparser(text_without_markers_.c_str());
    if (!parser_->parse(input_.get(), &file_)) {
      return false;
    }

    const sourcecodeinfo& source_info = file_.source_code_info();
    for (int i = 0; i < source_info.location_size(); i++) {
      const sourcecodeinfo::location& location = source_info.location(i);
      const message* descriptor_proto = null;
      const fielddescriptor* field = null;
      int index = 0;
      if (!followpath(file_, location.path().begin(), location.path().end(),
                      &descriptor_proto, &field, &index)) {
        return false;
      }

      spans_.insert(make_pair(spankey(*descriptor_proto, field, index),
                              &location));
    }

    return true;
  }

  virtual void teardown() {
    expect_true(spans_.empty())
        << "forgot to call hasspan() for:\n"
        << spans_.begin()->second->debugstring();
  }

  // -----------------------------------------------------------------
  // hasspan() checks that the span of source code delimited by the given
  // tags (comments) correspond via the sourcecodeinfo table to the given
  // part of the filedescriptorproto.  (if unclear, look at the actual tests;
  // it should quickly become obvious.)

  bool hasspan(char start_marker, char end_marker,
               const message& descriptor_proto) {
    return hasspanwithcomment(
        start_marker, end_marker, descriptor_proto, null, -1, null, null);
  }

  bool hasspanwithcomment(char start_marker, char end_marker,
                          const message& descriptor_proto,
                          const char* expected_leading_comments,
                          const char* expected_trailing_comments) {
    return hasspanwithcomment(
        start_marker, end_marker, descriptor_proto, null, -1,
        expected_leading_comments, expected_trailing_comments);
  }

  bool hasspan(char start_marker, char end_marker,
               const message& descriptor_proto, const string& field_name) {
    return hasspan(start_marker, end_marker, descriptor_proto, field_name, -1);
  }

  bool hasspan(char start_marker, char end_marker,
               const message& descriptor_proto, const string& field_name,
               int index) {
    return hasspan(start_marker, end_marker, descriptor_proto,
                   field_name, index, null, null);
  }

  bool hasspan(char start_marker, char end_marker,
               const message& descriptor_proto,
               const string& field_name, int index,
               const char* expected_leading_comments,
               const char* expected_trailing_comments) {
    const fielddescriptor* field =
        descriptor_proto.getdescriptor()->findfieldbyname(field_name);
    if (field == null) {
      add_failure() << descriptor_proto.getdescriptor()->name()
                    << " has no such field: " << field_name;
      return false;
    }

    return hasspanwithcomment(
        start_marker, end_marker, descriptor_proto, field, index,
        expected_leading_comments, expected_trailing_comments);
  }

  bool hasspan(const message& descriptor_proto) {
    return hasspanwithcomment(
        '\0', '\0', descriptor_proto, null, -1, null, null);
  }

  bool hasspan(const message& descriptor_proto, const string& field_name) {
    return hasspan('\0', '\0', descriptor_proto, field_name, -1);
  }

  bool hasspan(const message& descriptor_proto, const string& field_name,
               int index) {
    return hasspan('\0', '\0', descriptor_proto, field_name, index);
  }

  bool hasspanwithcomment(char start_marker, char end_marker,
                          const message& descriptor_proto,
                          const fielddescriptor* field, int index,
                          const char* expected_leading_comments,
                          const char* expected_trailing_comments) {
    pair<spanmap::iterator, spanmap::iterator> range =
        spans_.equal_range(spankey(descriptor_proto, field, index));

    if (start_marker == '\0') {
      if (range.first == range.second) {
        return false;
      } else {
        spans_.erase(range.first);
        return true;
      }
    } else {
      pair<int, int> start_pos = findordie(markers_, start_marker);
      pair<int, int> end_pos = findordie(markers_, end_marker);

      repeatedfield<int> expected_span;
      expected_span.add(start_pos.first);
      expected_span.add(start_pos.second);
      if (end_pos.first != start_pos.first) {
        expected_span.add(end_pos.first);
      }
      expected_span.add(end_pos.second);

      for (spanmap::iterator iter = range.first; iter != range.second; ++iter) {
        if (comparespans(expected_span, iter->second->span())) {
          if (expected_leading_comments == null) {
            expect_false(iter->second->has_leading_comments());
          } else {
            expect_true(iter->second->has_leading_comments());
            expect_eq(expected_leading_comments,
                      iter->second->leading_comments());
          }
          if (expected_trailing_comments == null) {
            expect_false(iter->second->has_trailing_comments());
          } else {
            expect_true(iter->second->has_trailing_comments());
            expect_eq(expected_trailing_comments,
                      iter->second->trailing_comments());
          }

          spans_.erase(iter);
          return true;
        }
      }

      return false;
    }
  }

 private:
  struct spankey {
    const message* descriptor_proto;
    const fielddescriptor* field;
    int index;

    inline spankey() {}
    inline spankey(const message& descriptor_proto_param,
                   const fielddescriptor* field_param,
                   int index_param)
        : descriptor_proto(&descriptor_proto_param), field(field_param),
          index(index_param) {}

    inline bool operator<(const spankey& other) const {
      if (descriptor_proto < other.descriptor_proto) return true;
      if (descriptor_proto > other.descriptor_proto) return false;
      if (field < other.field) return true;
      if (field > other.field) return false;
      return index < other.index;
    }
  };

  typedef multimap<spankey, const sourcecodeinfo::location*> spanmap;
  spanmap spans_;
  map<char, pair<int, int> > markers_;
  string text_without_markers_;

  void extractmarkers(const char* text) {
    markers_.clear();
    text_without_markers_.clear();
    int line = 0;
    int column = 0;
    while (*text != '\0') {
      if (*text == '$') {
        ++text;
        google_check_ne('\0', *text);
        if (*text == '$') {
          text_without_markers_ += '$';
          ++column;
        } else {
          markers_[*text] = make_pair(line, column);
          ++text;
          google_check_eq('$', *text);
        }
      } else if (*text == '\n') {
        ++line;
        column = 0;
        text_without_markers_ += *text;
      } else {
        text_without_markers_ += *text;
        ++column;
      }
      ++text;
    }
  }
};

test_f(sourceinfotest, basicfiledecls) {
  expect_true(parse(
      "$a$syntax = \"proto2\";\n"
      "package $b$foo.bar$c$;\n"
      "import $d$\"baz.proto\"$e$;\n"
      "import $f$\"qux.proto\"$g$;$h$\n"
      "\n"
      "// comment ignored\n"));

  expect_true(hasspan('a', 'h', file_));
  expect_true(hasspan('b', 'c', file_, "package"));
  expect_true(hasspan('d', 'e', file_, "dependency", 0));
  expect_true(hasspan('f', 'g', file_, "dependency", 1));
}

test_f(sourceinfotest, messages) {
  expect_true(parse(
      "$a$message $b$foo$c$ {}$d$\n"
      "$e$message $f$bar$g$ {}$h$\n"));

  expect_true(hasspan('a', 'd', file_.message_type(0)));
  expect_true(hasspan('b', 'c', file_.message_type(0), "name"));
  expect_true(hasspan('e', 'h', file_.message_type(1)));
  expect_true(hasspan('f', 'g', file_.message_type(1), "name"));

  // ignore these.
  expect_true(hasspan(file_));
}

test_f(sourceinfotest, fields) {
  expect_true(parse(
      "message foo {\n"
      "  $a$optional$b$ $c$int32$d$ $e$bar$f$ = $g$1$h$;$i$\n"
      "  $j$repeated$k$ $l$x.y$m$ $n$baz$o$ = $p$2$q$;$r$\n"
      "}\n"));

  const fielddescriptorproto& field1 = file_.message_type(0).field(0);
  const fielddescriptorproto& field2 = file_.message_type(0).field(1);

  expect_true(hasspan('a', 'i', field1));
  expect_true(hasspan('a', 'b', field1, "label"));
  expect_true(hasspan('c', 'd', field1, "type"));
  expect_true(hasspan('e', 'f', field1, "name"));
  expect_true(hasspan('g', 'h', field1, "number"));

  expect_true(hasspan('j', 'r', field2));
  expect_true(hasspan('j', 'k', field2, "label"));
  expect_true(hasspan('l', 'm', field2, "type_name"));
  expect_true(hasspan('n', 'o', field2, "name"));
  expect_true(hasspan('p', 'q', field2, "number"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
}

test_f(sourceinfotest, extensions) {
  expect_true(parse(
      "$a$extend $b$foo$c$ {\n"
      "  $d$optional$e$ int32 bar = 1;$f$\n"
      "  $g$repeated$h$ x.y baz = 2;$i$\n"
      "}$j$\n"
      "$k$extend $l$bar$m$ {\n"
      "  $n$optional int32 qux = 1;$o$\n"
      "}$p$\n"));

  const fielddescriptorproto& field1 = file_.extension(0);
  const fielddescriptorproto& field2 = file_.extension(1);
  const fielddescriptorproto& field3 = file_.extension(2);

  expect_true(hasspan('a', 'j', file_, "extension"));
  expect_true(hasspan('k', 'p', file_, "extension"));

  expect_true(hasspan('d', 'f', field1));
  expect_true(hasspan('d', 'e', field1, "label"));
  expect_true(hasspan('b', 'c', field1, "extendee"));

  expect_true(hasspan('g', 'i', field2));
  expect_true(hasspan('g', 'h', field2, "label"));
  expect_true(hasspan('b', 'c', field2, "extendee"));

  expect_true(hasspan('n', 'o', field3));
  expect_true(hasspan('l', 'm', field3, "extendee"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(field1, "type"));
  expect_true(hasspan(field1, "name"));
  expect_true(hasspan(field1, "number"));
  expect_true(hasspan(field2, "type_name"));
  expect_true(hasspan(field2, "name"));
  expect_true(hasspan(field2, "number"));
  expect_true(hasspan(field3, "label"));
  expect_true(hasspan(field3, "type"));
  expect_true(hasspan(field3, "name"));
  expect_true(hasspan(field3, "number"));
}

test_f(sourceinfotest, nestedextensions) {
  expect_true(parse(
      "message message {\n"
      "  $a$extend $b$foo$c$ {\n"
      "    $d$optional$e$ int32 bar = 1;$f$\n"
      "    $g$repeated$h$ x.y baz = 2;$i$\n"
      "  }$j$\n"
      "  $k$extend $l$bar$m$ {\n"
      "    $n$optional int32 qux = 1;$o$\n"
      "  }$p$\n"
      "}\n"));

  const fielddescriptorproto& field1 = file_.message_type(0).extension(0);
  const fielddescriptorproto& field2 = file_.message_type(0).extension(1);
  const fielddescriptorproto& field3 = file_.message_type(0).extension(2);

  expect_true(hasspan('a', 'j', file_.message_type(0), "extension"));
  expect_true(hasspan('k', 'p', file_.message_type(0), "extension"));

  expect_true(hasspan('d', 'f', field1));
  expect_true(hasspan('d', 'e', field1, "label"));
  expect_true(hasspan('b', 'c', field1, "extendee"));

  expect_true(hasspan('g', 'i', field2));
  expect_true(hasspan('g', 'h', field2, "label"));
  expect_true(hasspan('b', 'c', field2, "extendee"));

  expect_true(hasspan('n', 'o', field3));
  expect_true(hasspan('l', 'm', field3, "extendee"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
  expect_true(hasspan(field1, "type"));
  expect_true(hasspan(field1, "name"));
  expect_true(hasspan(field1, "number"));
  expect_true(hasspan(field2, "type_name"));
  expect_true(hasspan(field2, "name"));
  expect_true(hasspan(field2, "number"));
  expect_true(hasspan(field3, "label"));
  expect_true(hasspan(field3, "type"));
  expect_true(hasspan(field3, "name"));
  expect_true(hasspan(field3, "number"));
}

test_f(sourceinfotest, extensionranges) {
  expect_true(parse(
      "message message {\n"
      "  $a$extensions $b$1$c$ to $d$4$e$, $f$6$g$;$h$\n"
      "  $i$extensions $j$8$k$ to $l$max$m$;$n$\n"
      "}\n"));

  const descriptorproto::extensionrange& range1 =
      file_.message_type(0).extension_range(0);
  const descriptorproto::extensionrange& range2 =
      file_.message_type(0).extension_range(1);
  const descriptorproto::extensionrange& range3 =
      file_.message_type(0).extension_range(2);

  expect_true(hasspan('a', 'h', file_.message_type(0), "extension_range"));
  expect_true(hasspan('i', 'n', file_.message_type(0), "extension_range"));

  expect_true(hasspan('b', 'e', range1));
  expect_true(hasspan('b', 'c', range1, "start"));
  expect_true(hasspan('d', 'e', range1, "end"));

  expect_true(hasspan('f', 'g', range2));
  expect_true(hasspan('f', 'g', range2, "start"));
  expect_true(hasspan('f', 'g', range2, "end"));

  expect_true(hasspan('j', 'm', range3));
  expect_true(hasspan('j', 'k', range3, "start"));
  expect_true(hasspan('l', 'm', range3, "end"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
}

test_f(sourceinfotest, nestedmessages) {
  expect_true(parse(
      "message foo {\n"
      "  $a$message $b$bar$c$ {\n"
      "    $d$message $e$baz$f$ {}$g$\n"
      "  }$h$\n"
      "  $i$message $j$qux$k$ {}$l$\n"
      "}\n"));

  const descriptorproto& bar = file_.message_type(0).nested_type(0);
  const descriptorproto& baz = bar.nested_type(0);
  const descriptorproto& qux = file_.message_type(0).nested_type(1);

  expect_true(hasspan('a', 'h', bar));
  expect_true(hasspan('b', 'c', bar, "name"));
  expect_true(hasspan('d', 'g', baz));
  expect_true(hasspan('e', 'f', baz, "name"));
  expect_true(hasspan('i', 'l', qux));
  expect_true(hasspan('j', 'k', qux, "name"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
}

test_f(sourceinfotest, groups) {
  expect_true(parse(
      "message foo {\n"
      "  message bar {}\n"
      "  $a$optional$b$ $c$group$d$ $e$baz$f$ = $g$1$h$ {\n"
      "    $i$message qux {}$j$\n"
      "  }$k$\n"
      "}\n"));

  const descriptorproto& bar = file_.message_type(0).nested_type(0);
  const descriptorproto& baz = file_.message_type(0).nested_type(1);
  const descriptorproto& qux = baz.nested_type(0);
  const fielddescriptorproto& field = file_.message_type(0).field(0);

  expect_true(hasspan('a', 'k', field));
  expect_true(hasspan('a', 'b', field, "label"));
  expect_true(hasspan('c', 'd', field, "type"));
  expect_true(hasspan('e', 'f', field, "name"));
  expect_true(hasspan('e', 'f', field, "type_name"));
  expect_true(hasspan('g', 'h', field, "number"));

  expect_true(hasspan('a', 'k', baz));
  expect_true(hasspan('e', 'f', baz, "name"));
  expect_true(hasspan('i', 'j', qux));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
  expect_true(hasspan(bar));
  expect_true(hasspan(bar, "name"));
  expect_true(hasspan(qux, "name"));
}

test_f(sourceinfotest, enums) {
  expect_true(parse(
      "$a$enum $b$foo$c$ {}$d$\n"
      "$e$enum $f$bar$g$ {}$h$\n"));

  expect_true(hasspan('a', 'd', file_.enum_type(0)));
  expect_true(hasspan('b', 'c', file_.enum_type(0), "name"));
  expect_true(hasspan('e', 'h', file_.enum_type(1)));
  expect_true(hasspan('f', 'g', file_.enum_type(1), "name"));

  // ignore these.
  expect_true(hasspan(file_));
}

test_f(sourceinfotest, enumvalues) {
  expect_true(parse(
      "enum foo {\n"
      "  $a$bar$b$ = $c$1$d$;$e$\n"
      "  $f$baz$g$ = $h$2$i$;$j$\n"
      "}"));

  const enumvaluedescriptorproto& bar = file_.enum_type(0).value(0);
  const enumvaluedescriptorproto& baz = file_.enum_type(0).value(1);

  expect_true(hasspan('a', 'e', bar));
  expect_true(hasspan('a', 'b', bar, "name"));
  expect_true(hasspan('c', 'd', bar, "number"));
  expect_true(hasspan('f', 'j', baz));
  expect_true(hasspan('f', 'g', baz, "name"));
  expect_true(hasspan('h', 'i', baz, "number"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.enum_type(0)));
  expect_true(hasspan(file_.enum_type(0), "name"));
}

test_f(sourceinfotest, nestedenums) {
  expect_true(parse(
      "message foo {\n"
      "  $a$enum $b$bar$c$ {}$d$\n"
      "  $e$enum $f$baz$g$ {}$h$\n"
      "}\n"));

  const enumdescriptorproto& bar = file_.message_type(0).enum_type(0);
  const enumdescriptorproto& baz = file_.message_type(0).enum_type(1);

  expect_true(hasspan('a', 'd', bar));
  expect_true(hasspan('b', 'c', bar, "name"));
  expect_true(hasspan('e', 'h', baz));
  expect_true(hasspan('f', 'g', baz, "name"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
}

test_f(sourceinfotest, services) {
  expect_true(parse(
      "$a$service $b$foo$c$ {}$d$\n"
      "$e$service $f$bar$g$ {}$h$\n"));

  expect_true(hasspan('a', 'd', file_.service(0)));
  expect_true(hasspan('b', 'c', file_.service(0), "name"));
  expect_true(hasspan('e', 'h', file_.service(1)));
  expect_true(hasspan('f', 'g', file_.service(1), "name"));

  // ignore these.
  expect_true(hasspan(file_));
}

test_f(sourceinfotest, methodsandstreams) {
  expect_true(parse(
      "service foo {\n"
      "  $a$rpc $b$bar$c$($d$x$e$) returns($f$y$g$);$h$"
      "  $i$rpc $j$baz$k$($l$z$m$) returns($n$w$o$);$p$"
      "}"));

  const methoddescriptorproto& bar = file_.service(0).method(0);
  const methoddescriptorproto& baz = file_.service(0).method(1);

  expect_true(hasspan('a', 'h', bar));
  expect_true(hasspan('b', 'c', bar, "name"));
  expect_true(hasspan('d', 'e', bar, "input_type"));
  expect_true(hasspan('f', 'g', bar, "output_type"));

  expect_true(hasspan('i', 'p', baz));
  expect_true(hasspan('j', 'k', baz, "name"));
  expect_true(hasspan('l', 'm', baz, "input_type"));
  expect_true(hasspan('n', 'o', baz, "output_type"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.service(0)));
  expect_true(hasspan(file_.service(0), "name"));
}

test_f(sourceinfotest, options) {
  expect_true(parse(
      "$a$option $b$foo$c$.$d$($e$bar.baz$f$)$g$ = "
          "$h$123$i$;$j$\n"
      "$k$option qux = $l$-123$m$;$n$\n"
      "$o$option corge = $p$abc$q$;$r$\n"
      "$s$option grault = $t$'blah'$u$;$v$\n"
      "$w$option garply = $x${ yadda yadda }$y$;$z$\n"
      "$0$option waldo = $1$123.0$2$;$3$\n"
  ));

  const uninterpretedoption& option1 = file_.options().uninterpreted_option(0);
  const uninterpretedoption& option2 = file_.options().uninterpreted_option(1);
  const uninterpretedoption& option3 = file_.options().uninterpreted_option(2);
  const uninterpretedoption& option4 = file_.options().uninterpreted_option(3);
  const uninterpretedoption& option5 = file_.options().uninterpreted_option(4);
  const uninterpretedoption& option6 = file_.options().uninterpreted_option(5);

  expect_true(hasspan('a', 'j', file_.options()));
  expect_true(hasspan('a', 'j', option1));
  expect_true(hasspan('b', 'g', option1, "name"));
  expect_true(hasspan('b', 'c', option1.name(0)));
  expect_true(hasspan('b', 'c', option1.name(0), "name_part"));
  expect_true(hasspan('d', 'g', option1.name(1)));
  expect_true(hasspan('e', 'f', option1.name(1), "name_part"));
  expect_true(hasspan('h', 'i', option1, "positive_int_value"));

  expect_true(hasspan('k', 'n', file_.options()));
  expect_true(hasspan('l', 'm', option2, "negative_int_value"));

  expect_true(hasspan('o', 'r', file_.options()));
  expect_true(hasspan('p', 'q', option3, "identifier_value"));

  expect_true(hasspan('s', 'v', file_.options()));
  expect_true(hasspan('t', 'u', option4, "string_value"));

  expect_true(hasspan('w', 'z', file_.options()));
  expect_true(hasspan('x', 'y', option5, "aggregate_value"));

  expect_true(hasspan('0', '3', file_.options()));
  expect_true(hasspan('1', '2', option6, "double_value"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(option2));
  expect_true(hasspan(option3));
  expect_true(hasspan(option4));
  expect_true(hasspan(option5));
  expect_true(hasspan(option6));
  expect_true(hasspan(option2, "name"));
  expect_true(hasspan(option3, "name"));
  expect_true(hasspan(option4, "name"));
  expect_true(hasspan(option5, "name"));
  expect_true(hasspan(option6, "name"));
  expect_true(hasspan(option2.name(0)));
  expect_true(hasspan(option3.name(0)));
  expect_true(hasspan(option4.name(0)));
  expect_true(hasspan(option5.name(0)));
  expect_true(hasspan(option6.name(0)));
  expect_true(hasspan(option2.name(0), "name_part"));
  expect_true(hasspan(option3.name(0), "name_part"));
  expect_true(hasspan(option4.name(0), "name_part"));
  expect_true(hasspan(option5.name(0), "name_part"));
  expect_true(hasspan(option6.name(0), "name_part"));
}

test_f(sourceinfotest, scopedoptions) {
  expect_true(parse(
    "message foo {\n"
    "  $a$option mopt = 1;$b$\n"
    "}\n"
    "enum bar {\n"
    "  $c$option eopt = 1;$d$\n"
    "}\n"
    "service baz {\n"
    "  $e$option sopt = 1;$f$\n"
    "  rpc m(x) returns(y) {\n"
    "    $g$option mopt = 1;$h$\n"
    "  }\n"
    "}\n"));

  expect_true(hasspan('a', 'b', file_.message_type(0).options()));
  expect_true(hasspan('c', 'd', file_.enum_type(0).options()));
  expect_true(hasspan('e', 'f', file_.service(0).options()));
  expect_true(hasspan('g', 'h', file_.service(0).method(0).options()));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
  expect_true(hasspan(file_.message_type(0).options()
                      .uninterpreted_option(0)));
  expect_true(hasspan(file_.message_type(0).options()
                      .uninterpreted_option(0), "name"));
  expect_true(hasspan(file_.message_type(0).options()
                      .uninterpreted_option(0).name(0)));
  expect_true(hasspan(file_.message_type(0).options()
                      .uninterpreted_option(0).name(0), "name_part"));
  expect_true(hasspan(file_.message_type(0).options()
                      .uninterpreted_option(0), "positive_int_value"));
  expect_true(hasspan(file_.enum_type(0)));
  expect_true(hasspan(file_.enum_type(0), "name"));
  expect_true(hasspan(file_.enum_type(0).options()
                      .uninterpreted_option(0)));
  expect_true(hasspan(file_.enum_type(0).options()
                      .uninterpreted_option(0), "name"));
  expect_true(hasspan(file_.enum_type(0).options()
                      .uninterpreted_option(0).name(0)));
  expect_true(hasspan(file_.enum_type(0).options()
                      .uninterpreted_option(0).name(0), "name_part"));
  expect_true(hasspan(file_.enum_type(0).options()
                      .uninterpreted_option(0), "positive_int_value"));
  expect_true(hasspan(file_.service(0)));
  expect_true(hasspan(file_.service(0), "name"));
  expect_true(hasspan(file_.service(0).method(0)));
  expect_true(hasspan(file_.service(0).options()
                      .uninterpreted_option(0)));
  expect_true(hasspan(file_.service(0).options()
                      .uninterpreted_option(0), "name"));
  expect_true(hasspan(file_.service(0).options()
                      .uninterpreted_option(0).name(0)));
  expect_true(hasspan(file_.service(0).options()
                      .uninterpreted_option(0).name(0), "name_part"));
  expect_true(hasspan(file_.service(0).options()
                      .uninterpreted_option(0), "positive_int_value"));
  expect_true(hasspan(file_.service(0).method(0), "name"));
  expect_true(hasspan(file_.service(0).method(0), "input_type"));
  expect_true(hasspan(file_.service(0).method(0), "output_type"));
  expect_true(hasspan(file_.service(0).method(0).options()
                      .uninterpreted_option(0)));
  expect_true(hasspan(file_.service(0).method(0).options()
                      .uninterpreted_option(0), "name"));
  expect_true(hasspan(file_.service(0).method(0).options()
                      .uninterpreted_option(0).name(0)));
  expect_true(hasspan(file_.service(0).method(0).options()
                      .uninterpreted_option(0).name(0), "name_part"));
  expect_true(hasspan(file_.service(0).method(0).options()
                      .uninterpreted_option(0), "positive_int_value"));
}

test_f(sourceinfotest, fieldoptions) {
  // the actual "name = value" pairs are parsed by the same code as for
  // top-level options so we won't re-test that -- just make sure that the
  // syntax used for field options is understood.
  expect_true(parse(
      "message foo {"
      "  optional int32 bar = 1 "
          "$a$[default=$b$123$c$,$d$opt1=123$e$,"
          "$f$opt2='hi'$g$]$h$;"
      "}\n"
  ));

  const fielddescriptorproto& field = file_.message_type(0).field(0);
  const uninterpretedoption& option1 = field.options().uninterpreted_option(0);
  const uninterpretedoption& option2 = field.options().uninterpreted_option(1);

  expect_true(hasspan('a', 'h', field.options()));
  expect_true(hasspan('b', 'c', field, "default_value"));
  expect_true(hasspan('d', 'e', option1));
  expect_true(hasspan('f', 'g', option2));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.message_type(0)));
  expect_true(hasspan(file_.message_type(0), "name"));
  expect_true(hasspan(field));
  expect_true(hasspan(field, "label"));
  expect_true(hasspan(field, "type"));
  expect_true(hasspan(field, "name"));
  expect_true(hasspan(field, "number"));
  expect_true(hasspan(option1, "name"));
  expect_true(hasspan(option2, "name"));
  expect_true(hasspan(option1.name(0)));
  expect_true(hasspan(option2.name(0)));
  expect_true(hasspan(option1.name(0), "name_part"));
  expect_true(hasspan(option2.name(0), "name_part"));
  expect_true(hasspan(option1, "positive_int_value"));
  expect_true(hasspan(option2, "string_value"));
}

test_f(sourceinfotest, enumvalueoptions) {
  // the actual "name = value" pairs are parsed by the same code as for
  // top-level options so we won't re-test that -- just make sure that the
  // syntax used for enum options is understood.
  expect_true(parse(
      "enum foo {"
      "  bar = 1 $a$[$b$opt1=123$c$,$d$opt2='hi'$e$]$f$;"
      "}\n"
  ));

  const enumvaluedescriptorproto& value = file_.enum_type(0).value(0);
  const uninterpretedoption& option1 = value.options().uninterpreted_option(0);
  const uninterpretedoption& option2 = value.options().uninterpreted_option(1);

  expect_true(hasspan('a', 'f', value.options()));
  expect_true(hasspan('b', 'c', option1));
  expect_true(hasspan('d', 'e', option2));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(file_.enum_type(0)));
  expect_true(hasspan(file_.enum_type(0), "name"));
  expect_true(hasspan(value));
  expect_true(hasspan(value, "name"));
  expect_true(hasspan(value, "number"));
  expect_true(hasspan(option1, "name"));
  expect_true(hasspan(option2, "name"));
  expect_true(hasspan(option1.name(0)));
  expect_true(hasspan(option2.name(0)));
  expect_true(hasspan(option1.name(0), "name_part"));
  expect_true(hasspan(option2.name(0), "name_part"));
  expect_true(hasspan(option1, "positive_int_value"));
  expect_true(hasspan(option2, "string_value"));
}

test_f(sourceinfotest, doccomments) {
  expect_true(parse(
      "// foo leading\n"
      "// line 2\n"
      "$a$message foo {\n"
      "  // foo trailing\n"
      "  // line 2\n"
      "\n"
      "  // ignored\n"
      "\n"
      "  // bar leading\n"
      "  $b$optional int32 bar = 1;$c$\n"
      "  // bar trailing\n"
      "}$d$\n"
      "// ignored\n"
  ));

  const descriptorproto& foo = file_.message_type(0);
  const fielddescriptorproto& bar = foo.field(0);

  expect_true(hasspanwithcomment('a', 'd', foo,
      " foo leading\n line 2\n",
      " foo trailing\n line 2\n"));
  expect_true(hasspanwithcomment('b', 'c', bar,
      " bar leading\n",
      " bar trailing\n"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(foo, "name"));
  expect_true(hasspan(bar, "label"));
  expect_true(hasspan(bar, "type"));
  expect_true(hasspan(bar, "name"));
  expect_true(hasspan(bar, "number"));
}

test_f(sourceinfotest, doccomments2) {
  expect_true(parse(
      "// ignored\n"
      "syntax = \"proto2\";\n"
      "// foo leading\n"
      "// line 2\n"
      "$a$message foo {\n"
      "  /* foo trailing\n"
      "   * line 2 */\n"
      "  // ignored\n"
      "  /* bar leading\n"
      "   */"
      "  $b$optional int32 bar = 1;$c$  // bar trailing\n"
      "  // ignored\n"
      "}$d$\n"
      "// ignored\n"
      "\n"
      "// option leading\n"
      "$e$option baz = 123;$f$\n"
      "// option trailing\n"
  ));

  const descriptorproto& foo = file_.message_type(0);
  const fielddescriptorproto& bar = foo.field(0);
  const uninterpretedoption& baz = file_.options().uninterpreted_option(0);

  expect_true(hasspanwithcomment('a', 'd', foo,
      " foo leading\n line 2\n",
      " foo trailing\n line 2 "));
  expect_true(hasspanwithcomment('b', 'c', bar,
      " bar leading\n",
      " bar trailing\n"));
  expect_true(hasspanwithcomment('e', 'f', baz,
      " option leading\n",
      " option trailing\n"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(foo, "name"));
  expect_true(hasspan(bar, "label"));
  expect_true(hasspan(bar, "type"));
  expect_true(hasspan(bar, "name"));
  expect_true(hasspan(bar, "number"));
  expect_true(hasspan(file_.options()));
  expect_true(hasspan(baz, "name"));
  expect_true(hasspan(baz.name(0)));
  expect_true(hasspan(baz.name(0), "name_part"));
  expect_true(hasspan(baz, "positive_int_value"));
}

test_f(sourceinfotest, doccomments3) {
  expect_true(parse(
      "$a$message foo {\n"
      "  // bar leading\n"
      "  $b$optional int32 bar = 1 [(baz.qux) = {}];$c$\n"
      "  // bar trailing\n"
      "}$d$\n"
      "// ignored\n"
  ));

  const descriptorproto& foo = file_.message_type(0);
  const fielddescriptorproto& bar = foo.field(0);

  expect_true(hasspanwithcomment('b', 'c', bar,
      " bar leading\n",
      " bar trailing\n"));

  // ignore these.
  expect_true(hasspan(file_));
  expect_true(hasspan(foo));
  expect_true(hasspan(foo, "name"));
  expect_true(hasspan(bar, "label"));
  expect_true(hasspan(bar, "type"));
  expect_true(hasspan(bar, "name"));
  expect_true(hasspan(bar, "number"));
  expect_true(hasspan(bar.options()));
  expect_true(hasspan(bar.options().uninterpreted_option(0)));
  expect_true(hasspan(bar.options().uninterpreted_option(0), "name"));
  expect_true(hasspan(bar.options().uninterpreted_option(0).name(0)));
  expect_true(hasspan(
      bar.options().uninterpreted_option(0).name(0), "name_part"));
  expect_true(hasspan(
      bar.options().uninterpreted_option(0), "aggregate_value"));
}

// ===================================================================

}  // anonymous namespace

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
