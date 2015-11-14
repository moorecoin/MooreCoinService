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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _msc_ver
#include <io.h>
#else
#include <unistd.h>
#endif
#include <vector>

#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/mock_code_generator.h>
#include <google/protobuf/compiler/subprocess.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/unittest.pb.h>
#include <google/protobuf/testing/file.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace compiler {

#if defined(_win32)
#ifndef stdin_fileno
#define stdin_fileno 0
#endif
#ifndef stdout_fileno
#define stdout_fileno 1
#endif
#ifndef f_ok
#define f_ok 00  // not defined by msvc for whatever reason
#endif
#endif

namespace {

class commandlineinterfacetest : public testing::test {
 protected:
  virtual void setup();
  virtual void teardown();

  // runs the commandlineinterface with the given command line.  the
  // command is automatically split on spaces, and the string "$tmpdir"
  // is replaced with testtempdir().
  void run(const string& command);

  // -----------------------------------------------------------------
  // methods to set up the test (called before run()).

  class nullcodegenerator;

  // normally plugins are allowed for all tests.  call this to explicitly
  // disable them.
  void disallowplugins() { disallow_plugins_ = true; }

  // create a temp file within temp_directory_ with the given name.
  // the containing directory is also created if necessary.
  void createtempfile(const string& name, const string& contents);

  // create a subdirectory within temp_directory_.
  void createtempdir(const string& name);

  void setinputsareprotopathrelative(bool enable) {
    cli_.setinputsareprotopathrelative(enable);
  }

  // -----------------------------------------------------------------
  // methods to check the test results (called after run()).

  // checks that no text was written to stderr during run(), and run()
  // returned 0.
  void expectnoerrors();

  // checks that run() returned non-zero and the stderr output is exactly
  // the text given.  expected_test may contain references to "$tmpdir",
  // which will be replaced by the temporary directory path.
  void expecterrortext(const string& expected_text);

  // checks that run() returned non-zero and the stderr contains the given
  // substring.
  void expecterrorsubstring(const string& expected_substring);

  // like expecterrorsubstring, but checks that run() returned zero.
  void expecterrorsubstringwithzeroreturncode(
      const string& expected_substring);

  // returns true if expecterrorsubstring(expected_substring) would pass, but
  // does not fail otherwise.
  bool hasalternateerrorsubstring(const string& expected_substring);

  // checks that mockcodegenerator::generate() was called in the given
  // context (or the generator in test_plugin.cc, which produces the same
  // output).  that is, this tests if the generator with the given name
  // was called with the given parameter and proto file and produced the
  // given output file.  this is checked by reading the output file and
  // checking that it contains the content that mockcodegenerator would
  // generate given these inputs.  message_name is the name of the first
  // message that appeared in the proto file; this is just to make extra
  // sure that the correct file was parsed.
  void expectgenerated(const string& generator_name,
                       const string& parameter,
                       const string& proto_name,
                       const string& message_name);
  void expectgenerated(const string& generator_name,
                       const string& parameter,
                       const string& proto_name,
                       const string& message_name,
                       const string& output_directory);
  void expectgeneratedwithmultipleinputs(const string& generator_name,
                                         const string& all_proto_names,
                                         const string& proto_name,
                                         const string& message_name);
  void expectgeneratedwithinsertions(const string& generator_name,
                                     const string& parameter,
                                     const string& insertions,
                                     const string& proto_name,
                                     const string& message_name);

  void expectnullcodegeneratorcalled(const string& parameter);

  void readdescriptorset(const string& filename,
                         filedescriptorset* descriptor_set);

 private:
  // the object we are testing.
  commandlineinterface cli_;

  // was disallowplugins() called?
  bool disallow_plugins_;

  // we create a directory within testtempdir() in order to add extra
  // protection against accidentally deleting user files (since we recursively
  // delete this directory during the test).  this is the full path of that
  // directory.
  string temp_directory_;

  // the result of run().
  int return_code_;

  // the captured stderr output.
  string error_text_;

  // pointers which need to be deleted later.
  vector<codegenerator*> mock_generators_to_delete_;

  nullcodegenerator* null_generator_;
};

class commandlineinterfacetest::nullcodegenerator : public codegenerator {
 public:
  nullcodegenerator() : called_(false) {}
  ~nullcodegenerator() {}

  mutable bool called_;
  mutable string parameter_;

