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

#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>

#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {

namespace {

#define expect_substring(needle, haystack) \
  expect_pred_format2(testing::issubstring, (needle), (haystack))

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

// -------------------------------------------------------------------

// a dummy implementation of sourcetree backed by a simple map.
class mocksourcetree : public sourcetree {
 public:
  mocksourcetree() {}
  ~mocksourcetree() {}

  void addfile(const string& name, const char* contents) {
    files_[name] = contents;
  }

  // implements sourcetree -------------------------------------------
  io::zerocopyinputstream* open(const string& filename) {
    const char* contents = findptrornull(files_, filename);
    if (contents == null) {
      return null;
    } else {
      return new io::arrayinputstream(contents, strlen(contents));
    }
  }

 private:
  hash_map<string, const char*> files_;
};

// ===================================================================

class importertest : public testing::test {
 protected:
  importertest()
    : importer_(&source_tree_, &error_collector_) {}

  void addfile(const string& filename, const char* text) {
    source_tree_.addfile(filename, text);
  }

  // return the collected error text
  string error() const { return error_collector_.text_; }

  mockerrorcollector error_collector_;
  mocksourcetree source_tree_;
  importer importer_;
};

test_f(importertest, import) {
  // test normal importing.
  addfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  const filedescriptor* file = importer_.import("foo.proto");
  expect_eq("", error_collector_.text_);
  assert_true(file != null);

  assert_eq(1, file->message_type_count());
  expect_eq("foo", file->message_type(0)->name());

  // importing again should return same object.
  expect_eq(file, importer_.import("foo.proto"));
}

test_f(importertest, importnested) {
  // test that importing a file which imports another file works.
  addfile("foo.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n"
    "message foo {\n"
    "  optional bar bar = 1;\n"
    "}\n");
  addfile("bar.proto",
    "syntax = \"proto2\";\n"
    "message bar {}\n");

  // note that both files are actually parsed by the first call to import()
  // here, since foo.proto imports bar.proto.  the second call just returns
  // the same protofile for bar.proto which was constructed while importing
  // foo.proto.  we test that this is the case below by checking that bar
  // is among foo's dependencies (by pointer).
  const filedescriptor* foo = importer_.import("foo.proto");
  const filedescriptor* bar = importer_.import("bar.proto");
  expect_eq("", error_collector_.text_);
  assert_true(foo != null);
  assert_true(bar != null);

  // check that foo's dependency is the same object as bar.
  assert_eq(1, foo->dependency_count());
  expect_eq(bar, foo->dependency(0));

  // check that foo properly cross-links bar.
  assert_eq(1, foo->message_type_count());
  assert_eq(1, bar->message_type_count());
  assert_eq(1, foo->message_type(0)->field_count());
  assert_eq(fielddescriptor::type_message,
            foo->message_type(0)->field(0)->type());
  expect_eq(bar->message_type(0),
            foo->message_type(0)->field(0)->message_type());
}

test_f(importertest, filenotfound) {
  // error:  parsing a file that doesn't exist.
  expect_true(importer_.import("foo.proto") == null);
  expect_eq(
    "foo.proto:-1:0: file not found.\n",
    error_collector_.text_);
}

test_f(importertest, importnotfound) {
  // error:  importing a file that doesn't exist.
  addfile("foo.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n");

  expect_true(importer_.import("foo.proto") == null);
  expect_eq(
    "bar.proto:-1:0: file not found.\n"
    "foo.proto:-1:0: import \"bar.proto\" was not found or had errors.\n",
    error_collector_.text_);
}

test_f(importertest, recursiveimport) {
  // error:  recursive import.
  addfile("recursive1.proto",
    "syntax = \"proto2\";\n"
    "import \"recursive2.proto\";\n");
  addfile("recursive2.proto",
    "syntax = \"proto2\";\n"
    "import \"recursive1.proto\";\n");

  expect_true(importer_.import("recursive1.proto") == null);
  expect_eq(
    "recursive1.proto:-1:0: file recursively imports itself: recursive1.proto "
      "-> recursive2.proto -> recursive1.proto\n"
    "recursive2.proto:-1:0: import \"recursive1.proto\" was not found "
      "or had errors.\n"
    "recursive1.proto:-1:0: import \"recursive2.proto\" was not found "
      "or had errors.\n",
    error_collector_.text_);
}

// todo(sanjay): the mapfield tests below more properly belong in
// descriptor_unittest, but are more convenient to test here.
test_f(importertest, mapfieldvalid) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message item {\n"
      "  required string key = 1;\n"
      "}\n"
      "message map {\n"
      "  repeated item items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  const filedescriptor* file = importer_.import("map.proto");
  assert_true(file != null) << error_collector_.text_;
  expect_eq("", error_collector_.text_);

  // check that map::items points to item::key
  const descriptor* item_type = file->findmessagetypebyname("item");
  assert_true(item_type != null);
  const descriptor* map_type = file->findmessagetypebyname("map");
  assert_true(map_type != null);
  const fielddescriptor* key_field = item_type->findfieldbyname("key");
  assert_true(key_field != null);
  const fielddescriptor* items_field = map_type->findfieldbyname("items");
  assert_true(items_field != null);
  expect_eq(items_field->experimental_map_key(), key_field);
}

test_f(importertest, mapfieldnotrepeated) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message item {\n"
      "  required string key = 1;\n"
      "}\n"
      "message map {\n"
      "  required item items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("only allowed for repeated fields", error());
}

test_f(importertest, mapfieldnotmessagetype) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message map {\n"
      "  repeated int32 items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("only allowed for fields with a message type", error());
}

test_f(importertest, mapfieldtypenotfound) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message map {\n"
      "  repeated unknown items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("not defined", error());
}

test_f(importertest, mapfieldkeynotfound) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message item {\n"
      "  required string key = 1;\n"
      "}\n"
      "message map {\n"
      "  repeated item items = 1 [experimental_map_key = \"badkey\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("could not find field", error());
}

test_f(importertest, mapfieldkeyrepeated) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message item {\n"
      "  repeated string key = 1;\n"
      "}\n"
      "message map {\n"
      "  repeated item items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("must not name a repeated field", error());
}

test_f(importertest, mapfieldkeynotscalar) {
  addfile(
      "map.proto",
      "syntax = \"proto2\";\n"
      "message itemkey { }\n"
      "message item {\n"
      "  required itemkey key = 1;\n"
      "}\n"
      "message map {\n"
      "  repeated item items = 1 [experimental_map_key = \"key\"];\n"
      "}\n"
      );
  expect_true(importer_.import("map.proto") == null);
  expect_substring("must name a scalar or string", error());
}

// ===================================================================

class disksourcetreetest : public testing::test {
 protected:
  virtual void setup() {
    dirnames_.push_back(testtempdir() + "/test_proto2_import_path_1");
    dirnames_.push_back(testtempdir() + "/test_proto2_import_path_2");

    for (int i = 0; i < dirnames_.size(); i++) {
      if (file::exists(dirnames_[i])) {
        file::deleterecursively(dirnames_[i], null, null);
      }
      google_check(file::createdir(dirnames_[i].c_str(), default_file_mode));
    }
  }

