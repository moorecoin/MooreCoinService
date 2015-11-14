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

#include <google/protobuf/message.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _msc_ver
#include <io.h>
#else
#include <unistd.h>
#endif
#include <sstream>
#include <fstream>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/test_util.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {

#ifndef o_binary
#ifdef _o_binary
#define o_binary _o_binary
#else
#define o_binary 0     // if this isn't defined, the platform doesn't need it.
#endif
#endif

test(messagetest, serializehelpers) {
  // todo(kenton):  test more helpers?  they're all two-liners so it seems
  //   like a waste of time.

  protobuf_unittest::testalltypes message;
  testutil::setallfields(&message);
  stringstream stream;

  string str1("foo");
  string str2("bar");

  expect_true(message.serializetostring(&str1));
  expect_true(message.appendtostring(&str2));
  expect_true(message.serializetoostream(&stream));

  expect_eq(str1.size() + 3, str2.size());
  expect_eq("bar", str2.substr(0, 3));
  // don't use expect_eq because we don't want to dump raw binary data to
  // stdout.
  expect_true(str2.substr(3) == str1);

  // gcc gives some sort of error if we try to just do stream.str() == str1.
  string temp = stream.str();
  expect_true(temp == str1);

  expect_true(message.serializeasstring() == str1);

}

test(messagetest, serializetobrokenostream) {
  ofstream out;
  protobuf_unittest::testalltypes message;
  message.set_optional_int32(123);

  expect_false(message.serializetoostream(&out));
}

test(messagetest, parsefromfiledescriptor) {
  string filename = testsourcedir() +
                    "/google/protobuf/testdata/golden_message";
  int file = open(filename.c_str(), o_rdonly | o_binary);

  unittest::testalltypes message;
  expect_true(message.parsefromfiledescriptor(file));
  testutil::expectallfieldsset(message);

  expect_ge(close(file), 0);
}

test(messagetest, parsepackedfromfiledescriptor) {
  string filename =
      testsourcedir() +
      "/google/protobuf/testdata/golden_packed_fields_message";
  int file = open(filename.c_str(), o_rdonly | o_binary);

  unittest::testpackedtypes message;
  expect_true(message.parsefromfiledescriptor(file));
  testutil::expectpackedfieldsset(message);

  expect_ge(close(file), 0);
}

test(messagetest, parsehelpers) {
  // todo(kenton):  test more helpers?  they're all two-liners so it seems
  //   like a waste of time.
  string data;

  {
    // set up.
    protobuf_unittest::testalltypes message;
    testutil::setallfields(&message);
    message.serializetostring(&data);
  }

  {
    // test parsefromstring.
    protobuf_unittest::testalltypes message;
    expect_true(message.parsefromstring(data));
    testutil::expectallfieldsset(message);
  }

  {
    // test parsefromistream.
    protobuf_unittest::testalltypes message;
    stringstream stream(data);
    expect_true(message.parsefromistream(&stream));
    expect_true(stream.eof());
    testutil::expectallfieldsset(message);
  }

  {
    // test parsefromboundedzerocopystream.
    string data_with_junk(data);
    data_with_junk.append("some junk on the end");
    io::arrayinputstream stream(data_with_junk.data(), data_with_junk.size());
    protobuf_unittest::testalltypes message;
    expect_true(message.parsefromboundedzerocopystream(&stream, data.size()));
    testutil::expectallfieldsset(message);
  }

  {
    // test that parsefromboundedzerocopystream fails (but doesn't crash) if
    // eof is reached before the expected number of bytes.
    io::arrayinputstream stream(data.data(), data.size());
    protobuf_unittest::testalltypes message;
    expect_false(
      message.parsefromboundedzerocopystream(&stream, data.size() + 1));
  }
}

test(messagetest, parsefailsifnotinitialized) {
  unittest::testrequired message;
  vector<string> errors;

  {
    scopedmemorylog log;
    expect_false(message.parsefromstring(""));
    errors = log.getmessages(error);
  }

  assert_eq(1, errors.size());
  expect_eq("can't parse message of type \"protobuf_unittest.testrequired\" "
            "because it is missing required fields: a, b, c",
            errors[0]);
}

test(messagetest, bypassinitializationcheckonparse) {
  unittest::testrequired message;
  io::arrayinputstream raw_input(null, 0);
  io::codedinputstream input(&raw_input);
  expect_true(message.mergepartialfromcodedstream(&input));
}

test(messagetest, initializationerrorstring) {
  unittest::testrequired message;
  expect_eq("a, b, c", message.initializationerrorstring());
}

#ifdef protobuf_has_death_test