  // implements codegenerator ----------------------------------------
  bool generate(const filedescriptor* file,
                const string& parameter,
                generatorcontext* context,
                string* error) const {
    called_ = true;
    parameter_ = parameter;
    return true;
  }
};

// ===================================================================

void commandlineinterfacetest::setup() {
  // most of these tests were written before this option was added, so we
  // run with the option on (which used to be the only way) except in certain
  // tests where we turn it off.
  cli_.setinputsareprotopathrelative(true);

  temp_directory_ = testtempdir() + "/proto2_cli_test_temp";

  // if the temp directory already exists, it must be left over from a
  // previous run.  delete it.
  if (file::exists(temp_directory_)) {
    file::deleterecursively(temp_directory_, null, null);
  }

  // create the temp directory.
  google_check(file::createdir(temp_directory_.c_str(), default_file_mode));

  // register generators.
  codegenerator* generator = new mockcodegenerator("test_generator");
  mock_generators_to_delete_.push_back(generator);
  cli_.registergenerator("--test_out", "--test_opt", generator, "test output.");
  cli_.registergenerator("-t", generator, "test output.");

  generator = new mockcodegenerator("alt_generator");
  mock_generators_to_delete_.push_back(generator);
  cli_.registergenerator("--alt_out", generator, "alt output.");

  generator = null_generator_ = new nullcodegenerator();
  mock_generators_to_delete_.push_back(generator);
  cli_.registergenerator("--null_out", generator, "null output.");

  disallow_plugins_ = false;
}

void commandlineinterfacetest::teardown() {
  // delete the temp directory.
  file::deleterecursively(temp_directory_, null, null);

  // delete all the mockcodegenerators.
  for (int i = 0; i < mock_generators_to_delete_.size(); i++) {
    delete mock_generators_to_delete_[i];
  }
  mock_generators_to_delete_.clear();
}

void commandlineinterfacetest::run(const string& command) {
  vector<string> args;
  splitstringusing(command, " ", &args);

  if (!disallow_plugins_) {
    cli_.allowplugins("prefix-");
    const char* possible_paths[] = {
      // when building with shared libraries, libtool hides the real executable
      // in .libs and puts a fake wrapper in the current directory.
      // unfortunately, due to an apparent bug on cygwin/mingw, if one program
      // wrapped in this way (e.g. protobuf-tests.exe) tries to execute another
      // program wrapped in this way (e.g. test_plugin.exe), the latter fails
      // with error code 127 and no explanation message.  presumably the problem
      // is that the wrapper for protobuf-tests.exe set some environment
      // variables that confuse the wrapper for test_plugin.exe.  luckily, it
      // turns out that if we simply invoke the wrapped test_plugin.exe
      // directly, it works -- i guess the environment variables set by the
      // protobuf-tests.exe wrapper happen to be correct for it too.  so we do
      // that.
      ".libs/test_plugin.exe",  // win32 w/autotool (cygwin / mingw)
      "test_plugin.exe",        // other win32 (msvc)
      "test_plugin",            // unix
    };

    string plugin_path;

    for (int i = 0; i < google_arraysize(possible_paths); i++) {
      if (access(possible_paths[i], f_ok) == 0) {
        plugin_path = possible_paths[i];
        break;
      }
    }

    if (plugin_path.empty()) {
      google_log(error)
          << "plugin executable not found.  plugin tests are likely to fail.";
    } else {
      args.push_back("--plugin=prefix-gen-plug=" + plugin_path);
    }
  }

  scoped_array<const char*> argv(new const char*[args.size()]);

  for (int i = 0; i < args.size(); i++) {
    args[i] = stringreplace(args[i], "$tmpdir", temp_directory_, true);
    argv[i] = args[i].c_str();
  }

  captureteststderr();

  return_code_ = cli_.run(args.size(), argv.get());

  error_text_ = getcapturedteststderr();
}

// -------------------------------------------------------------------

void commandlineinterfacetest::createtempfile(
    const string& name,
    const string& contents) {
  // create parent directory, if necessary.
  string::size_type slash_pos = name.find_last_of('/');
  if (slash_pos != string::npos) {
    string dir = name.substr(0, slash_pos);
    file::recursivelycreatedir(temp_directory_ + "/" + dir, 0777);
  }

  // write file.
  string full_name = temp_directory_ + "/" + name;
  file::writestringtofileordie(contents, full_name);
}

void commandlineinterfacetest::createtempdir(const string& name) {
  file::recursivelycreatedir(temp_directory_ + "/" + name, 0777);
}

// -------------------------------------------------------------------

void commandlineinterfacetest::expectnoerrors() {
  expect_eq(0, return_code_);
  expect_eq("", error_text_);
}

void commandlineinterfacetest::expecterrortext(const string& expected_text) {
  expect_ne(0, return_code_);
  expect_eq(stringreplace(expected_text, "$tmpdir", temp_directory_, true),
            error_text_);
}

void commandlineinterfacetest::expecterrorsubstring(
    const string& expected_substring) {
  expect_ne(0, return_code_);
  expect_pred_format2(testing::issubstring, expected_substring, error_text_);
}

void commandlineinterfacetest::expecterrorsubstringwithzeroreturncode(
    const string& expected_substring) {
  expect_eq(0, return_code_);
  expect_pred_format2(testing::issubstring, expected_substring, error_text_);
}

bool commandlineinterfacetest::hasalternateerrorsubstring(
    const string& expected_substring) {
  expect_ne(0, return_code_);
  return error_text_.find(expected_substring) != string::npos;
}

void commandlineinterfacetest::expectgenerated(
    const string& generator_name,
    const string& parameter,
    const string& proto_name,
    const string& message_name) {
  mockcodegenerator::expectgenerated(
      generator_name, parameter, "", proto_name, message_name, proto_name,
      temp_directory_);
}

void commandlineinterfacetest::expectgenerated(
    const string& generator_name,
    const string& parameter,
    const string& proto_name,
    const string& message_name,
    const string& output_directory) {
  mockcodegenerator::expectgenerated(
      generator_name, parameter, "", proto_name, message_name, proto_name,
      temp_directory_ + "/" + output_directory);
}

void commandlineinterfacetest::expectgeneratedwithmultipleinputs(
    const string& generator_name,
    const string& all_proto_names,
    const string& proto_name,
    const string& message_name) {
  mockcodegenerator::expectgenerated(
      generator_name, "", "", proto_name, message_name,
      all_proto_names,
      temp_directory_);
}

void commandlineinterfacetest::expectgeneratedwithinsertions(
    const string& generator_name,
    const string& parameter,
    const string& insertions,
    const string& proto_name,
    const string& message_name) {
  mockcodegenerator::expectgenerated(
      generator_name, parameter, insertions, proto_name, message_name,
      proto_name, temp_directory_);
}

void commandlineinterfacetest::expectnullcodegeneratorcalled(
    const string& parameter) {
  expect_true(null_generator_->called_);
  expect_eq(parameter, null_generator_->parameter_);
}

void commandlineinterfacetest::readdescriptorset(
    const string& filename, filedescriptorset* descriptor_set) {
  string path = temp_directory_ + "/" + filename;
  string file_contents;
  if (!file::readfiletostring(path, &file_contents)) {
    fail() << "file not found: " << path;
  }
  if (!descriptor_set->parsefromstring(file_contents)) {
    fail() << "could not parse file contents: " << path;
  }
}

// ===================================================================

test_f(commandlineinterfacetest, basicoutput) {
  // test that the common case works.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, basicplugin) {
  // test that basic plugins work.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_plugin", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, generatorandplugin) {
  // invoke a generator and a plugin at the same time.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
  expectgenerated("test_plugin", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, multipleinputs) {
  // test parsing multiple input files.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "message bar {}\n");

  run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto bar.proto");

  expectnoerrors();
  expectgeneratedwithmultipleinputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "foo");
  expectgeneratedwithmultipleinputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "bar");
  expectgeneratedwithmultipleinputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "foo");
  expectgeneratedwithmultipleinputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "bar");
}

