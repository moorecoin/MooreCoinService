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
// to test the code generator, we actually use it to generate code for
// google/protobuf/unittest.proto, then test that.  this means that we
// are actually testing the parser and other parts of the system at the same
// time, and that problems in the generator may show up as compile-time errors
// rather than unittest failures, which may be surprising.  however, testing
// the output of the c++ generator directly would be very hard.  we can't very
// well just check it against golden files since those files would have to be
// updated for any small change; such a test would be very brittle and probably
// not very helpful.  what we really want to test is that the code compiles
// correctly and produces the interfaces we expect, which is why this test
// is written this way.

#include <google/protobuf/compiler/cpp/cpp_unittest.h>

#include <vector>

#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/unittest_optimize_for.pb.h>
#include <google/protobuf/unittest_embed_optimize_for.pb.h>
#include <google/protobuf/unittest_no_generic_services.pb.h>
#include <google/protobuf/test_util.h>
#include <google/protobuf/compiler/cpp/cpp_test_bad_identifiers.pb.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// can't use an anonymous namespace here due to brokenness of tru64 compiler.
namespace cpp_unittest {

namespace protobuf_unittest = ::protobuf_unittest;


class mockerrorcollector : public multifileerrorcollector {
 public:
  mockerrorcollector() {}
  ~mockerrorcollector() {}

  string text_;

