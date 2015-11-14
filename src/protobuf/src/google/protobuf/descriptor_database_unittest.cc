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
// this file makes extensive use of rfc 3092.  :)

#include <algorithm>

#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {

static void addtodatabase(simpledescriptordatabase* database,
                          const char* file_text) {
  filedescriptorproto file_proto;
  expect_true(textformat::parsefromstring(file_text, &file_proto));
  database->add(file_proto);
}

static void expectcontainstype(const filedescriptorproto& proto,
                               const string& type_name) {
  for (int i = 0; i < proto.message_type_size(); i++) {
    if (proto.message_type(i).name() == type_name) return;
  }
  add_failure() << "\"" << proto.name()
                << "\" did not contain expected type \""
                << type_name << "\".";
}

// ===================================================================

#if gtest_has_param_test

// simpledescriptordatabase, encodeddescriptordatabase, and
// descriptorpooldatabase call for very similar tests.  instead of writing
// three nearly-identical sets of tests, we use parameterized tests to apply
// the same code to all three.

// the parameterized test runs against a descriptardatabasetestcase.  we have
// implementations for each of the three classes we want to test.
class descriptordatabasetestcase {
 public:
  virtual ~descriptordatabasetestcase() {}

  virtual descriptordatabase* getdatabase() = 0;
  virtual bool addtodatabase(const filedescriptorproto& file) = 0;
};

// factory function type.
typedef descriptordatabasetestcase* descriptordatabasetestcasefactory();

// specialization for simpledescriptordatabase.
class simpledescriptordatabasetestcase : public descriptordatabasetestcase {
 public:
  static descriptordatabasetestcase* new() {
    return new simpledescriptordatabasetestcase;
  }

  virtual ~simpledescriptordatabasetestcase() {}

  virtual descriptordatabase* getdatabase() {
    return &database_;
  }
  virtual bool addtodatabase(const filedescriptorproto& file) {
    return database_.add(file);
  }

 private:
  simpledescriptordatabase database_;
};

// specialization for encodeddescriptordatabase.
class encodeddescriptordatabasetestcase : public descriptordatabasetestcase {
 public:
  static descriptordatabasetestcase* new() {
    return new encodeddescriptordatabasetestcase;
  }

  virtual ~encodeddescriptordatabasetestcase() {}

  virtual descriptordatabase* getdatabase() {
    return &database_;
  }
  virtual bool addtodatabase(const filedescriptorproto& file) {
    string data;
    file.serializetostring(&data);
    return database_.addcopy(data.data(), data.size());
  }

 private:
  encodeddescriptordatabase database_;
};

// specialization for descriptorpooldatabase.
class descriptorpooldatabasetestcase : public descriptordatabasetestcase {
 public:
  static descriptordatabasetestcase* new() {
    return new encodeddescriptordatabasetestcase;
  }

  descriptorpooldatabasetestcase() : database_(pool_) {}
  virtual ~descriptorpooldatabasetestcase() {}

  virtual descriptordatabase* getdatabase() {
    return &database_;
  }
  virtual bool addtodatabase(const filedescriptorproto& file) {
    return pool_.buildfile(file);
  }

 private:
  descriptorpool pool_;
  descriptorpooldatabase database_;
};

// -------------------------------------------------------------------

class descriptordatabasetest
    : public testing::testwithparam<descriptordatabasetestcasefactory*> {
 protected:
  virtual void setup() {
    test_case_.reset(getparam()());
    database_ = test_case_->getdatabase();
  }

  void addtodatabase(const char* file_descriptor_text) {
    filedescriptorproto file_proto;
    expect_true(textformat::parsefromstring(file_descriptor_text, &file_proto));
    expect_true(test_case_->addtodatabase(file_proto));
  }

  void addtodatabasewitherror(const char* file_descriptor_text) {
    filedescriptorproto file_proto;
    expect_true(textformat::parsefromstring(file_descriptor_text, &file_proto));
    expect_false(test_case_->addtodatabase(file_proto));
  }

  scoped_ptr<descriptordatabasetestcase> test_case_;
  descriptordatabase* database_;
};

test_p(descriptordatabasetest, findfilebyname) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { name:\"foo\" }");
  addtodatabase(
    "name: \"bar.proto\" "
    "message_type { name:\"bar\" }");

  {
    filedescriptorproto file;
    expect_true(database_->findfilebyname("foo.proto", &file));
    expect_eq("foo.proto", file.name());
    expectcontainstype(file, "foo");
  }

  {
    filedescriptorproto file;
    expect_true(database_->findfilebyname("bar.proto", &file));
    expect_eq("bar.proto", file.name());
    expectcontainstype(file, "bar");
  }

  {
    // fails to find undefined files.
    filedescriptorproto file;
    expect_false(database_->findfilebyname("baz.proto", &file));
  }
}

