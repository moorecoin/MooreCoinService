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

#include <google/protobuf/compiler/command_line_interface.h>
#include <google/protobuf/compiler/cpp/cpp_generator.h>
#include <google/protobuf/compiler/python/python_generator.h>
#include <google/protobuf/compiler/java/java_generator.h>


int main(int argc, char* argv[]) {

  google::protobuf::compiler::commandlineinterface cli;
  cli.allowplugins("protoc-");

  // proto2 c++
  google::protobuf::compiler::cpp::cppgenerator cpp_generator;
  cli.registergenerator("--cpp_out", "--cpp_opt", &cpp_generator,
                        "generate c++ header and source.");

  // proto2 java
  google::protobuf::compiler::java::javagenerator java_generator;
  cli.registergenerator("--java_out", &java_generator,
                        "generate java source file.");


  // proto2 python
  google::protobuf::compiler::python::generator py_generator;
  cli.registergenerator("--python_out", &py_generator,
                        "generate python source file.");

  return cli.run(argc, argv);
}