test_f(commandlineinterfacetest, multipleinputswithimport) {
  // test parsing multiple input files with an import of a separate file.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "import \"baz.proto\";\n"
    "message bar {\n"
    "  optional baz a = 1;\n"
    "}\n");
  createtempfile("baz.proto",
    "syntax = \"proto2\";\n"
    "message baz {}\n");

  run("protocol_compiler --test_out=$tmpdir --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto bar.proto");

  expectnoerrors();
  expectgeneratedwithmultipleinputs("test_generator", "foo.proto,bar.proto",
                                    "foo.proto", "foo");
  expectgeneratedwithmultipleinputs("test_generator", "foo.proto,bar.proto",
                                    "bar.proto", "bar");
  expectgeneratedwithmultipleinputs("test_plugin", "foo.proto,bar.proto",
                                    "foo.proto", "foo");
  expectgeneratedwithmultipleinputs("test_plugin", "foo.proto,bar.proto",
                                    "bar.proto", "bar");
}

test_f(commandlineinterfacetest, createdirectory) {
  // test that when we output to a sub-directory, it is created.

  createtempfile("bar/baz/foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempdir("out");
  createtempdir("plugout");

  run("protocol_compiler --test_out=$tmpdir/out --plug_out=$tmpdir/plugout "
      "--proto_path=$tmpdir bar/baz/foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "bar/baz/foo.proto", "foo", "out");
  expectgenerated("test_plugin", "", "bar/baz/foo.proto", "foo", "plugout");
}

test_f(commandlineinterfacetest, generatorparameters) {
  // test that generator parameters are correctly parsed from the command line.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=testparameter:$tmpdir "
      "--plug_out=testpluginparameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "testparameter", "foo.proto", "foo");
  expectgenerated("test_plugin", "testpluginparameter", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, extrageneratorparameters) {
  // test that generator parameters specified with the option flag are
  // correctly passed to the code generator.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  // create the "a" and "b" sub-directories.
  createtempdir("a");
  createtempdir("b");

  run("protocol_compiler "
      "--test_opt=foo1 "
      "--test_out=bar:$tmpdir/a "
      "--test_opt=foo2 "
      "--test_out=baz:$tmpdir/b "
      "--test_opt=foo3 "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated(
      "test_generator", "bar,foo1,foo2,foo3", "foo.proto", "foo", "a");
  expectgenerated(
      "test_generator", "baz,foo1,foo2,foo3", "foo.proto", "foo", "b");
}

test_f(commandlineinterfacetest, insert) {
  // test running a generator that inserts code into another's output.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler "
      "--test_out=testparameter:$tmpdir "
      "--plug_out=testpluginparameter:$tmpdir "
      "--test_out=insert=test_generator,test_plugin:$tmpdir "
      "--plug_out=insert=test_generator,test_plugin:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgeneratedwithinsertions(
      "test_generator", "testparameter", "test_generator,test_plugin",
      "foo.proto", "foo");
  expectgeneratedwithinsertions(
      "test_plugin", "testpluginparameter", "test_generator,test_plugin",
      "foo.proto", "foo");
}

#if defined(_win32)

test_f(commandlineinterfacetest, windowsoutputpath) {
  // test that the output path can be a windows-style path.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n");

  run("protocol_compiler --null_out=c:\\ "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectnullcodegeneratorcalled("");
}

test_f(commandlineinterfacetest, windowsoutputpathandparameter) {
  // test that we can have a windows-style output path and a parameter.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n");

  run("protocol_compiler --null_out=bar:c:\\ "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectnullcodegeneratorcalled("bar");
}

test_f(commandlineinterfacetest, trailingbackslash) {
  // test that the directories can end in backslashes.  some users claim this
  // doesn't work on their system.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir\\ "
      "--proto_path=$tmpdir\\ foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

#endif  // defined(_win32) || defined(__cygwin__)

test_f(commandlineinterfacetest, pathlookup) {
  // test that specifying multiple directories in the proto search path works.

  createtempfile("b/bar.proto",
    "syntax = \"proto2\";\n"
    "message bar {}\n");
  createtempfile("a/foo.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n"
    "message foo {\n"
    "  optional bar a = 1;\n"
    "}\n");
  createtempfile("b/foo.proto", "this should not be parsed\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/a --proto_path=$tmpdir/b foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, colondelimitedpath) {
  // same as pathlookup, but we provide the proto_path in a single flag.

  createtempfile("b/bar.proto",
    "syntax = \"proto2\";\n"
    "message bar {}\n");
  createtempfile("a/foo.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n"
    "message foo {\n"
    "  optional bar a = 1;\n"
    "}\n");
  createtempfile("b/foo.proto", "this should not be parsed\n");

#undef path_separator
#if defined(_win32)
#define path_separator ";"
#else
#define path_separator ":"
#endif

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/a"path_separator"$tmpdir/b foo.proto");

#undef path_separator

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, nonrootmapping) {
  // test setting up a search path mapping a directory to a non-root location.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=bar=$tmpdir bar/foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "bar/foo.proto", "foo");
}

test_f(commandlineinterfacetest, multiplegenerators) {
  // test that we can have multiple generators and use both in one invocation,
  // each with a different output directory.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  // create the "a" and "b" sub-directories.
  createtempdir("a");
  createtempdir("b");

  run("protocol_compiler "
      "--test_out=$tmpdir/a "
      "--alt_out=$tmpdir/b "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo", "a");
  expectgenerated("alt_generator", "", "foo.proto", "foo", "b");
}

test_f(commandlineinterfacetest, disallowservicesnoservices) {
  // test that --disallow_services doesn't cause a problem when there are no
  // services.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --disallow_services --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, disallowserviceshasservice) {
  // test that --disallow_services produces an error when there are services.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n"
    "service bar {}\n");

  run("protocol_compiler --disallow_services --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring("foo.proto: this file contains services");
}

test_f(commandlineinterfacetest, allowserviceshasservice) {
  // test that services work fine as long as --disallow_services is not used.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n"
    "service bar {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, cwdrelativeinputs) {
  // test that we can accept working-directory-relative input files.

  setinputsareprotopathrelative(false);

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir $tmpdir/foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, writedescriptorset) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "import \"foo.proto\";\n"
    "message bar {\n"
    "  optional foo foo = 1;\n"
    "}\n");

  run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--proto_path=$tmpdir bar.proto");

  expectnoerrors();

  filedescriptorset descriptor_set;
  readdescriptorset("descriptor_set", &descriptor_set);
  if (hasfatalfailure()) return;
  assert_eq(1, descriptor_set.file_size());
  expect_eq("bar.proto", descriptor_set.file(0).name());
  // descriptor set should not have source code info.
  expect_false(descriptor_set.file(0).has_source_code_info());
}

test_f(commandlineinterfacetest, writedescriptorsetwithsourceinfo) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "import \"foo.proto\";\n"
    "message bar {\n"
    "  optional foo foo = 1;\n"
    "}\n");

  run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_source_info --proto_path=$tmpdir bar.proto");

  expectnoerrors();

  filedescriptorset descriptor_set;
  readdescriptorset("descriptor_set", &descriptor_set);
  if (hasfatalfailure()) return;
  assert_eq(1, descriptor_set.file_size());
  expect_eq("bar.proto", descriptor_set.file(0).name());
  // source code info included.
  expect_true(descriptor_set.file(0).has_source_code_info());
}

test_f(commandlineinterfacetest, writetransitivedescriptorset) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "import \"foo.proto\";\n"
    "message bar {\n"
    "  optional foo foo = 1;\n"
    "}\n");

  run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_imports --proto_path=$tmpdir bar.proto");

  expectnoerrors();

  filedescriptorset descriptor_set;
  readdescriptorset("descriptor_set", &descriptor_set);
  if (hasfatalfailure()) return;
  assert_eq(2, descriptor_set.file_size());
  if (descriptor_set.file(0).name() == "bar.proto") {
    std::swap(descriptor_set.mutable_file()->mutable_data()[0],
              descriptor_set.mutable_file()->mutable_data()[1]);
  }
  expect_eq("foo.proto", descriptor_set.file(0).name());
  expect_eq("bar.proto", descriptor_set.file(1).name());
  // descriptor set should not have source code info.
  expect_false(descriptor_set.file(0).has_source_code_info());
  expect_false(descriptor_set.file(1).has_source_code_info());
}

test_f(commandlineinterfacetest, writetransitivedescriptorsetwithsourceinfo) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "import \"foo.proto\";\n"
    "message bar {\n"
    "  optional foo foo = 1;\n"
    "}\n");

  run("protocol_compiler --descriptor_set_out=$tmpdir/descriptor_set "
      "--include_imports --include_source_info --proto_path=$tmpdir bar.proto");

  expectnoerrors();

  filedescriptorset descriptor_set;
  readdescriptorset("descriptor_set", &descriptor_set);
  if (hasfatalfailure()) return;
  assert_eq(2, descriptor_set.file_size());
  if (descriptor_set.file(0).name() == "bar.proto") {
    std::swap(descriptor_set.mutable_file()->mutable_data()[0],
              descriptor_set.mutable_file()->mutable_data()[1]);
  }
  expect_eq("foo.proto", descriptor_set.file(0).name());
  expect_eq("bar.proto", descriptor_set.file(1).name());
  // source code info included.
  expect_true(descriptor_set.file(0).has_source_code_info());
  expect_true(descriptor_set.file(1).has_source_code_info());
}

