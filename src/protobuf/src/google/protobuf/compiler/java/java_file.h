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

#ifndef google_protobuf_compiler_java_file_h__
#define google_protobuf_compiler_java_file_h__

#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
  class filedescriptor;        // descriptor.h
  namespace io {
    class printer;             // printer.h
  }
  namespace compiler {
    class generatorcontext;     // code_generator.h
  }
}

namespace protobuf {
namespace compiler {
namespace java {

class filegenerator {
 public:
  explicit filegenerator(const filedescriptor* file);
  ~filegenerator();

  // checks for problems that would otherwise lead to cryptic compile errors.
  // returns true if there are no problems, or writes an error description to
  // the given string and returns false otherwise.
  bool validate(string* error);

  void generate(io::printer* printer);

  // if we aren't putting everything into one file, this will write all the
  // files other than the outer file (i.e. one for each message, enum, and
  // service type).
  void generatesiblings(const string& package_dir,
                        generatorcontext* generator_context,
                        vector<string>* file_list);

  const string& java_package() { return java_package_; }
  const string& classname()    { return classname_;    }


 private:
  // returns whether the dependency should be included in the output file.
  // always returns true for opensource, but used internally at google to help
  // improve compatibility with version 1 of protocol buffers.
  bool shouldincludedependency(const filedescriptor* descriptor);

  const filedescriptor* file_;
  string java_package_;
  string classname_;


  void generateembeddeddescriptor(io::printer* printer);

  google_disallow_evil_constructors(filegenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_java_file_h__