  virtual void teardown() {
    for (int i = 0; i < dirnames_.size(); i++) {
      file::deleterecursively(dirnames_[i], null, null);
    }
  }

  void addfile(const string& filename, const char* contents) {
    file::writestringtofileordie(contents, filename);
  }

  void addsubdir(const string& dirname) {
    google_check(file::createdir(dirname.c_str(), default_file_mode));
  }

  void expectfilecontents(const string& filename,
                          const char* expected_contents) {
    scoped_ptr<io::zerocopyinputstream> input(source_tree_.open(filename));

    assert_false(input == null);

    // read all the data from the file.
    string file_contents;
    const void* data;
    int size;
    while (input->next(&data, &size)) {
      file_contents.append(reinterpret_cast<const char*>(data), size);
    }

    expect_eq(expected_contents, file_contents);
  }

  void expectfilenotfound(const string& filename) {
    scoped_ptr<io::zerocopyinputstream> input(source_tree_.open(filename));
    expect_true(input == null);
  }

  disksourcetree source_tree_;

  // paths of two on-disk directories to use during the test.
  vector<string> dirnames_;
};

test_f(disksourcetreetest, maproot) {
  // test opening a file in a directory that is mapped to the root of the
  // source tree.
  addfile(dirnames_[0] + "/foo", "hello world!");
  source_tree_.mappath("", dirnames_[0]);

  expectfilecontents("foo", "hello world!");
  expectfilenotfound("bar");
}

test_f(disksourcetreetest, mapdirectory) {
  // test opening a file in a directory that is mapped to somewhere other
  // than the root of the source tree.

  addfile(dirnames_[0] + "/foo", "hello world!");
  source_tree_.mappath("baz", dirnames_[0]);

  expectfilecontents("baz/foo", "hello world!");
  expectfilenotfound("baz/bar");
  expectfilenotfound("foo");
  expectfilenotfound("bar");

  // non-canonical file names should not work.
  expectfilenotfound("baz//foo");
  expectfilenotfound("baz/../baz/foo");
  expectfilenotfound("baz/./foo");
  expectfilenotfound("baz/foo/");
}

test_f(disksourcetreetest, noparent) {
  // test that we cannot open files in a parent of a mapped directory.

  addfile(dirnames_[0] + "/foo", "hello world!");
  addsubdir(dirnames_[0] + "/bar");
  addfile(dirnames_[0] + "/bar/baz", "blah.");
  source_tree_.mappath("", dirnames_[0] + "/bar");

  expectfilecontents("baz", "blah.");
  expectfilenotfound("../foo");
  expectfilenotfound("../bar/baz");
}

test_f(disksourcetreetest, mapfile) {
  // test opening a file that is mapped directly into the source tree.

  addfile(dirnames_[0] + "/foo", "hello world!");
  source_tree_.mappath("foo", dirnames_[0] + "/foo");

  expectfilecontents("foo", "hello world!");
  expectfilenotfound("bar");
}

test_f(disksourcetreetest, searchmultipledirectories) {
  // test mapping and searching multiple directories.

  addfile(dirnames_[0] + "/foo", "hello world!");
  addfile(dirnames_[1] + "/foo", "this file should be hidden.");
  addfile(dirnames_[1] + "/bar", "goodbye world!");
  source_tree_.mappath("", dirnames_[0]);
  source_tree_.mappath("", dirnames_[1]);

  expectfilecontents("foo", "hello world!");
  expectfilecontents("bar", "goodbye world!");
  expectfilenotfound("baz");
}

test_f(disksourcetreetest, orderingtrumpsspecificity) {
  // test that directories are always searched in order, even when a latter
  // directory is more-specific than a former one.

  // create the "bar" directory so we can put a file in it.
  assert_true(file::createdir((dirnames_[0] + "/bar").c_str(),
                              default_file_mode));

  // add files and map paths.
  addfile(dirnames_[0] + "/bar/foo", "hello world!");
  addfile(dirnames_[1] + "/foo", "this file should be hidden.");
  source_tree_.mappath("", dirnames_[0]);
  source_tree_.mappath("bar", dirnames_[1]);

  // check.
  expectfilecontents("bar/foo", "hello world!");
}

test_f(disksourcetreetest, diskfiletovirtualfile) {
  // test diskfiletovirtualfile.

  addfile(dirnames_[0] + "/foo", "hello world!");
  addfile(dirnames_[1] + "/foo", "this file should be hidden.");
  source_tree_.mappath("bar", dirnames_[0]);
  source_tree_.mappath("bar", dirnames_[1]);

  string virtual_file;
  string shadowing_disk_file;

  expect_eq(disksourcetree::no_mapping,
    source_tree_.diskfiletovirtualfile(
      "/foo", &virtual_file, &shadowing_disk_file));

  expect_eq(disksourcetree::shadowed,
    source_tree_.diskfiletovirtualfile(
      dirnames_[1] + "/foo", &virtual_file, &shadowing_disk_file));
  expect_eq("bar/foo", virtual_file);
  expect_eq(dirnames_[0] + "/foo", shadowing_disk_file);

  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      dirnames_[1] + "/baz", &virtual_file, &shadowing_disk_file));
  expect_eq("bar/baz", virtual_file);

  expect_eq(disksourcetree::success,
    source_tree_.diskfiletovirtualfile(
      dirnames_[0] + "/foo", &virtual_file, &shadowing_disk_file));
  expect_eq("bar/foo", virtual_file);
}

