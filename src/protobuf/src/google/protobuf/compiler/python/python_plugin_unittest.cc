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
//
// todo(kenton):  share code with the versions of this test in other languages?
//   it seemed like parameterizing it would add more complexity than it is
//   worth.

#include <google/protobuf/compiler/python/python_generator.h>
#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/printer.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <google/protobuf/testing/file.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace python {
namespace {

class testgenerator : public codegenerator {
 public:
  testgenerator() {}
  ~testgenerator() {}

  virtual bool generate(const filedescriptor* file,
                        const string& parameter,
                        generatorcontext* context,
                        string* error) const {
    tryinsert("test_pb2.py", "imports", context);
    tryinsert("test_pb2.py", "module_scope", context);
    tryinsert("test_pb2.py", "class_scope:foo.bar", context);
    tryinsert("test_pb2.py", "class_scope:foo.bar.baz", context);
    return true;
  }

  void tryinsert(const string& filename, const string& insertion_point,
                 generatorcontext* context) const {
    scoped_ptr<io::zerocopyoutputstream> output(
      context->openforinsert(filename, insertion_point));
    io::printer printer(output.get(), '$');
    printer.print("// inserted $name$\n", "name", insertion_point);
  }
};

// this test verifies that all the expected insertion points exist.  it does
// not verify that they are correctly-placed; that would require actually
// compiling the output which is a bit more than i care to do for this test.
test(pythonplugintest, plugintest) {
  file::writestringtofileordie(
      "syntax = \"proto2\";\n"
      "package foo;\n"
      "message bar {\n"
      "  message baz {}\n"
      "}\n",
      testtempdir() + "/test.proto");

  google::protobuf::compiler::commandlineinterface cli;
  cli.setinputsareprotopathrelative(true);

  python::generator python_generator;
  testgenerator test_generator;
  cli.registergenerator("--python_out", &python_generator, "");
  cli.registergenerator("--test_out", &test_generator, "");

  string proto_path = "-i" + testtempdir();
  string python_out = "--python_out=" + testtempdir();
  string test_out = "--test_out=" + testtempdir();

  const char* argv[] = {
    "protoc",
    proto_path.c_str(),
    python_out.c_str(),
    test_out.c_str(),
    "test.proto"
  };

  expect_eq(0, cli.run(5, argv));
}

}  // namespace
}  // namespace python
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