// -------------------------------------------------------------------

test_f(commandlineinterfacetest, parseerrors) {
  // test that parse errors are reported.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "badsyntax\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrortext(
    "foo.proto:2:1: expected top-level statement (e.g. \"message\").\n");
}

test_f(commandlineinterfacetest, parseerrorsmultiplefiles) {
  // test that parse errors are reported from multiple files.

  // we set up files such that foo.proto actually depends on bar.proto in
  // two ways:  directly and through baz.proto.  bar.proto's errors should
  // only be reported once.
  createtempfile("bar.proto",
    "syntax = \"proto2\";\n"
    "badsyntax\n");
  createtempfile("baz.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n");
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "import \"bar.proto\";\n"
    "import \"baz.proto\";\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrortext(
    "bar.proto:2:1: expected top-level statement (e.g. \"message\").\n"
    "baz.proto: import \"bar.proto\" was not found or had errors.\n"
    "foo.proto: import \"bar.proto\" was not found or had errors.\n"
    "foo.proto: import \"baz.proto\" was not found or had errors.\n");
}

test_f(commandlineinterfacetest, inputnotfounderror) {
  // test what happens if the input file is not found.

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrortext(
    "foo.proto: file not found.\n");
}

test_f(commandlineinterfacetest, cwdrelativeinputnotfounderror) {
  // test what happens when a working-directory-relative input file is not
  // found.

  setinputsareprotopathrelative(false);

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir $tmpdir/foo.proto");

  expecterrortext(
    "$tmpdir/foo.proto: no such file or directory\n");
}

