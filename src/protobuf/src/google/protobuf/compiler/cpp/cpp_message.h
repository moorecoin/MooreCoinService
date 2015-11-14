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

#ifndef google_protobuf_compiler_cpp_message_h__
#define google_protobuf_compiler_cpp_message_h__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_options.h>

namespace google {
namespace protobuf {
  namespace io {
    class printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace cpp {

class enumgenerator;           // enum.h
class extensiongenerator;      // extension.h

class messagegenerator {
 public:
  // see generator.cc for the meaning of dllexport_decl.
  explicit messagegenerator(const descriptor* descriptor,
                            const options& options);
  ~messagegenerator();

  // header stuff.

  // generate foward declarations for this class and all its nested types.
  void generateforwarddeclaration(io::printer* printer);

  // generate definitions of all nested enums (must come before class
  // definitions because those classes use the enums definitions).
  void generateenumdefinitions(io::printer* printer);

  // generate specializations of getenumdescriptor<myenum>().
  // precondition: in ::google::protobuf namespace.
  void generategetenumdescriptorspecializations(io::printer* printer);

  // generate definitions for this class and all its nested types.
  void generateclassdefinition(io::printer* printer);

  // generate definitions of inline methods (placed at the end of the header
  // file).
  void generateinlinemethods(io::printer* printer);

  // source file stuff.

  // generate code which declares all the global descriptor pointers which
  // will be initialized by the methods below.
  void generatedescriptordeclarations(io::printer* printer);

  // generate code that initializes the global variable storing the message's
  // descriptor.
  void generatedescriptorinitializer(io::printer* printer, int index);

  // generate code that calls messagefactory::internalregistergeneratedmessage()
  // for all types.
  void generatetyperegistrations(io::printer* printer);

  // generates code that allocates the message's default instance.
  void generatedefaultinstanceallocator(io::printer* printer);

  // generates code that initializes the message's default instance.  this
  // is separate from allocating because all default instances must be
  // allocated before any can be initialized.
  void generatedefaultinstanceinitializer(io::printer* printer);

  // generates code that should be run when shutdownprotobuflibrary() is called,
  // to delete all dynamically-allocated objects.
  void generateshutdowncode(io::printer* printer);

  // generate all non-inline methods for this class.
  void generateclassmethods(io::printer* printer);

 private:
  // generate declarations and definitions of accessors for fields.
  void generatefieldaccessordeclarations(io::printer* printer);
  void generatefieldaccessordefinitions(io::printer* printer);

  // generate the field offsets array.
  void generateoffsets(io::printer* printer);

  // generate constructors and destructor.
  void generatestructors(io::printer* printer);

  // the compiler typically generates multiple copies of each constructor and
  // destructor: http://gcc.gnu.org/bugs.html#nonbugs_cxx
  // placing common code in a separate method reduces the generated code size.
  //
  // generate the shared constructor code.
  void generatesharedconstructorcode(io::printer* printer);
  // generate the shared destructor code.
  void generateshareddestructorcode(io::printer* printer);

  // generate standard message methods.
  void generateclear(io::printer* printer);
  void generatemergefromcodedstream(io::printer* printer);
  void generateserializewithcachedsizes(io::printer* printer);
  void generateserializewithcachedsizestoarray(io::printer* printer);
  void generateserializewithcachedsizesbody(io::printer* printer,
                                            bool to_array);
  void generatebytesize(io::printer* printer);
  void generatemergefrom(io::printer* printer);
  void generatecopyfrom(io::printer* printer);
  void generateswap(io::printer* printer);
  void generateisinitialized(io::printer* printer);

  // helpers for generateserializewithcachedsizes().
  void generateserializeonefield(io::printer* printer,
                                 const fielddescriptor* field,
                                 bool unbounded);
  void generateserializeoneextensionrange(
      io::printer* printer, const descriptor::extensionrange* range,
      bool unbounded);


  const descriptor* descriptor_;
  string classname_;
  options options_;
  fieldgeneratormap field_generators_;
  scoped_array<scoped_ptr<messagegenerator> > nested_generators_;
  scoped_array<scoped_ptr<enumgenerator> > enum_generators_;
  scoped_array<scoped_ptr<extensiongenerator> > extension_generators_;

  google_disallow_evil_constructors(messagegenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_message_h__
