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
// front-end for protoc code generator plugins written in c++.
//
// to implement a protoc plugin in c++, simply write an implementation of
// codegenerator, then create a main() function like:
//   int main(int argc, char* argv[]) {
//     mycodegenerator generator;
//     return google::protobuf::compiler::pluginmain(argc, argv, &generator);
//   }
// you must link your plugin against libprotobuf and libprotoc.
//
// to get protoc to use the plugin, do one of the following:
// * place the plugin binary somewhere in the path and give it the name
//   "protoc-gen-name" (replacing "name" with the name of your plugin).  if you
//   then invoke protoc with the parameter --name_out=out_dir (again, replace
//   "name" with your plugin's name), protoc will invoke your plugin to generate
//   the output, which will be placed in out_dir.
// * place the plugin binary anywhere, with any name, and pass the --plugin
//   parameter to protoc to direct it to your plugin like so:
//     protoc --plugin=protoc-gen-name=path/to/mybinary --name_out=out_dir
//   on windows, make sure to include the .exe suffix:
//     protoc --plugin=protoc-gen-name=path/to/mybinary.exe --name_out=out_dir

#ifndef google_protobuf_compiler_plugin_h__
#define google_protobuf_compiler_plugin_h__

#include <google/protobuf/stubs/common.h>
namespace google {
namespace protobuf {
namespace compiler {

class codegenerator;    // code_generator.h

// implements main() for a protoc plugin exposing the given code generator.
libprotoc_export int pluginmain(int argc, char* argv[], const codegenerator* generator);

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_plugin_h__