test_p(descriptordatabasetest, findfilecontainingsymbol) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "  field { name:\"qux\" }"
    "  nested_type { name: \"grault\" } "
    "  enum_type { name: \"garply\" } "
    "} "
    "enum_type { "
    "  name: \"waldo\" "
    "  value { name:\"fred\" } "
    "} "
    "extension { name: \"plugh\" } "
    "service { "
    "  name: \"xyzzy\" "
    "  method { name: \"thud\" } "
    "}"
    );
  addtodatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "message_type { name: \"bar\" }");

  {
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("foo", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find fields.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("foo.qux", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find nested types.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("foo.grault", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find nested enums.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("foo.garply", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find enum types.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("waldo", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find enum values.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("waldo.fred", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find extensions.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("plugh", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find services.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("xyzzy", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find methods.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("xyzzy.thud", &file));
    expect_eq("foo.proto", file.name());
  }

  {
    // can find things in packages.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingsymbol("corge.bar", &file));
    expect_eq("bar.proto", file.name());
  }

  {
    // fails to find undefined symbols.
    filedescriptorproto file;
    expect_false(database_->findfilecontainingsymbol("baz", &file));
  }

  {
    // names must be fully-qualified.
    filedescriptorproto file;
    expect_false(database_->findfilecontainingsymbol("bar", &file));
  }
}

test_p(descriptordatabasetest, findfilecontainingextension) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "  extension_range { start: 1 end: 1000 } "
    "  extension { name:\"qux\" label:label_optional type:type_int32 number:5 "
    "              extendee: \".foo\" }"
    "}");
  addtodatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "dependency: \"foo.proto\" "
    "message_type { "
    "  name: \"bar\" "
    "  extension_range { start: 1 end: 1000 } "
    "} "
    "extension { name:\"grault\" extendee: \".foo\"       number:32 } "
    "extension { name:\"garply\" extendee: \".corge.bar\" number:70 } "
    "extension { name:\"waldo\"  extendee: \"bar\"        number:56 } ");

  {
    filedescriptorproto file;
    expect_true(database_->findfilecontainingextension("foo", 5, &file));
    expect_eq("foo.proto", file.name());
  }

  {
    filedescriptorproto file;
    expect_true(database_->findfilecontainingextension("foo", 32, &file));
    expect_eq("bar.proto", file.name());
  }

  {
    // can find extensions for qualified type names.
    filedescriptorproto file;
    expect_true(database_->findfilecontainingextension("corge.bar", 70, &file));
    expect_eq("bar.proto", file.name());
  }

  {
    // can't find extensions whose extendee was not fully-qualified in the
    // filedescriptorproto.
    filedescriptorproto file;
    expect_false(database_->findfilecontainingextension("bar", 56, &file));
    expect_false(
        database_->findfilecontainingextension("corge.bar", 56, &file));
  }

  {
    // can't find non-existent extension numbers.
    filedescriptorproto file;
    expect_false(database_->findfilecontainingextension("foo", 12, &file));
  }

  {
    // can't find extensions for non-existent types.
    filedescriptorproto file;
    expect_false(
        database_->findfilecontainingextension("nosuchtype", 5, &file));
  }

  {
    // can't find extensions for unqualified type names.
    filedescriptorproto file;
    expect_false(database_->findfilecontainingextension("bar", 70, &file));
  }
}

test_p(descriptordatabasetest, findallextensionnumbers) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "  extension_range { start: 1 end: 1000 } "
    "  extension { name:\"qux\" label:label_optional type:type_int32 number:5 "
    "              extendee: \".foo\" }"
    "}");
  addtodatabase(
    "name: \"bar.proto\" "
    "package: \"corge\" "
    "dependency: \"foo.proto\" "
    "message_type { "
    "  name: \"bar\" "
    "  extension_range { start: 1 end: 1000 } "
    "} "
    "extension { name:\"grault\" extendee: \".foo\"       number:32 } "
    "extension { name:\"garply\" extendee: \".corge.bar\" number:70 } "
    "extension { name:\"waldo\"  extendee: \"bar\"        number:56 } ");

  {
    vector<int> numbers;
    expect_true(database_->findallextensionnumbers("foo", &numbers));
    assert_eq(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    expect_eq(5, numbers[0]);
    expect_eq(32, numbers[1]);
  }

  {
    vector<int> numbers;
    expect_true(database_->findallextensionnumbers("corge.bar", &numbers));
    // note: won't find extension 56 due to the name not being fully qualified.
    assert_eq(1, numbers.size());
    expect_eq(70, numbers[0]);
  }

  {
    // can't find extensions for non-existent types.
    vector<int> numbers;
    expect_false(database_->findallextensionnumbers("nosuchtype", &numbers));
  }

  {
    // can't find extensions for unqualified types.
    vector<int> numbers;
    expect_false(database_->findallextensionnumbers("bar", &numbers));
  }
}

test_p(descriptordatabasetest, conflictingfileerror) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "}");
  addtodatabasewitherror(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"bar\" "
    "}");
}

