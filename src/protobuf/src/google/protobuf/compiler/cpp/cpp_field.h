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

#ifndef google_protobuf_compiler_cpp_field_h__
#define google_protobuf_compiler_cpp_field_h__

#include <map>
#include <string>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
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

// helper function: set variables in the map that are the same for all
// field code generators.
// ['name', 'index', 'number', 'classname', 'declared_type', 'tag_size',
// 'deprecation'].
void setcommonfieldvariables(const fielddescriptor* descriptor,
                             map<string, string>* variables,
                             const options& options);

class fieldgenerator {
 public:
  fieldgenerator() {}
  virtual ~fieldgenerator();

  // generate lines of code declaring members fields of the message class
  // needed to represent this field.  these are placed inside the message
  // class.
  virtual void generateprivatemembers(io::printer* printer) const = 0;

  // generate prototypes for all of the accessor functions related to this
  // field.  these are placed inside the class definition.
  virtual void generateaccessordeclarations(io::printer* printer) const = 0;

  // generate inline definitions of accessor functions for this field.
  // these are placed inside the header after all class definitions.
  virtual void generateinlineaccessordefinitions(
    io::printer* printer) const = 0;

  // generate definitions of accessors that aren't inlined.  these are
  // placed somewhere in the .cc file.
  // most field types don't need this, so the default implementation is empty.
  virtual void generatenoninlineaccessordefinitions(
    io::printer* printer) const {}

  // generate lines of code (statements, not declarations) which clear the
  // field.  this is used to define the clear_$name$() method as well as
  // the clear() method for the whole message.
  virtual void generateclearingcode(io::printer* printer) const = 0;

  // generate lines of code (statements, not declarations) which merges the
  // contents of the field from the current message to the target message,
  // which is stored in the generated code variable "from".
  // this is used to fill in the mergefrom method for the whole message.
  // details of this usage can be found in message.cc under the
  // generatemergefrom method.
  virtual void generatemergingcode(io::printer* printer) const = 0;

  // generate lines of code (statements, not declarations) which swaps
  // this field and the corresponding field of another message, which
  // is stored in the generated code variable "other". this is used to
  // define the swap method. details of usage can be found in
  // message.cc under the generateswap method.
  virtual void generateswappingcode(io::printer* printer) const = 0;

  // generate initialization code for private members declared by
  // generateprivatemembers(). these go into the message class's sharedctor()
  // method, invoked by each of the generated constructors.
  virtual void generateconstructorcode(io::printer* printer) const = 0;

  // generate any code that needs to go in the class's shareddtor() method,
  // invoked by the destructor.
  // most field types don't need this, so the default implementation is empty.
  virtual void generatedestructorcode(io::printer* printer) const {}

  // generate code that allocates the fields's default instance.
  virtual void generatedefaultinstanceallocator(io::printer* printer) const {}

  // generate code that should be run when shutdownprotobuflibrary() is called,
  // to delete all dynamically-allocated objects.
  virtual void generateshutdowncode(io::printer* printer) const {}

  // generate lines to decode this field, which will be placed inside the
  // message's mergefromcodedstream() method.
  virtual void generatemergefromcodedstream(io::printer* printer) const = 0;

  // generate lines to decode this field from a packed value, which will be
  // placed inside the message's mergefromcodedstream() method.
  virtual void generatemergefromcodedstreamwithpacking(io::printer* printer)
      const;

  // generate lines to serialize this field, which are placed within the
  // message's serializewithcachedsizes() method.
  virtual void generateserializewithcachedsizes(io::printer* printer) const = 0;

  // generate lines to serialize this field directly to the array "target",
  // which are placed within the message's serializewithcachedsizestoarray()
  // method. this must also advance "target" past the written bytes.
  virtual void generateserializewithcachedsizestoarray(
      io::printer* printer) const = 0;

  // generate lines to compute the serialized size of this field, which
  // are placed in the message's bytesize() method.
  virtual void generatebytesize(io::printer* printer) const = 0;

 private:
  google_disallow_evil_constructors(fieldgenerator);
};

// convenience class which constructs fieldgenerators for a descriptor.
class fieldgeneratormap {
 public:
  explicit fieldgeneratormap(const descriptor* descriptor, const options& options);
  ~fieldgeneratormap();

  const fieldgenerator& get(const fielddescriptor* field) const;

 private:
  const descriptor* descriptor_;
  scoped_array<scoped_ptr<fieldgenerator> > field_generators_;

  static fieldgenerator* makegenerator(const fielddescriptor* field,
                                       const options& options);

  google_disallow_evil_constructors(fieldgeneratormap);
};


}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_field_h__
