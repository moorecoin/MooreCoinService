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
// generates c++ code for a given .proto file.

#ifndef google_protobuf_compiler_cpp_generator_h__
#define google_protobuf_compiler_cpp_generator_h__

#include <string>
#include <google/protobuf/compiler/code_generator.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

// codegenerator implementation which generates a c++ source file and
// header.  if you create your own protocol compiler binary and you want
// it to support c++ output, you can do so by registering an instance of this
// codegenerator with the commandlineinterface in your main() function.
class libprotoc_export cppgenerator : public codegenerator {
 public:
  cppgenerator();
  ~cppgenerator();

  // implements codegenerator ----------------------------------------
  bool generate(const filedescriptor* file,
                const string& parameter,
                generatorcontext* generator_context,
                string* error) const;

 private:
  google_disallow_evil_constructors(cppgenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_generator_h__