test_f(disksourcetreetest, diskfiletovirtualfilecanonicalization) {
  // test handling of "..", ".", etc. in diskfiletovirtualfile().

  source_tree_.mappath("dir1", "..");
  source_tree_.mappath("dir2", "../../foo");
  source_tree_.mappath("dir3", "./foo/bar/.");
  source_tree_.mappath("dir4", ".");
  source_tree_.mappath("", "/qux");
  source_tree_.mappath("dir5", "/quux/");

  string virtual_file;
  string shadowing_disk_file;

  // "../.." should not be considered to be under "..".
  expect_eq(disksourcetree::no_mapping,
    source_tree_.diskfiletovirtualfile(
      "../../baz", &virtual_file, &shadowing_disk_file));

  // "/foo" is not mapped (it should not be misintepreted as being under ".").
  expect_eq(disksourcetree::no_mapping,
    source_tree_.diskfiletovirtualfile(
      "/foo", &virtual_file, &shadowing_disk_file));

#ifdef win32
  // "c:\foo" is not mapped (it should not be misintepreted as being under ".").
  expect_eq(disksourcetree::no_mapping,
    source_tree_.diskfiletovirtualfile(
      "c:\\foo", &virtual_file, &shadowing_disk_file));
#endif  // win32

  // but "../baz" should be.
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "../baz", &virtual_file, &shadowing_disk_file));
  expect_eq("dir1/baz", virtual_file);

  // "../../foo/baz" is under "../../foo".
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "../../foo/baz", &virtual_file, &shadowing_disk_file));
  expect_eq("dir2/baz", virtual_file);

  // "foo/./bar/baz" is under "./foo/bar/.".
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "foo/bar/baz", &virtual_file, &shadowing_disk_file));
  expect_eq("dir3/baz", virtual_file);

  // "bar" is under ".".
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "bar", &virtual_file, &shadowing_disk_file));
  expect_eq("dir4/bar", virtual_file);

  // "/qux/baz" is under "/qux".
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "/qux/baz", &virtual_file, &shadowing_disk_file));
  expect_eq("baz", virtual_file);

  // "/quux/bar" is under "/quux".
  expect_eq(disksourcetree::cannot_open,
    source_tree_.diskfiletovirtualfile(
      "/quux/bar", &virtual_file, &shadowing_disk_file));
  expect_eq("dir5/bar", virtual_file);
}