test_f(commandlineinterfacetest, cwdrelativeinputnotmappederror) {
  // test what happens when a working-directory-relative input file is not
  // mapped to a virtual path.

  setinputsareprotopathrelative(false);

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  // create a directory called "bar" so that we can point --proto_path at it.
  createtempfile("bar/dummy", "");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/bar $tmpdir/foo.proto");

  expecterrortext(
    "$tmpdir/foo.proto: file does not reside within any path "
      "specified using --proto_path (or -i).  you must specify a "
      "--proto_path which encompasses this file.  note that the "
      "proto_path must be an exact prefix of the .proto file "
      "names -- protoc is too dumb to figure out when two paths "
      "(e.g. absolute and relative) are equivalent (it's harder "
      "than you think).\n");
}

test_f(commandlineinterfacetest, cwdrelativeinputnotfoundandnotmappederror) {
  // check what happens if the input file is not found *and* is not mapped
  // in the proto_path.

  setinputsareprotopathrelative(false);

  // create a directory called "bar" so that we can point --proto_path at it.
  createtempfile("bar/dummy", "");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/bar $tmpdir/foo.proto");

  expecterrortext(
    "$tmpdir/foo.proto: no such file or directory\n");
}

test_f(commandlineinterfacetest, cwdrelativeinputshadowederror) {
  // test what happens when a working-directory-relative input file is shadowed
  // by another file in the virtual path.

  setinputsareprotopathrelative(false);

  createtempfile("foo/foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");
  createtempfile("bar/foo.proto",
    "syntax = \"proto2\";\n"
    "message bar {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/foo --proto_path=$tmpdir/bar "
      "$tmpdir/bar/foo.proto");

  expecterrortext(
    "$tmpdir/bar/foo.proto: input is shadowed in the --proto_path "
    "by \"$tmpdir/foo/foo.proto\".  either use the latter "
    "file as your input or reorder the --proto_path so that the "
    "former file's location comes first.\n");
}

