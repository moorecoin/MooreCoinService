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

#ifndef google_protobuf_compiler_cpp_file_h__
#define google_protobuf_compiler_cpp_file_h__

#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_options.h>

namespace google {
namespace protobuf {
  class filedescriptor;        // descriptor.h
  namespace io {
    class printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace cpp {

class enumgenerator;           // enum.h
class messagegenerator;        // message.h
class servicegenerator;        // service.h
class extensiongenerator;      // extension.h

class filegenerator {
 public:
  // see generator.cc for the meaning of dllexport_decl.
  explicit filegenerator(const filedescriptor* file,
                         const options& options);
  ~filegenerator();

  void generateheader(io::printer* printer);
  void generatesource(io::printer* printer);

 private:
  // generate the builddescriptors() procedure, which builds all descriptors
  // for types defined in the file.
  void generatebuilddescriptors(io::printer* printer);

  void generatenamespaceopeners(io::printer* printer);
  void generatenamespaceclosers(io::printer* printer);

  const filedescriptor* file_;

  scoped_array<scoped_ptr<messagegenerator> > message_generators_;
  scoped_array<scoped_ptr<enumgenerator> > enum_generators_;
  scoped_array<scoped_ptr<servicegenerator> > service_generators_;
  scoped_array<scoped_ptr<extensiongenerator> > extension_generators_;

  // e.g. if the package is foo.bar, package_parts_ is {"foo", "bar"}.
  vector<string> package_parts_;

  const options options_;

  google_disallow_evil_constructors(filegenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_file_h__