  // implements errorcollector ---------------------------------------
  void adderror(const string& filename, int line, int column,
                const string& message) {
    strings::substituteandappend(&text_, "$0:$1:$2: $3\n",
                                 filename, line, column, message);
  }
};

#ifndef protobuf_test_no_descriptors

// test that generated code has proper descriptors:
// parse a descriptor directly (using google::protobuf::compiler::importer) and
// compare it to the one that was produced by generated code.
test(generateddescriptortest, identicaldescriptors) {
  const filedescriptor* generated_descriptor =
    unittest::testalltypes::descriptor()->file();

  // set up the importer.
  mockerrorcollector error_collector;
  disksourcetree source_tree;
  source_tree.mappath("", testsourcedir());
  importer importer(&source_tree, &error_collector);

  // import (parse) unittest.proto.
  const filedescriptor* parsed_descriptor =
    importer.import("google/protobuf/unittest.proto");
  expect_eq("", error_collector.text_);
  assert_true(parsed_descriptor != null);

  // test that descriptors are generated correctly by converting them to
  // filedescriptorprotos and comparing.
  filedescriptorproto generated_decsriptor_proto, parsed_descriptor_proto;
  generated_descriptor->copyto(&generated_decsriptor_proto);
  parsed_descriptor->copyto(&parsed_descriptor_proto);

  expect_eq(parsed_descriptor_proto.debugstring(),
            generated_decsriptor_proto.debugstring());
}

#endif  // !protobuf_test_no_descriptors

// ===================================================================

test(generatedmessagetest, defaults) {
  // check that all default values are set correctly in the initial message.
  unittest::testalltypes message;

  testutil::expectclear(message);

  // messages should return pointers to default instances until first use.
  // (this is not checked by expectclear() since it is not actually true after
  // the fields have been set and then cleared.)
  expect_eq(&unittest::testalltypes::optionalgroup::default_instance(),
            &message.optionalgroup());
  expect_eq(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.optional_nested_message());
  expect_eq(&unittest::foreignmessage::default_instance(),
            &message.optional_foreign_message());
  expect_eq(&unittest_import::importmessage::default_instance(),
            &message.optional_import_message());
}

test(generatedmessagetest, floatingpointdefaults) {
  const unittest::testextremedefaultvalues& extreme_default =
      unittest::testextremedefaultvalues::default_instance();

  expect_eq(0.0f, extreme_default.zero_float());
  expect_eq(1.0f, extreme_default.one_float());
  expect_eq(1.5f, extreme_default.small_float());
  expect_eq(-1.0f, extreme_default.negative_one_float());
  expect_eq(-1.5f, extreme_default.negative_float());
  expect_eq(2.0e8f, extreme_default.large_float());
  expect_eq(-8e-28f, extreme_default.small_negative_float());
  expect_eq(numeric_limits<double>::infinity(),
            extreme_default.inf_double());
  expect_eq(-numeric_limits<double>::infinity(),
            extreme_default.neg_inf_double());
  expect_true(extreme_default.nan_double() != extreme_default.nan_double());
  expect_eq(numeric_limits<float>::infinity(),
            extreme_default.inf_float());
  expect_eq(-numeric_limits<float>::infinity(),
            extreme_default.neg_inf_float());
  expect_true(extreme_default.nan_float() != extreme_default.nan_float());
}

test(generatedmessagetest, trigraph) {
  const unittest::testextremedefaultvalues& extreme_default =
      unittest::testextremedefaultvalues::default_instance();

  expect_eq("? ? ?? ?? ??? ?\?/ ?\?-", extreme_default.cpp_trigraph());
}

test(generatedmessagetest, extremesmallintegerdefault) {
  const unittest::testextremedefaultvalues& extreme_default =
      unittest::testextremedefaultvalues::default_instance();
  expect_eq(-0x80000000, kint32min);
  expect_eq(google_longlong(-0x8000000000000000), kint64min);
  expect_eq(kint32min, extreme_default.really_small_int32());
  expect_eq(kint64min, extreme_default.really_small_int64());
}

test(generatedmessagetest, accessors) {
  // set every field to a unique value then go back and check all those
  // values.
  unittest::testalltypes message;

  testutil::setallfields(&message);
  testutil::expectallfieldsset(message);

  testutil::modifyrepeatedfields(&message);
  testutil::expectrepeatedfieldsmodified(message);
}

test(generatedmessagetest, mutablestringdefault) {
  // mutable_foo() for a string should return a string initialized to its
  // default value.
  unittest::testalltypes message;

  expect_eq("hello", *message.mutable_default_string());

  // note that the first time we call mutable_foo(), we get a newly-allocated
  // string, but if we clear it and call it again, we get the same object again.
  // we should verify that it has its default value in both cases.
  message.set_default_string("blah");
  message.clear();

  expect_eq("hello", *message.mutable_default_string());
}

test(generatedmessagetest, stringdefaults) {
  unittest::testextremedefaultvalues message;
  // check if '\000' can be used in default string value.
  expect_eq(string("hel\000lo", 6), message.string_with_zero());
  expect_eq(string("wor\000ld", 6), message.bytes_with_zero());
}

test(generatedmessagetest, releasestring) {
  // check that release_foo() starts out null, and gives us a value
  // that we can delete after it's been set.
  unittest::testalltypes message;

  expect_eq(null, message.release_default_string());
  expect_false(message.has_default_string());
  expect_eq("hello", message.default_string());

  message.set_default_string("blah");
  expect_true(message.has_default_string());
  string* str = message.release_default_string();
  expect_false(message.has_default_string());
  assert_true(str != null);
  expect_eq("blah", *str);
  delete str;

  expect_eq(null, message.release_default_string());
  expect_false(message.has_default_string());
  expect_eq("hello", message.default_string());
}

test(generatedmessagetest, releasemessage) {
  // check that release_foo() starts out null, and gives us a value
  // that we can delete after it's been set.
  unittest::testalltypes message;

  expect_eq(null, message.release_optional_nested_message());
  expect_false(message.has_optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  unittest::testalltypes::nestedmessage* nest =
      message.release_optional_nested_message();
  expect_false(message.has_optional_nested_message());
  assert_true(nest != null);
  expect_eq(1, nest->bb());
  delete nest;

  expect_eq(null, message.release_optional_nested_message());
  expect_false(message.has_optional_nested_message());
}

test(generatedmessagetest, setallocatedstring) {
  // check that set_allocated_foo() works for strings.
  unittest::testalltypes message;

  expect_false(message.has_optional_string());
  const string khello("hello");
  message.set_optional_string(khello);
  expect_true(message.has_optional_string());

  message.set_allocated_optional_string(null);
  expect_false(message.has_optional_string());
  expect_eq("", message.optional_string());

  message.set_allocated_optional_string(new string(khello));
  expect_true(message.has_optional_string());
  expect_eq(khello, message.optional_string());
}

test(generatedmessagetest, setallocatedmessage) {
  // check that set_allocated_foo() can be called in all cases.
  unittest::testalltypes message;

  expect_false(message.has_optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  expect_true(message.has_optional_nested_message());

  message.set_allocated_optional_nested_message(null);
  expect_false(message.has_optional_nested_message());
  expect_eq(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.optional_nested_message());

  message.mutable_optional_nested_message()->set_bb(1);
  unittest::testalltypes::nestedmessage* nest =
      message.release_optional_nested_message();
  assert_true(nest != null);
  expect_false(message.has_optional_nested_message());

  message.set_allocated_optional_nested_message(nest);
  expect_true(message.has_optional_nested_message());
  expect_eq(1, message.optional_nested_message().bb());
}

test(generatedmessagetest, clear) {
  // set every field to a unique value, clear the message, then check that
  // it is cleared.
  unittest::testalltypes message;

  testutil::setallfields(&message);
  message.clear();
  testutil::expectclear(message);

  // unlike with the defaults test, we do not expect that requesting embedded
  // messages will return a pointer to the default instance.  instead, they
  // should return the objects that were created when mutable_blah() was
  // called.
  expect_ne(&unittest::testalltypes::optionalgroup::default_instance(),
            &message.optionalgroup());
  expect_ne(&unittest::testalltypes::nestedmessage::default_instance(),
            &message.optional_nested_message());
  expect_ne(&unittest::foreignmessage::default_instance(),
            &message.optional_foreign_message());
  expect_ne(&unittest_import::importmessage::default_instance(),
            &message.optional_import_message());
}

test(generatedmessagetest, embeddednullsinbytescharstar) {
  unittest::testalltypes message;

  const char* value = "\0lalala\0\0";
  message.set_optional_bytes(value, 9);
  assert_eq(9, message.optional_bytes().size());
  expect_eq(0, memcmp(value, message.optional_bytes().data(), 9));

  message.add_repeated_bytes(value, 9);
  assert_eq(9, message.repeated_bytes(0).size());
  expect_eq(0, memcmp(value, message.repeated_bytes(0).data(), 9));
}

test(generatedmessagetest, clearonefield) {
  // set every field to a unique value, then clear one value and insure that
  // only that one value is cleared.
  unittest::testalltypes message;

  testutil::setallfields(&message);
  int64 original_value = message.optional_int64();

  // clear the field and make sure it shows up as cleared.
  message.clear_optional_int64();
  expect_false(message.has_optional_int64());
  expect_eq(0, message.optional_int64());

  // other adjacent fields should not be cleared.
  expect_true(message.has_optional_int32());
  expect_true(message.has_optional_uint32());

  // make sure if we set it again, then all fields are set.
  message.set_optional_int64(original_value);
  testutil::expectallfieldsset(message);
}

test(generatedmessagetest, stringcharstarlength) {
  // verify that we can use a char*,length to set one of the string fields.
  unittest::testalltypes message;
  message.set_optional_string("abcdef", 3);
  expect_eq("abc", message.optional_string());

  // verify that we can use a char*,length to add to a repeated string field.
  message.add_repeated_string("abcdef", 3);
  expect_eq(1, message.repeated_string_size());
  expect_eq("abc", message.repeated_string(0));

  // verify that we can use a char*,length to set a repeated string field.
  message.set_repeated_string(0, "wxyz", 2);
  expect_eq("wx", message.repeated_string(0));
}

test(generatedmessagetest, copyfrom) {
  unittest::testalltypes message1, message2;

  testutil::setallfields(&message1);
  message2.copyfrom(message1);
  testutil::expectallfieldsset(message2);

  // copying from self should be a no-op.
  message2.copyfrom(message2);
  testutil::expectallfieldsset(message2);
}

test(generatedmessagetest, swapwithempty) {
  unittest::testalltypes message1, message2;
  testutil::setallfields(&message1);

  testutil::expectallfieldsset(message1);
  testutil::expectclear(message2);
  message1.swap(&message2);
  testutil::expectallfieldsset(message2);
  testutil::expectclear(message1);
}

test(generatedmessagetest, swapwithself) {
  unittest::testalltypes message;
  testutil::setallfields(&message);
  testutil::expectallfieldsset(message);
  message.swap(&message);
  testutil::expectallfieldsset(message);
}

test(generatedmessagetest, swapwithother) {
  unittest::testalltypes message1, message2;

  message1.set_optional_int32(123);
  message1.set_optional_string("abc");
  message1.mutable_optional_nested_message()->set_bb(1);
  message1.set_optional_nested_enum(unittest::testalltypes::foo);
  message1.add_repeated_int32(1);
  message1.add_repeated_int32(2);
  message1.add_repeated_string("a");
  message1.add_repeated_string("b");
  message1.add_repeated_nested_message()->set_bb(7);
  message1.add_repeated_nested_message()->set_bb(8);
  message1.add_repeated_nested_enum(unittest::testalltypes::foo);
  message1.add_repeated_nested_enum(unittest::testalltypes::bar);

  message2.set_optional_int32(456);
  message2.set_optional_string("def");
  message2.mutable_optional_nested_message()->set_bb(2);
  message2.set_optional_nested_enum(unittest::testalltypes::bar);
  message2.add_repeated_int32(3);
  message2.add_repeated_string("c");
  message2.add_repeated_nested_message()->set_bb(9);
  message2.add_repeated_nested_enum(unittest::testalltypes::baz);

  message1.swap(&message2);

  expect_eq(456, message1.optional_int32());
  expect_eq("def", message1.optional_string());
  expect_eq(2, message1.optional_nested_message().bb());
  expect_eq(unittest::testalltypes::bar, message1.optional_nested_enum());
  assert_eq(1, message1.repeated_int32_size());
  expect_eq(3, message1.repeated_int32(0));
  assert_eq(1, message1.repeated_string_size());
  expect_eq("c", message1.repeated_string(0));
  assert_eq(1, message1.repeated_nested_message_size());
  expect_eq(9, message1.repeated_nested_message(0).bb());
  assert_eq(1, message1.repeated_nested_enum_size());
  expect_eq(unittest::testalltypes::baz, message1.repeated_nested_enum(0));

  expect_eq(123, message2.optional_int32());
  expect_eq("abc", message2.optional_string());
  expect_eq(1, message2.optional_nested_message().bb());
  expect_eq(unittest::testalltypes::foo, message2.optional_nested_enum());
  assert_eq(2, message2.repeated_int32_size());
  expect_eq(1, message2.repeated_int32(0));
  expect_eq(2, message2.repeated_int32(1));
  assert_eq(2, message2.repeated_string_size());
  expect_eq("a", message2.repeated_string(0));
  expect_eq("b", message2.repeated_string(1));
  assert_eq(2, message2.repeated_nested_message_size());
  expect_eq(7, message2.repeated_nested_message(0).bb());
  expect_eq(8, message2.repeated_nested_message(1).bb());
  assert_eq(2, message2.repeated_nested_enum_size());
  expect_eq(unittest::testalltypes::foo, message2.repeated_nested_enum(0));
  expect_eq(unittest::testalltypes::bar, message2.repeated_nested_enum(1));
}

test(generatedmessagetest, copyconstructor) {
  unittest::testalltypes message1;
  testutil::setallfields(&message1);

  unittest::testalltypes message2(message1);
  testutil::expectallfieldsset(message2);
}

test(generatedmessagetest, copyassignmentoperator) {
  unittest::testalltypes message1;
  testutil::setallfields(&message1);

  unittest::testalltypes message2;
  message2 = message1;
  testutil::expectallfieldsset(message2);

  // make sure that self-assignment does something sane.
  message2.operator=(message2);
  testutil::expectallfieldsset(message2);
}

#if !defined(protobuf_test_no_descriptors) || \
    !defined(google_protobuf_no_rtti)
test(generatedmessagetest, upcastcopyfrom) {
  // test the copyfrom method that takes in the generic const message&
  // parameter.
  unittest::testalltypes message1, message2;

  testutil::setallfields(&message1);

  const message* source = implicit_cast<const message*>(&message1);
  message2.copyfrom(*source);

  testutil::expectallfieldsset(message2);
}
#endif

#ifndef protobuf_test_no_descriptors

test(generatedmessagetest, dynamicmessagecopyfrom) {
  // test copying from a dynamicmessage, which must fall back to using
  // reflection.
  unittest::testalltypes message2;

  // construct a new version of the dynamic message via the factory.
  dynamicmessagefactory factory;
  scoped_ptr<message> message1;
  message1.reset(factory.getprototype(
                     unittest::testalltypes::descriptor())->new());

  testutil::reflectiontester reflection_tester(
    unittest::testalltypes::descriptor());
  reflection_tester.setallfieldsviareflection(message1.get());

  message2.copyfrom(*message1);

  testutil::expectallfieldsset(message2);
}

#endif  // !protobuf_test_no_descriptors

test(generatedmessagetest, nonemptymergefrom) {
  // test merging with a non-empty message. code is a modified form
  // of that found in google/protobuf/reflection_ops_unittest.cc.
  unittest::testalltypes message1, message2;

  testutil::setallfields(&message1);

  // this field will test merging into an empty spot.
  message2.set_optional_int32(message1.optional_int32());
  message1.clear_optional_int32();

  // this tests overwriting.
  message2.set_optional_string(message1.optional_string());
  message1.set_optional_string("something else");

  // this tests concatenating.
  message2.add_repeated_int32(message1.repeated_int32(1));
  int32 i = message1.repeated_int32(0);
  message1.clear_repeated_int32();
  message1.add_repeated_int32(i);

  message1.mergefrom(message2);

  testutil::expectallfieldsset(message1);
}

#if !defined(protobuf_test_no_descriptors) || \
    !defined(google_protobuf_no_rtti)
#ifdef protobuf_has_death_test

test(generatedmessagetest, mergefromself) {
  unittest::testalltypes message;
  expect_death(message.mergefrom(message), "&from");
  expect_death(message.mergefrom(implicit_cast<const message&>(message)),
               "&from");
}

#endif  // protobuf_has_death_test
#endif  // !protobuf_test_no_descriptors || !google_protobuf_no_rtti

// test the generated serializewithcachedsizestoarray(),
test(generatedmessagetest, serializationtoarray) {
  unittest::testalltypes message1, message2;
  string data;
  testutil::setallfields(&message1);
  int size = message1.bytesize();
  data.resize(size);
  uint8* start = reinterpret_cast<uint8*>(string_as_array(&data));
  uint8* end = message1.serializewithcachedsizestoarray(start);
  expect_eq(size, end - start);
  expect_true(message2.parsefromstring(data));
  testutil::expectallfieldsset(message2);

}

test(generatedmessagetest, packedfieldsserializationtoarray) {
  unittest::testpackedtypes packed_message1, packed_message2;
  string packed_data;
  testutil::setpackedfields(&packed_message1);
  int packed_size = packed_message1.bytesize();
  packed_data.resize(packed_size);
  uint8* start = reinterpret_cast<uint8*>(string_as_array(&packed_data));
  uint8* end = packed_message1.serializewithcachedsizestoarray(start);
  expect_eq(packed_size, end - start);
  expect_true(packed_message2.parsefromstring(packed_data));
  testutil::expectpackedfieldsset(packed_message2);
}

// test the generated serializewithcachedsizes() by forcing the buffer to write
// one byte at a time.
test(generatedmessagetest, serializationtostream) {
  unittest::testalltypes message1, message2;
  testutil::setallfields(&message1);
  int size = message1.bytesize();
  string data;
  data.resize(size);
  {
    // allow the output stream to buffer only one byte at a time.
    io::arrayoutputstream array_stream(string_as_array(&data), size, 1);
    io::codedoutputstream output_stream(&array_stream);
    message1.serializewithcachedsizes(&output_stream);
    expect_false(output_stream.haderror());
    expect_eq(size, output_stream.bytecount());
  }
  expect_true(message2.parsefromstring(data));
  testutil::expectallfieldsset(message2);

}

test(generatedmessagetest, packedfieldsserializationtostream) {
  unittest::testpackedtypes message1, message2;
  testutil::setpackedfields(&message1);
  int size = message1.bytesize();
  string data;
  data.resize(size);
  {
    // allow the output stream to buffer only one byte at a time.
    io::arrayoutputstream array_stream(string_as_array(&data), size, 1);
    io::codedoutputstream output_stream(&array_stream);
    message1.serializewithcachedsizes(&output_stream);
    expect_false(output_stream.haderror());
    expect_eq(size, output_stream.bytecount());
  }
  expect_true(message2.parsefromstring(data));
  testutil::expectpackedfieldsset(message2);
}


test(generatedmessagetest, required) {
  // test that isinitialized() returns false if required fields are missing.
  unittest::testrequired message;

  expect_false(message.isinitialized());
  message.set_a(1);
  expect_false(message.isinitialized());
  message.set_b(2);
  expect_false(message.isinitialized());
  message.set_c(3);
  expect_true(message.isinitialized());
}

test(generatedmessagetest, requiredforeign) {
  // test that isinitialized() returns false if required fields in nested
  // messages are missing.
  unittest::testrequiredforeign message;

  expect_true(message.isinitialized());

  message.mutable_optional_message();
  expect_false(message.isinitialized());

  message.mutable_optional_message()->set_a(1);
  message.mutable_optional_message()->set_b(2);
  message.mutable_optional_message()->set_c(3);
  expect_true(message.isinitialized());

  message.add_repeated_message();
  expect_false(message.isinitialized());

  message.mutable_repeated_message(0)->set_a(1);
  message.mutable_repeated_message(0)->set_b(2);
  message.mutable_repeated_message(0)->set_c(3);
  expect_true(message.isinitialized());
}

test(generatedmessagetest, foreignnested) {
  // test that testalltypes::nestedmessage can be embedded directly into
  // another message.
  unittest::testforeignnested message;

  // if this compiles and runs without crashing, it must work.  we have
  // nothing more to test.
  unittest::testalltypes::nestedmessage* nested =
    message.mutable_foreign_nested();
  nested->set_bb(1);
}

test(generatedmessagetest, reallylargetagnumber) {
  // test that really large tag numbers don't break anything.
  unittest::testreallylargetagnumber message1, message2;
  string data;

  // for the most part, if this compiles and runs then we're probably good.
  // (the most likely cause for failure would be if something were attempting
  // to allocate a lookup table of some sort using tag numbers as the index.)
  // we'll try serializing just for fun.
  message1.set_a(1234);
  message1.set_bb(5678);
  message1.serializetostring(&data);
  expect_true(message2.parsefromstring(data));
  expect_eq(1234, message2.a());
  expect_eq(5678, message2.bb());
}

test(generatedmessagetest, mutualrecursion) {
  // test that mutually-recursive message types work.
  unittest::testmutualrecursiona message;
  unittest::testmutualrecursiona* nested = message.mutable_bb()->mutable_a();
  unittest::testmutualrecursiona* nested2 = nested->mutable_bb()->mutable_a();

  // again, if the above compiles and runs, that's all we really have to
  // test, but just for run we'll check that the system didn't somehow come
  // up with a pointer loop...
  expect_ne(&message, nested);
  expect_ne(&message, nested2);
  expect_ne(nested, nested2);
}

test(generatedmessagetest, camelcasefieldnames) {
  // this test is mainly checking that the following compiles, which verifies
  // that the field names were coerced to lower-case.
  //
  // protocol buffers standard style is to use lowercase-with-underscores for
  // field names.  some old proto1 .protos unfortunately used camel-case field
  // names.  in proto1, these names were forced to lower-case.  so, we do the
  // same thing in proto2.

  unittest::testcamelcasefieldnames message;

  message.set_primitivefield(2);
  message.set_stringfield("foo");
  message.set_enumfield(unittest::foreign_foo);
  message.mutable_messagefield()->set_c(6);

  message.add_repeatedprimitivefield(8);
  message.add_repeatedstringfield("qux");
  message.add_repeatedenumfield(unittest::foreign_bar);
  message.add_repeatedmessagefield()->set_c(15);

  expect_eq(2, message.primitivefield());
  expect_eq("foo", message.stringfield());
  expect_eq(unittest::foreign_foo, message.enumfield());
  expect_eq(6, message.messagefield().c());

  expect_eq(8, message.repeatedprimitivefield(0));
  expect_eq("qux", message.repeatedstringfield(0));
  expect_eq(unittest::foreign_bar, message.repeatedenumfield(0));
  expect_eq(15, message.repeatedmessagefield(0).c());
}

test(generatedmessagetest, testconflictingsymbolnames) {
  // test_bad_identifiers.proto successfully compiled, then it works.  the
  // following is just a token usage to insure that the code is, in fact,
  // being compiled and linked.

  protobuf_unittest::testconflictingsymbolnames message;
  message.set_uint32(1);
  expect_eq(3, message.bytesize());

  message.set_friend_(5);
  expect_eq(5, message.friend_());

  // instantiate extension template functions to test conflicting template
  // parameter names.
  typedef protobuf_unittest::testconflictingsymbolnamesextension extensionmessage;
  message.addextension(extensionmessage::repeated_int32_ext, 123);
  expect_eq(123,
            message.getextension(extensionmessage::repeated_int32_ext, 0));
}

#ifndef protobuf_test_no_descriptors

test(generatedmessagetest, testoptimizedforsize) {
  // we rely on the tests in reflection_ops_unittest and wire_format_unittest
  // to really test that reflection-based methods work.  here we are mostly
  // just making sure that testoptimizedforsize actually builds and seems to
  // function.

  protobuf_unittest::testoptimizedforsize message, message2;
  message.set_i(1);
  message.mutable_msg()->set_c(2);
  message2.copyfrom(message);
  expect_eq(1, message2.i());
  expect_eq(2, message2.msg().c());
}

test(generatedmessagetest, testembedoptimizedforsize) {
  // verifies that something optimized for speed can contain something optimized
  // for size.

  protobuf_unittest::testembedoptimizedforsize message, message2;
  message.mutable_optional_message()->set_i(1);
  message.add_repeated_message()->mutable_msg()->set_c(2);
  string data;
  message.serializetostring(&data);
  assert_true(message2.parsefromstring(data));
  expect_eq(1, message2.optional_message().i());
  expect_eq(2, message2.repeated_message(0).msg().c());
}

test(generatedmessagetest, testspaceused) {
  unittest::testalltypes message1;
  // sizeof provides a lower bound on spaceused().
  expect_le(sizeof(unittest::testalltypes), message1.spaceused());
  const int empty_message_size = message1.spaceused();

  // setting primitive types shouldn't affect the space used.
  message1.set_optional_int32(123);
  message1.set_optional_int64(12345);
  message1.set_optional_uint32(123);
  message1.set_optional_uint64(12345);
  expect_eq(empty_message_size, message1.spaceused());

  // on some stl implementations, setting the string to a small value should
  // only increase spaceused() by the size of a string object, though this is
  // not true everywhere.
  message1.set_optional_string("abc");
  expect_le(empty_message_size + sizeof(string), message1.spaceused());

  // setting a string to a value larger than the string object itself should
  // increase spaceused(), because it cannot store the value internally.
  message1.set_optional_string(string(sizeof(string) + 1, 'x'));
  int min_expected_increase = message1.optional_string().capacity() +
      sizeof(string);
  expect_le(empty_message_size + min_expected_increase,
            message1.spaceused());

  int previous_size = message1.spaceused();
  // adding an optional message should increase the size by the size of the
  // nested message type. nestedmessage is simple enough (1 int field) that it
  // is equal to sizeof(nestedmessage)
  message1.mutable_optional_nested_message();
  assert_eq(sizeof(unittest::testalltypes::nestedmessage),
            message1.optional_nested_message().spaceused());
  expect_eq(previous_size +
            sizeof(unittest::testalltypes::nestedmessage),
            message1.spaceused());
}

#endif  // !protobuf_test_no_descriptors


test(generatedmessagetest, fieldconstantvalues) {
  unittest::testrequired message;
  expect_eq(unittest::testalltypes_nestedmessage::kbbfieldnumber, 1);
  expect_eq(unittest::testalltypes::koptionalint32fieldnumber, 1);
  expect_eq(unittest::testalltypes::koptionalgroupfieldnumber, 16);
  expect_eq(unittest::testalltypes::koptionalnestedmessagefieldnumber, 18);
  expect_eq(unittest::testalltypes::koptionalnestedenumfieldnumber, 21);
  expect_eq(unittest::testalltypes::krepeatedint32fieldnumber, 31);
  expect_eq(unittest::testalltypes::krepeatedgroupfieldnumber, 46);
  expect_eq(unittest::testalltypes::krepeatednestedmessagefieldnumber, 48);
  expect_eq(unittest::testalltypes::krepeatednestedenumfieldnumber, 51);
}

test(generatedmessagetest, extensionconstantvalues) {
  expect_eq(unittest::testrequired::ksinglefieldnumber, 1000);
  expect_eq(unittest::testrequired::kmultifieldnumber, 1001);
  expect_eq(unittest::koptionalint32extensionfieldnumber, 1);
  expect_eq(unittest::koptionalgroupextensionfieldnumber, 16);
  expect_eq(unittest::koptionalnestedmessageextensionfieldnumber, 18);
  expect_eq(unittest::koptionalnestedenumextensionfieldnumber, 21);
  expect_eq(unittest::krepeatedint32extensionfieldnumber, 31);
  expect_eq(unittest::krepeatedgroupextensionfieldnumber, 46);
  expect_eq(unittest::krepeatednestedmessageextensionfieldnumber, 48);
  expect_eq(unittest::krepeatednestedenumextensionfieldnumber, 51);
}

// ===================================================================

test(generatedenumtest, enumvaluesasswitchcases) {
  // test that our nested enum values can be used as switch cases.  this test
  // doesn't actually do anything, the proof that it works is that it
  // compiles.
  int i =0;
  unittest::testalltypes::nestedenum a = unittest::testalltypes::bar;
  switch (a) {
    case unittest::testalltypes::foo:
      i = 1;
      break;
    case unittest::testalltypes::bar:
      i = 2;
      break;
    case unittest::testalltypes::baz:
      i = 3;
      break;
    // no default case:  we want to make sure the compiler recognizes that
    //   all cases are covered.  (gcc warns if you do not cover all cases of
    //   an enum in a switch.)
  }

  // token check just for fun.
  expect_eq(2, i);
}

test(generatedenumtest, isvalidvalue) {
  // test enum isvalidvalue.
  expect_true(unittest::testalltypes::nestedenum_isvalid(1));
  expect_true(unittest::testalltypes::nestedenum_isvalid(2));
  expect_true(unittest::testalltypes::nestedenum_isvalid(3));

  expect_false(unittest::testalltypes::nestedenum_isvalid(0));
  expect_false(unittest::testalltypes::nestedenum_isvalid(4));

  // make sure it also works when there are dups.
  expect_true(unittest::testenumwithdupvalue_isvalid(1));
  expect_true(unittest::testenumwithdupvalue_isvalid(2));
  expect_true(unittest::testenumwithdupvalue_isvalid(3));

  expect_false(unittest::testenumwithdupvalue_isvalid(0));
  expect_false(unittest::testenumwithdupvalue_isvalid(4));
}

test(generatedenumtest, minandmax) {
  expect_eq(unittest::testalltypes::foo,
            unittest::testalltypes::nestedenum_min);
  expect_eq(unittest::testalltypes::baz,
            unittest::testalltypes::nestedenum_max);
  expect_eq(4, unittest::testalltypes::nestedenum_arraysize);

  expect_eq(unittest::foreign_foo, unittest::foreignenum_min);
  expect_eq(unittest::foreign_baz, unittest::foreignenum_max);
  expect_eq(7, unittest::foreignenum_arraysize);

  expect_eq(1, unittest::testenumwithdupvalue_min);
  expect_eq(3, unittest::testenumwithdupvalue_max);
  expect_eq(4, unittest::testenumwithdupvalue_arraysize);

  expect_eq(unittest::sparse_e, unittest::testsparseenum_min);
  expect_eq(unittest::sparse_c, unittest::testsparseenum_max);
  expect_eq(12589235, unittest::testsparseenum_arraysize);

  // make sure we can take the address of _min, _max and _arraysize.
  void* null_pointer = 0;  // null may be integer-type, not pointer-type.
  expect_ne(null_pointer, &unittest::testalltypes::nestedenum_min);
  expect_ne(null_pointer, &unittest::testalltypes::nestedenum_max);
  expect_ne(null_pointer, &unittest::testalltypes::nestedenum_arraysize);

  expect_ne(null_pointer, &unittest::foreignenum_min);
  expect_ne(null_pointer, &unittest::foreignenum_max);
  expect_ne(null_pointer, &unittest::foreignenum_arraysize);

  // make sure we can use _min and _max as switch cases.
  switch (unittest::sparse_a) {
    case unittest::testsparseenum_min:
    case unittest::testsparseenum_max:
      break;
    default:
      break;
  }
}

#ifndef protobuf_test_no_descriptors

test(generatedenumtest, name) {
  // "names" in the presence of dup values are a bit arbitrary.
  expect_eq("foo1", unittest::testenumwithdupvalue_name(unittest::foo1));
  expect_eq("foo1", unittest::testenumwithdupvalue_name(unittest::foo2));

  expect_eq("sparse_a", unittest::testsparseenum_name(unittest::sparse_a));
  expect_eq("sparse_b", unittest::testsparseenum_name(unittest::sparse_b));
  expect_eq("sparse_c", unittest::testsparseenum_name(unittest::sparse_c));
  expect_eq("sparse_d", unittest::testsparseenum_name(unittest::sparse_d));
  expect_eq("sparse_e", unittest::testsparseenum_name(unittest::sparse_e));
  expect_eq("sparse_f", unittest::testsparseenum_name(unittest::sparse_f));
  expect_eq("sparse_g", unittest::testsparseenum_name(unittest::sparse_g));
}

test(generatedenumtest, parse) {
  unittest::testenumwithdupvalue dup_value = unittest::foo1;
  expect_true(unittest::testenumwithdupvalue_parse("foo1", &dup_value));
  expect_eq(unittest::foo1, dup_value);
  expect_true(unittest::testenumwithdupvalue_parse("foo2", &dup_value));
  expect_eq(unittest::foo2, dup_value);
  expect_false(unittest::testenumwithdupvalue_parse("foo", &dup_value));
}

test(generatedenumtest, getenumdescriptor) {
  expect_eq(unittest::testalltypes::nestedenum_descriptor(),
            getenumdescriptor<unittest::testalltypes::nestedenum>());
  expect_eq(unittest::foreignenum_descriptor(),
            getenumdescriptor<unittest::foreignenum>());
  expect_eq(unittest::testenumwithdupvalue_descriptor(),
            getenumdescriptor<unittest::testenumwithdupvalue>());
  expect_eq(unittest::testsparseenum_descriptor(),
            getenumdescriptor<unittest::testsparseenum>());
}

#endif  // protobuf_test_no_descriptors

// ===================================================================

#ifndef protobuf_test_no_descriptors

// support code for testing services.
class generatedservicetest : public testing::test {
 protected:
  class mocktestservice : public unittest::testservice {
   public:
    mocktestservice()
      : called_(false),
        method_(""),
        controller_(null),
        request_(null),
        response_(null),
        done_(null) {}

    ~mocktestservice() {}

    void reset() { called_ = false; }

    // implements testservice ----------------------------------------

    void foo(rpccontroller* controller,
             const unittest::foorequest* request,
             unittest::fooresponse* response,
             closure* done) {
      assert_false(called_);
      called_ = true;
      method_ = "foo";
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    void bar(rpccontroller* controller,
             const unittest::barrequest* request,
             unittest::barresponse* response,
             closure* done) {
      assert_false(called_);
      called_ = true;
      method_ = "bar";
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    // ---------------------------------------------------------------

    bool called_;
    string method_;
    rpccontroller* controller_;
    const message* request_;
    message* response_;
    closure* done_;
  };

  class mockrpcchannel : public rpcchannel {
   public:
    mockrpcchannel()
      : called_(false),
        method_(null),
        controller_(null),
        request_(null),
        response_(null),
        done_(null),
        destroyed_(null) {}

    ~mockrpcchannel() {
      if (destroyed_ != null) *destroyed_ = true;
    }

    void reset() { called_ = false; }

    // implements testservice ----------------------------------------

    void callmethod(const methoddescriptor* method,
                    rpccontroller* controller,
                    const message* request,
                    message* response,
                    closure* done) {
      assert_false(called_);
      called_ = true;
      method_ = method;
      controller_ = controller;
      request_ = request;
      response_ = response;
      done_ = done;
    }

    // ---------------------------------------------------------------

    bool called_;
    const methoddescriptor* method_;
    rpccontroller* controller_;
    const message* request_;
    message* response_;
    closure* done_;
    bool* destroyed_;
  };

  class mockcontroller : public rpccontroller {
   public:
    void reset() {
      add_failure() << "reset() not expected during this test.";
    }
    bool failed() const {
      add_failure() << "failed() not expected during this test.";
      return false;
    }
    string errortext() const {
      add_failure() << "errortext() not expected during this test.";
      return "";
    }
    void startcancel() {
      add_failure() << "startcancel() not expected during this test.";
    }
    void setfailed(const string& reason) {
      add_failure() << "setfailed() not expected during this test.";
    }
    bool iscanceled() const {
      add_failure() << "iscanceled() not expected during this test.";
      return false;
    }
    void notifyoncancel(closure* callback) {
      add_failure() << "notifyoncancel() not expected during this test.";
    }
  };

  generatedservicetest()
    : descriptor_(unittest::testservice::descriptor()),
      foo_(descriptor_->findmethodbyname("foo")),
      bar_(descriptor_->findmethodbyname("bar")),
      stub_(&mock_channel_),
      done_(newpermanentcallback(&donothing)) {}

  virtual void setup() {
    assert_true(foo_ != null);
    assert_true(bar_ != null);
  }

  const servicedescriptor* descriptor_;
  const methoddescriptor* foo_;
  const methoddescriptor* bar_;

  mocktestservice mock_service_;
  mockcontroller mock_controller_;

  mockrpcchannel mock_channel_;
  unittest::testservice::stub stub_;

  // just so we don't have to re-define these with every test.
  unittest::foorequest foo_request_;
  unittest::fooresponse foo_response_;
  unittest::barrequest bar_request_;
  unittest::barresponse bar_response_;
  scoped_ptr<closure> done_;
};

test_f(generatedservicetest, getdescriptor) {
  // test that getdescriptor() works.

  expect_eq(descriptor_, mock_service_.getdescriptor());
}

test_f(generatedservicetest, getchannel) {
  expect_eq(&mock_channel_, stub_.channel());
}

test_f(generatedservicetest, ownschannel) {
  mockrpcchannel* channel = new mockrpcchannel;
  bool destroyed = false;
  channel->destroyed_ = &destroyed;

  {
    unittest::testservice::stub owning_stub(channel,
                                            service::stub_owns_channel);
    expect_false(destroyed);
  }

  expect_true(destroyed);
}

test_f(generatedservicetest, callmethod) {
  // test that callmethod() works.

  // call foo() via callmethod().
  mock_service_.callmethod(foo_, &mock_controller_,
                           &foo_request_, &foo_response_, done_.get());

  assert_true(mock_service_.called_);

  expect_eq("foo"            , mock_service_.method_    );
  expect_eq(&mock_controller_, mock_service_.controller_);
  expect_eq(&foo_request_    , mock_service_.request_   );
  expect_eq(&foo_response_   , mock_service_.response_  );
  expect_eq(done_.get()      , mock_service_.done_      );

  // try again, but call bar() instead.
  mock_service_.reset();
  mock_service_.callmethod(bar_, &mock_controller_,
                           &bar_request_, &bar_response_, done_.get());

  assert_true(mock_service_.called_);
  expect_eq("bar", mock_service_.method_);
}

test_f(generatedservicetest, callmethodtypefailure) {
  // verify death if we call foo() with bar's message types.

#ifdef protobuf_has_death_test  // death tests do not work on windows yet
  expect_debug_death(
    mock_service_.callmethod(foo_, &mock_controller_,
                             &foo_request_, &bar_response_, done_.get()),
    "dynamic_cast");

  mock_service_.reset();
  expect_debug_death(
    mock_service_.callmethod(foo_, &mock_controller_,
                             &bar_request_, &foo_response_, done_.get()),
    "dynamic_cast");
#endif  // protobuf_has_death_test
}

test_f(generatedservicetest, getprototypes) {
  // test get{request,response}prototype() methods.

  expect_eq(&unittest::foorequest::default_instance(),
            &mock_service_.getrequestprototype(foo_));
  expect_eq(&unittest::barrequest::default_instance(),
            &mock_service_.getrequestprototype(bar_));

  expect_eq(&unittest::fooresponse::default_instance(),
            &mock_service_.getresponseprototype(foo_));
  expect_eq(&unittest::barresponse::default_instance(),
            &mock_service_.getresponseprototype(bar_));
}

test_f(generatedservicetest, stub) {
  // test that the stub class works.

  // call foo() via the stub.
  stub_.foo(&mock_controller_, &foo_request_, &foo_response_, done_.get());

  assert_true(mock_channel_.called_);

  expect_eq(foo_             , mock_channel_.method_    );
  expect_eq(&mock_controller_, mock_channel_.controller_);
  expect_eq(&foo_request_    , mock_channel_.request_   );
  expect_eq(&foo_response_   , mock_channel_.response_  );
  expect_eq(done_.get()      , mock_channel_.done_      );

  // call bar() via the stub.
  mock_channel_.reset();
  stub_.bar(&mock_controller_, &bar_request_, &bar_response_, done_.get());

  assert_true(mock_channel_.called_);
  expect_eq(bar_, mock_channel_.method_);
}

test_f(generatedservicetest, notimplemented) {
  // test that failing to implement a method of a service causes it to fail
  // with a "not implemented" error message.

  // a service which doesn't implement any methods.
  class unimplementedservice : public unittest::testservice {
   public:
    unimplementedservice() {}
  };

  unimplementedservice unimplemented_service;

  // and a controller which expects to get a "not implemented" error.
  class expectunimplementedcontroller : public mockcontroller {
   public:
    expectunimplementedcontroller() : called_(false) {}

    void setfailed(const string& reason) {
      expect_false(called_);
      called_ = true;
      expect_eq("method foo() not implemented.", reason);
    }

    bool called_;
  };

  expectunimplementedcontroller controller;

  // call foo.
  unimplemented_service.foo(&controller, &foo_request_, &foo_response_,
                            done_.get());

  expect_true(controller.called_);
}

}  // namespace cpp_unittest
}  // namespace cpp
}  // namespace compiler

namespace no_generic_services_test {
  // verify that no class called "testservice" was defined in
  // unittest_no_generic_services.pb.h by defining a different type by the same
  // name.  if such a service was generated, this will not compile.
  struct testservice {
    int i;
  };
}

namespace compiler {
namespace cpp {
namespace cpp_unittest {

test_f(generatedservicetest, nogenericservices) {
  // verify that non-services in unittest_no_generic_services.proto were
  // generated.
  no_generic_services_test::testmessage message;
  message.set_a(1);
  message.setextension(no_generic_services_test::test_extension, 123);
  no_generic_services_test::testenum e = no_generic_services_test::foo;
  expect_eq(e, 1);

  // verify that a servicedescriptor is generated for the service even if the
  // class itself is not.
  const filedescriptor* file =
      no_generic_services_test::testmessage::descriptor()->file();

  assert_eq(1, file->service_count());
  expect_eq("testservice", file->service(0)->name());
  assert_eq(1, file->service(0)->method_count());
  expect_eq("foo", file->service(0)->method(0)->name());
}

#endif  // !protobuf_test_no_descriptors

// ===================================================================

// this test must run last.  it verifies that descriptors were or were not
// initialized depending on whether protobuf_test_no_descriptors was defined.
// when this is defined, we skip all tests which are expected to trigger
// descriptor initialization.  this verifies that everything else still works
// if descriptors are not initialized.
test(descriptorinitializationtest, initialized) {
#ifdef protobuf_test_no_descriptors
  bool should_have_descriptors = false;
#else
  bool should_have_descriptors = true;
#endif

  expect_eq(should_have_descriptors,
    descriptorpool::generated_pool()->internalisfileloaded(
      "google/protobuf/unittest.proto"));
}

}  // namespace cpp_unittest

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