test_f(commandlineinterfacetest, protopathnotfounderror) {
  // test what happens if the input file is not found.

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir/foo foo.proto");

  expecterrortext(
    "$tmpdir/foo: warning: directory does not exist.\n"
    "foo.proto: file not found.\n");
}

test_f(commandlineinterfacetest, missinginputerror) {
  // test that we get an error if no inputs are given.

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir");

  expecterrortext("missing input file.\n");
}

test_f(commandlineinterfacetest, missingoutputerror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --proto_path=$tmpdir foo.proto");

  expecterrortext("missing output directives.\n");
}

test_f(commandlineinterfacetest, outputwriteerror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  string output_file =
      mockcodegenerator::getoutputfilename("test_generator", "foo.proto");

  // create a directory blocking our output location.
  createtempdir(output_file);

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  // mockcodegenerator no longer detects an error because we actually write to
  // an in-memory location first, then dump to disk at the end.  this is no
  // big deal.
  //   expecterrorsubstring("mockcodegenerator detected write error.");

#if defined(_win32) && !defined(__cygwin__)
  // windows with msvcrt.dll produces eperm instead of eisdir.
  if (hasalternateerrorsubstring(output_file + ": permission denied")) {
    return;
  }
#endif

  expecterrorsubstring(output_file + ": is a directory");
}

test_f(commandlineinterfacetest, pluginoutputwriteerror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  string output_file =
      mockcodegenerator::getoutputfilename("test_plugin", "foo.proto");

  // create a directory blocking our output location.
  createtempdir(output_file);

  run("protocol_compiler --plug_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

#if defined(_win32) && !defined(__cygwin__)
  // windows with msvcrt.dll produces eperm instead of eisdir.
  if (hasalternateerrorsubstring(output_file + ": permission denied")) {
    return;
  }
#endif

  expecterrorsubstring(output_file + ": is a directory");
}

test_f(commandlineinterfacetest, outputdirectorynotfounderror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir/nosuchdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring("nosuchdir/: no such file or directory");
}

test_f(commandlineinterfacetest, pluginoutputdirectorynotfounderror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --plug_out=$tmpdir/nosuchdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring("nosuchdir/: no such file or directory");
}

test_f(commandlineinterfacetest, outputdirectoryisfileerror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out=$tmpdir/foo.proto "
      "--proto_path=$tmpdir foo.proto");

#if defined(_win32) && !defined(__cygwin__)
  // windows with msvcrt.dll produces einval instead of enotdir.
  if (hasalternateerrorsubstring("foo.proto/: invalid argument")) {
    return;
  }
#endif

  expecterrorsubstring("foo.proto/: not a directory");
}

test_f(commandlineinterfacetest, generatorerror) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message mockcodegenerator_error {}\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring(
      "--test_out: foo.proto: saw message type mockcodegenerator_error.");
}

test_f(commandlineinterfacetest, generatorpluginerror) {
  // test a generator plugin that returns an error.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message mockcodegenerator_error {}\n");

  run("protocol_compiler --plug_out=testparameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring(
      "--plug_out: foo.proto: saw message type mockcodegenerator_error.");
}