test_f(disksourcetreetest, virtualfiletodiskfile) {
  // test virtualfiletodiskfile.

  addfile(dirnames_[0] + "/foo", "hello world!");
  addfile(dirnames_[1] + "/foo", "this file should be hidden.");
  addfile(dirnames_[1] + "/quux", "this file should not be hidden.");
  source_tree_.mappath("bar", dirnames_[0]);
  source_tree_.mappath("bar", dirnames_[1]);

  // existent files, shadowed and non-shadowed case.
  string disk_file;
  expect_true(source_tree_.virtualfiletodiskfile("bar/foo", &disk_file));
  expect_eq(dirnames_[0] + "/foo", disk_file);
  expect_true(source_tree_.virtualfiletodiskfile("bar/quux", &disk_file));
  expect_eq(dirnames_[1] + "/quux", disk_file);

  // nonexistent file in existent directory and vice versa.
  string not_touched = "not touched";
  expect_false(source_tree_.virtualfiletodiskfile("bar/baz", &not_touched));
  expect_eq("not touched", not_touched);
  expect_false(source_tree_.virtualfiletodiskfile("baz/foo", &not_touched));
  expect_eq("not touched", not_touched);

  // accept null as output parameter.
  expect_true(source_tree_.virtualfiletodiskfile("bar/foo", null));
  expect_false(source_tree_.virtualfiletodiskfile("baz/foo", null));
}

}  // namespace

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
