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

#ifndef google_protobuf_compiler_cpp_service_h__
#define google_protobuf_compiler_cpp_service_h__

#include <map>
#include <string>
#include <google/protobuf/stubs/common.h>
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

class servicegenerator {
 public:
  // see generator.cc for the meaning of dllexport_decl.
  explicit servicegenerator(const servicedescriptor* descriptor,
                            const options& options);
  ~servicegenerator();

  // header stuff.

  // generate the class definitions for the service's interface and the
  // stub implementation.
  void generatedeclarations(io::printer* printer);

  // source file stuff.

  // generate code that initializes the global variable storing the service's
  // descriptor.
  void generatedescriptorinitializer(io::printer* printer, int index);

  // generate implementations of everything declared by generatedeclarations().
  void generateimplementation(io::printer* printer);

 private:
  enum requestorresponse { request, response };
  enum virtualornon { virtual, non_virtual };

  // header stuff.

  // generate the service abstract interface.
  void generateinterface(io::printer* printer);

  // generate the stub class definition.
  void generatestubdefinition(io::printer* printer);

  // prints signatures for all methods in the
  void generatemethodsignatures(virtualornon virtual_or_non,
                                io::printer* printer);

  // source file stuff.

  // generate the default implementations of the service methods, which
  // produce a "not implemented" error.
  void generatenotimplementedmethods(io::printer* printer);

  // generate the callmethod() method of the service.
  void generatecallmethod(io::printer* printer);

  // generate the get{request,response}prototype() methods.
  void generategetprototype(requestorresponse which, io::printer* printer);

  // generate the stub's implementations of the service methods.
  void generatestubmethods(io::printer* printer);

  const servicedescriptor* descriptor_;
  map<string, string> vars_;

  google_disallow_evil_constructors(servicegenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_service_h__