test_f(commandlineinterfacetest, generatorpluginfail) {
  // test a generator plugin that exits with an error code.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message mockcodegenerator_exit {}\n");

  run("protocol_compiler --plug_out=testparameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring("saw message type mockcodegenerator_exit.");
  expecterrorsubstring(
      "--plug_out: prefix-gen-plug: plugin failed with status code 123.");
}

test_f(commandlineinterfacetest, generatorplugincrash) {
  // test a generator plugin that crashes.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message mockcodegenerator_abort {}\n");

  run("protocol_compiler --plug_out=testparameter:$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expecterrorsubstring("saw message type mockcodegenerator_abort.");

#ifdef _win32
  // windows doesn't have signals.  it looks like abort()ing causes the process
  // to exit with status code 3, but let's not depend on the exact number here.
  expecterrorsubstring(
      "--plug_out: prefix-gen-plug: plugin failed with status code");
#else
  // don't depend on the exact signal number.
  expecterrorsubstring(
      "--plug_out: prefix-gen-plug: plugin killed by signal");
#endif
}

test_f(commandlineinterfacetest, pluginreceivessourcecodeinfo) {
  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message mockcodegenerator_hassourcecodeinfo {}\n");

  run("protocol_compiler --plug_out=$tmpdir --proto_path=$tmpdir foo.proto");

  expecterrorsubstring(
      "saw message type mockcodegenerator_hassourcecodeinfo: 1.");
}

test_f(commandlineinterfacetest, generatorpluginnotfound) {
  // test what happens if the plugin isn't found.

  createtempfile("error.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --badplug_out=testparameter:$tmpdir "
      "--plugin=prefix-gen-badplug=no_such_file "
      "--proto_path=$tmpdir error.proto");

#ifdef _win32
  expecterrorsubstring("--badplug_out: prefix-gen-badplug: " +
      subprocess::win32errormessage(error_file_not_found));
#else
  // error written to stdout by child process after exec() fails.
  expecterrorsubstring(
      "no_such_file: program not found or is not executable");

  // error written by parent process when child fails.
  expecterrorsubstring(
      "--badplug_out: prefix-gen-badplug: plugin failed with status code 1.");
#endif
}

test_f(commandlineinterfacetest, generatorpluginnotallowed) {
  // test what happens if plugins aren't allowed.

  createtempfile("error.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  disallowplugins();
  run("protocol_compiler --plug_out=testparameter:$tmpdir "
      "--proto_path=$tmpdir error.proto");

  expecterrorsubstring("unknown flag: --plug_out");
}

test_f(commandlineinterfacetest, helptext) {
  run("test_exec_name --help");

  expecterrorsubstringwithzeroreturncode("usage: test_exec_name ");
  expecterrorsubstringwithzeroreturncode("--test_out=out_dir");
  expecterrorsubstringwithzeroreturncode("test output.");
  expecterrorsubstringwithzeroreturncode("--alt_out=out_dir");
  expecterrorsubstringwithzeroreturncode("alt output.");
}

test_f(commandlineinterfacetest, gccformaterrors) {
  // test --error_format=gcc (which is the default, but we want to verify
  // that it can be set explicitly).

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "badsyntax\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=gcc foo.proto");

  expecterrortext(
    "foo.proto:2:1: expected top-level statement (e.g. \"message\").\n");
}

test_f(commandlineinterfacetest, msvsformaterrors) {
  // test --error_format=msvs

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "badsyntax\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=msvs foo.proto");

  expecterrortext(
    "$tmpdir/foo.proto(2) : error in column=1: expected top-level statement "
      "(e.g. \"message\").\n");
}

test_f(commandlineinterfacetest, invaliderrorformat) {
  // test --error_format=msvs

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "badsyntax\n");

  run("protocol_compiler --test_out=$tmpdir "
      "--proto_path=$tmpdir --error_format=invalid foo.proto");

  expecterrortext(
    "unknown error format: invalid\n");
}

// -------------------------------------------------------------------
// flag parsing tests

test_f(commandlineinterfacetest, parsesinglecharacterflag) {
  // test that a single-character flag works.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler -t$tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, parsespacedelimitedvalue) {
  // test that separating the flag value with a space works.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler --test_out $tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, parsesinglecharacterspacedelimitedvalue) {
  // test that separating the flag value with a space works for
  // single-character flags.

  createtempfile("foo.proto",
    "syntax = \"proto2\";\n"
    "message foo {}\n");

  run("protocol_compiler -t $tmpdir "
      "--proto_path=$tmpdir foo.proto");

  expectnoerrors();
  expectgenerated("test_generator", "", "foo.proto", "foo");
}

test_f(commandlineinterfacetest, missingvalueerror) {
  // test that we get an error if a flag is missing its value.

  run("protocol_compiler --test_out --proto_path=$tmpdir foo.proto");

  expecterrortext("missing value for flag: --test_out\n");
}

test_f(commandlineinterfacetest, missingvalueatenderror) {
  // test that we get an error if the last argument is a flag requiring a
  // value.

  run("protocol_compiler --test_out");

  expecterrortext("missing value for flag: --test_out\n");
}

// ===================================================================