test_p(descriptordatabasetest, conflictingtypeerror) {
  addtodatabase(
    "name: \"foo.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "}");
  addtodatabasewitherror(
    "name: \"bar.proto\" "
    "message_type { "
    "  name: \"foo\" "
    "}");
}

test_p(descriptordatabasetest, conflictingextensionerror) {
  addtodatabase(
    "name: \"foo.proto\" "
    "extension { name:\"foo\" label:label_optional type:type_int32 number:5 "
    "            extendee: \".foo\" }");
  addtodatabasewitherror(
    "name: \"bar.proto\" "
    "extension { name:\"bar\" label:label_optional type:type_int32 number:5 "
    "            extendee: \".foo\" }");
}

instantiate_test_case_p(simple, descriptordatabasetest,
    testing::values(&simpledescriptordatabasetestcase::new));
instantiate_test_case_p(memoryconserving, descriptordatabasetest,
    testing::values(&encodeddescriptordatabasetestcase::new));
instantiate_test_case_p(pool, descriptordatabasetest,
    testing::values(&descriptorpooldatabasetestcase::new));

#endif  // gtest_has_param_test

test(encodeddescriptordatabaseextratest, findnameoffilecontainingsymbol) {
  // create two files, one of which is in two parts.
  filedescriptorproto file1, file2a, file2b;
  file1.set_name("foo.proto");
  file1.set_package("foo");
  file1.add_message_type()->set_name("foo");
  file2a.set_name("bar.proto");
  file2b.set_package("bar");
  file2b.add_message_type()->set_name("bar");

  // normal serialization allows our optimization to kick in.
  string data1 = file1.serializeasstring();

  // force out-of-order serialization to test slow path.
  string data2 = file2b.serializeasstring() + file2a.serializeasstring();

  // create encodeddescriptordatabase containing both files.
  encodeddescriptordatabase db;
  db.add(data1.data(), data1.size());
  db.add(data2.data(), data2.size());

  // test!
  string filename;
  expect_true(db.findnameoffilecontainingsymbol("foo.foo", &filename));
  expect_eq("foo.proto", filename);
  expect_true(db.findnameoffilecontainingsymbol("foo.foo.blah", &filename));
  expect_eq("foo.proto", filename);
  expect_true(db.findnameoffilecontainingsymbol("bar.bar", &filename));
  expect_eq("bar.proto", filename);
  expect_false(db.findnameoffilecontainingsymbol("foo", &filename));
  expect_false(db.findnameoffilecontainingsymbol("bar", &filename));
  expect_false(db.findnameoffilecontainingsymbol("baz.baz", &filename));
}

// ===================================================================

class mergeddescriptordatabasetest : public testing::test {
 protected:
  mergeddescriptordatabasetest()
    : forward_merged_(&database1_, &database2_),
      reverse_merged_(&database2_, &database1_) {}

  virtual void setup() {
    addtodatabase(&database1_,
      "name: \"foo.proto\" "
      "message_type { name:\"foo\" extension_range { start: 1 end: 100 } } "
      "extension { name:\"foo_ext\" extendee: \".foo\" number:3 "
      "            label:label_optional type:type_int32 } ");
    addtodatabase(&database2_,
      "name: \"bar.proto\" "
      "message_type { name:\"bar\" extension_range { start: 1 end: 100 } } "
      "extension { name:\"bar_ext\" extendee: \".bar\" number:5 "
      "            label:label_optional type:type_int32 } ");

    // baz.proto exists in both pools, with different definitions.
    addtodatabase(&database1_,
      "name: \"baz.proto\" "
      "message_type { name:\"baz\" extension_range { start: 1 end: 100 } } "
      "message_type { name:\"frompool1\" } "
      "extension { name:\"baz_ext\" extendee: \".baz\" number:12 "
      "            label:label_optional type:type_int32 } "
      "extension { name:\"database1_only_ext\" extendee: \".baz\" number:13 "
      "            label:label_optional type:type_int32 } ");
    addtodatabase(&database2_,
      "name: \"baz.proto\" "
      "message_type { name:\"baz\" extension_range { start: 1 end: 100 } } "
      "message_type { name:\"frompool2\" } "
      "extension { name:\"baz_ext\" extendee: \".baz\" number:12 "
      "            label:label_optional type:type_int32 } ");
  }

  simpledescriptordatabase database1_;
  simpledescriptordatabase database2_;

  mergeddescriptordatabase forward_merged_;
  mergeddescriptordatabase reverse_merged_;
};