test(messagetest, serializefailsifnotinitialized) {
  unittest::testrequired message;
  string data;
  expect_debug_death(expect_true(message.serializetostring(&data)),
    "can't serialize message of type \"protobuf_unittest.testrequired\" because "
    "it is missing required fields: a, b, c");
}

test(messagetest, checkinitialized) {
  unittest::testrequired message;
  expect_death(message.checkinitialized(),
    "message of type \"protobuf_unittest.testrequired\" is missing required "
    "fields: a, b, c");
}

#endif  // protobuf_has_death_test

test(messagetest, bypassinitializationcheckonserialize) {
  unittest::testrequired message;
  io::arrayoutputstream raw_output(null, 0);
  io::codedoutputstream output(&raw_output);
  expect_true(message.serializepartialtocodedstream(&output));
}

test(messagetest, findinitializationerrors) {
  unittest::testrequired message;
  vector<string> errors;
  message.findinitializationerrors(&errors);
  assert_eq(3, errors.size());
  expect_eq("a", errors[0]);
  expect_eq("b", errors[1]);
  expect_eq("c", errors[2]);
}

test(messagetest, parsefailsoninvalidmessageend) {
  unittest::testalltypes message;

  // control case.
  expect_true(message.parsefromarray("", 0));

  // the byte is a valid varint, but not a valid tag (zero).
  expect_false(message.parsefromarray("\0", 1));

  // the byte is a malformed varint.
  expect_false(message.parsefromarray("\200", 1));

  // the byte is an endgroup tag, but we aren't parsing a group.
  expect_false(message.parsefromarray("\014", 1));
}

namespace {

void expectmessagemerged(const unittest::testalltypes& message) {
  expect_eq(3, message.optional_int32());
  expect_eq(2, message.optional_int64());
  expect_eq("hello", message.optional_string());
}

void assignparsingmergemessages(
    unittest::testalltypes* msg1,
    unittest::testalltypes* msg2,
    unittest::testalltypes* msg3) {
  msg1->set_optional_int32(1);
  msg2->set_optional_int64(2);
  msg3->set_optional_int32(3);
  msg3->set_optional_string("hello");
}

}  // namespace

// test that if an optional or required message/group field appears multiple
// times in the input, they need to be merged.
test(messagetest, parsingmerge) {
  unittest::testparsingmerge::repeatedfieldsgenerator generator;
  unittest::testalltypes* msg1;
  unittest::testalltypes* msg2;
  unittest::testalltypes* msg3;

#define assign_repeated_field(field)                \
  msg1 = generator.add_##field();                   \
  msg2 = generator.add_##field();                   \
  msg3 = generator.add_##field();                   \
  assignparsingmergemessages(msg1, msg2, msg3)

  assign_repeated_field(field1);
  assign_repeated_field(field2);
  assign_repeated_field(field3);
  assign_repeated_field(ext1);
  assign_repeated_field(ext2);

#undef assign_repeated_field
#define assign_repeated_group(field)                \
  msg1 = generator.add_##field()->mutable_field1(); \
  msg2 = generator.add_##field()->mutable_field1(); \
  msg3 = generator.add_##field()->mutable_field1(); \
  assignparsingmergemessages(msg1, msg2, msg3)

  assign_repeated_group(group1);
  assign_repeated_group(group2);

#undef assign_repeated_group

  string buffer;
  generator.serializetostring(&buffer);
  unittest::testparsingmerge parsing_merge;
  parsing_merge.parsefromstring(buffer);

  // required and optional fields should be merged.
  expectmessagemerged(parsing_merge.required_all_types());
  expectmessagemerged(parsing_merge.optional_all_types());
  expectmessagemerged(
      parsing_merge.optionalgroup().optional_group_all_types());
  expectmessagemerged(
      parsing_merge.getextension(unittest::testparsingmerge::optional_ext));

  // repeated fields should not be merged.
  expect_eq(3, parsing_merge.repeated_all_types_size());
  expect_eq(3, parsing_merge.repeatedgroup_size());
  expect_eq(3, parsing_merge.extensionsize(
      unittest::testparsingmerge::repeated_ext));
}

test(messagefactorytest, generatedfactorylookup) {
  expect_eq(
    messagefactory::generated_factory()->getprototype(
      protobuf_unittest::testalltypes::descriptor()),
    &protobuf_unittest::testalltypes::default_instance());
}

test(messagefactorytest, generatedfactoryunknowntype) {
  // construct a new descriptor.
  descriptorpool pool;
  filedescriptorproto file;
  file.set_name("foo.proto");
  file.add_message_type()->set_name("foo");
  const descriptor* descriptor = pool.buildfile(file)->message_type(0);

  // trying to construct it should return null.
  expect_true(
    messagefactory::generated_factory()->getprototype(descriptor) == null);
}


}  // namespace protobuf
}  // namespace google