// test for --encode and --decode.  note that it would be easier to do this
// test as a shell script, but we'd like to be able to run the test on
// platforms that don't have a bourne-compatible shell available (especially
// windows/msvc).
class encodedecodetest : public testing::test {
 protected:
  virtual void setup() {
    duped_stdin_ = dup(stdin_fileno);
  }

  virtual void teardown() {
    dup2(duped_stdin_, stdin_fileno);
    close(duped_stdin_);
  }

  void redirectstdinfromtext(const string& input) {
    string filename = testtempdir() + "/test_stdin";
    file::writestringtofileordie(input, filename);
    google_check(redirectstdinfromfile(filename));
  }

  bool redirectstdinfromfile(const string& filename) {
    int fd = open(filename.c_str(), o_rdonly);
    if (fd < 0) return false;
    dup2(fd, stdin_fileno);
    close(fd);
    return true;
  }

  // remove '\r' characters from text.
  string stripcr(const string& text) {
    string result;

    for (int i = 0; i < text.size(); i++) {
      if (text[i] != '\r') {
        result.push_back(text[i]);
      }
    }

    return result;
  }

  enum type { text, binary };
  enum returncode { success, error };

  bool run(const string& command) {
    vector<string> args;
    args.push_back("protoc");
    splitstringusing(command, " ", &args);
    args.push_back("--proto_path=" + testsourcedir());

    scoped_array<const char*> argv(new const char*[args.size()]);
    for (int i = 0; i < args.size(); i++) {
      argv[i] = args[i].c_str();
    }

    commandlineinterface cli;
    cli.setinputsareprotopathrelative(true);

    captureteststdout();
    captureteststderr();

    int result = cli.run(args.size(), argv.get());

    captured_stdout_ = getcapturedteststdout();
    captured_stderr_ = getcapturedteststderr();

    return result == 0;
  }

  void expectstdoutmatchesbinaryfile(const string& filename) {
    string expected_output;
    assert_true(file::readfiletostring(filename, &expected_output));

    // don't use expect_eq because we don't want to print raw binary data to
    // stdout on failure.
    expect_true(captured_stdout_ == expected_output);
  }

  void expectstdoutmatchestextfile(const string& filename) {
    string expected_output;
    assert_true(file::readfiletostring(filename, &expected_output));

    expectstdoutmatchestext(expected_output);
  }

  void expectstdoutmatchestext(const string& expected_text) {
    expect_eq(stripcr(expected_text), stripcr(captured_stdout_));
  }

  void expectstderrmatchestext(const string& expected_text) {
    expect_eq(stripcr(expected_text), stripcr(captured_stderr_));
  }

 private:
  int duped_stdin_;
  string captured_stdout_;
  string captured_stderr_;
};

test_f(encodedecodetest, encode) {
  redirectstdinfromfile(testsourcedir() +
    "/google/protobuf/testdata/text_format_unittest_data.txt");
  expect_true(run("google/protobuf/unittest.proto "
                  "--encode=protobuf_unittest.testalltypes"));
  expectstdoutmatchesbinaryfile(testsourcedir() +
    "/google/protobuf/testdata/golden_message");
  expectstderrmatchestext("");
}

test_f(encodedecodetest, decode) {
  redirectstdinfromfile(testsourcedir() +
    "/google/protobuf/testdata/golden_message");
  expect_true(run("google/protobuf/unittest.proto "
                  "--decode=protobuf_unittest.testalltypes"));
  expectstdoutmatchestextfile(testsourcedir() +
    "/google/protobuf/testdata/text_format_unittest_data.txt");
  expectstderrmatchestext("");
}

test_f(encodedecodetest, partial) {
  redirectstdinfromtext("");
  expect_true(run("google/protobuf/unittest.proto "
                  "--encode=protobuf_unittest.testrequired"));
  expectstdoutmatchestext("");
  expectstderrmatchestext(
    "warning:  input message is missing required fields:  a, b, c\n");
}

test_f(encodedecodetest, decoderaw) {
  protobuf_unittest::testalltypes message;
  message.set_optional_int32(123);
  message.set_optional_string("foo");
  string data;
  message.serializetostring(&data);

  redirectstdinfromtext(data);
  expect_true(run("--decode_raw"));
  expectstdoutmatchestext("1: 123\n"
                          "14: \"foo\"\n");
  expectstderrmatchestext("");
}

test_f(encodedecodetest, unknowntype) {
  expect_false(run("google/protobuf/unittest.proto "
                   "--encode=nosuchtype"));
  expectstdoutmatchestext("");
  expectstderrmatchestext("type not defined: nosuchtype\n");
}

test_f(encodedecodetest, protoparseerror) {
  expect_false(run("google/protobuf/no_such_file.proto "
                   "--encode=nosuchtype"));
  expectstdoutmatchestext("");
  expectstderrmatchestext(
    "google/protobuf/no_such_file.proto: file not found.\n");
}

}  // anonymous namespace

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