test_f(mergeddescriptordatabasetest, findfilebyname) {
  {
    // can find file that is only in database1_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilebyname("foo.proto", &file));
    expect_eq("foo.proto", file.name());
    expectcontainstype(file, "foo");
  }

  {
    // can find file that is only in database2_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilebyname("bar.proto", &file));
    expect_eq("bar.proto", file.name());
    expectcontainstype(file, "bar");
  }

  {
    // in forward_merged_, database1_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilebyname("baz.proto", &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool1");
  }

  {
    // in reverse_merged_, database2_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(reverse_merged_.findfilebyname("baz.proto", &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool2");
  }

  {
    // can't find non-existent file.
    filedescriptorproto file;
    expect_false(forward_merged_.findfilebyname("no_such.proto", &file));
  }
}

test_f(mergeddescriptordatabasetest, findfilecontainingsymbol) {
  {
    // can find file that is only in database1_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilecontainingsymbol("foo", &file));
    expect_eq("foo.proto", file.name());
    expectcontainstype(file, "foo");
  }

  {
    // can find file that is only in database2_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilecontainingsymbol("bar", &file));
    expect_eq("bar.proto", file.name());
    expectcontainstype(file, "bar");
  }

  {
    // in forward_merged_, database1_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilecontainingsymbol("baz", &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool1");
  }

  {
    // in reverse_merged_, database2_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(reverse_merged_.findfilecontainingsymbol("baz", &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool2");
  }

  {
    // frompool1 only shows up in forward_merged_ because it is masked by
    // database2_'s baz.proto in reverse_merged_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilecontainingsymbol("frompool1", &file));
    expect_false(reverse_merged_.findfilecontainingsymbol("frompool1", &file));
  }

  {
    // can't find non-existent symbol.
    filedescriptorproto file;
    expect_false(
      forward_merged_.findfilecontainingsymbol("nosuchtype", &file));
  }
}

test_f(mergeddescriptordatabasetest, findfilecontainingextension) {
  {
    // can find file that is only in database1_.
    filedescriptorproto file;
    expect_true(
      forward_merged_.findfilecontainingextension("foo", 3, &file));
    expect_eq("foo.proto", file.name());
    expectcontainstype(file, "foo");
  }

  {
    // can find file that is only in database2_.
    filedescriptorproto file;
    expect_true(
      forward_merged_.findfilecontainingextension("bar", 5, &file));
    expect_eq("bar.proto", file.name());
    expectcontainstype(file, "bar");
  }

  {
    // in forward_merged_, database1_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(
      forward_merged_.findfilecontainingextension("baz", 12, &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool1");
  }

  {
    // in reverse_merged_, database2_'s baz.proto takes precedence.
    filedescriptorproto file;
    expect_true(
      reverse_merged_.findfilecontainingextension("baz", 12, &file));
    expect_eq("baz.proto", file.name());
    expectcontainstype(file, "frompool2");
  }

  {
    // baz's extension 13 only shows up in forward_merged_ because it is
    // masked by database2_'s baz.proto in reverse_merged_.
    filedescriptorproto file;
    expect_true(forward_merged_.findfilecontainingextension("baz", 13, &file));
    expect_false(reverse_merged_.findfilecontainingextension("baz", 13, &file));
  }

  {
    // can't find non-existent extension.
    filedescriptorproto file;
    expect_false(
      forward_merged_.findfilecontainingextension("foo", 6, &file));
  }
}

test_f(mergeddescriptordatabasetest, findallextensionnumbers) {
  {
    // message only has extension in database1_
    vector<int> numbers;
    expect_true(forward_merged_.findallextensionnumbers("foo", &numbers));
    assert_eq(1, numbers.size());
    expect_eq(3, numbers[0]);
  }

  {
    // message only has extension in database2_
    vector<int> numbers;
    expect_true(forward_merged_.findallextensionnumbers("bar", &numbers));
    assert_eq(1, numbers.size());
    expect_eq(5, numbers[0]);
  }

  {
    // merge results from the two databases.
    vector<int> numbers;
    expect_true(forward_merged_.findallextensionnumbers("baz", &numbers));
    assert_eq(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    expect_eq(12, numbers[0]);
    expect_eq(13, numbers[1]);
  }

  {
    vector<int> numbers;
    expect_true(reverse_merged_.findallextensionnumbers("baz", &numbers));
    assert_eq(2, numbers.size());
    sort(numbers.begin(), numbers.end());
    expect_eq(12, numbers[0]);
    expect_eq(13, numbers[1]);
  }

  {
    // can't find extensions for a non-existent message.
    vector<int> numbers;
    expect_false(reverse_merged_.findallextensionnumbers("blah", &numbers));
  }
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google
