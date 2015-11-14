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

#ifndef google_protobuf_compiler_cpp_enum_h__
#define google_protobuf_compiler_cpp_enum_h__

#include <string>
#include <google/protobuf/compiler/cpp/cpp_options.h>
#include <google/protobuf/descriptor.h>


namespace google {
namespace protobuf {
  namespace io {
    class printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace cpp {

class enumgenerator {
 public:
  // see generator.cc for the meaning of dllexport_decl.
  explicit enumgenerator(const enumdescriptor* descriptor,
                         const options& options);
  ~enumgenerator();

  // header stuff.

  // generate header code defining the enum.  this code should be placed
  // within the enum's package namespace, but not within any class, even for
  // nested enums.
  void generatedefinition(io::printer* printer);

  // generate specialization of getenumdescriptor<myenum>().
  // precondition: in ::google::protobuf namespace.
  void generategetenumdescriptorspecializations(io::printer* printer);

  // for enums nested within a message, generate code to import all the enum's
  // symbols (e.g. the enum type name, all its values, etc.) into the class's
  // namespace.  this should be placed inside the class definition in the
  // header.
  void generatesymbolimports(io::printer* printer);

  // source file stuff.

  // generate code that initializes the global variable storing the enum's
  // descriptor.
  void generatedescriptorinitializer(io::printer* printer, int index);

  // generate non-inline methods related to the enum, such as isvalidvalue().
  // goes in the .cc file.
  void generatemethods(io::printer* printer);

 private:
  const enumdescriptor* descriptor_;
  string classname_;
  options options_;

  google_disallow_evil_constructors(enumgenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_enum_h__
