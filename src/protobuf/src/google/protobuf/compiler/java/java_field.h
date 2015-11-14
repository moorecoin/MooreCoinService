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

#ifndef google_protobuf_compiler_java_field_h__
#define google_protobuf_compiler_java_field_h__

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
  namespace io {
    class printer;             // printer.h
  }
}

namespace protobuf {
namespace compiler {
namespace java {

class fieldgenerator {
 public:
  fieldgenerator() {}
  virtual ~fieldgenerator();

  virtual int getnumbitsformessage() const = 0;
  virtual int getnumbitsforbuilder() const = 0;
  virtual void generateinterfacemembers(io::printer* printer) const = 0;
  virtual void generatemembers(io::printer* printer) const = 0;
  virtual void generatebuildermembers(io::printer* printer) const = 0;
  virtual void generateinitializationcode(io::printer* printer) const = 0;
  virtual void generatebuilderclearcode(io::printer* printer) const = 0;
  virtual void generatemergingcode(io::printer* printer) const = 0;
  virtual void generatebuildingcode(io::printer* printer) const = 0;
  virtual void generateparsingcode(io::printer* printer) const = 0;
  virtual void generateparsingcodefrompacked(io::printer* printer) const;
  virtual void generateparsingdonecode(io::printer* printer) const = 0;
  virtual void generateserializationcode(io::printer* printer) const = 0;
  virtual void generateserializedsizecode(io::printer* printer) const = 0;
  virtual void generatefieldbuilderinitializationcode(io::printer* printer)
      const = 0;

  virtual void generateequalscode(io::printer* printer) const = 0;
  virtual void generatehashcode(io::printer* printer) const = 0;

  virtual string getboxedtype() const = 0;

 private:
  google_disallow_evil_constructors(fieldgenerator);
};

// convenience class which constructs fieldgenerators for a descriptor.
class fieldgeneratormap {
 public:
  explicit fieldgeneratormap(const descriptor* descriptor);
  ~fieldgeneratormap();

  const fieldgenerator& get(const fielddescriptor* field) const;
  const fieldgenerator& get_extension(int index) const;

 private:
  const descriptor* descriptor_;
  scoped_array<scoped_ptr<fieldgenerator> > field_generators_;
  scoped_array<scoped_ptr<fieldgenerator> > extension_generators_;

  static fieldgenerator* makegenerator(const fielddescriptor* field,
      int messagebitindex, int builderbitindex);

  google_disallow_evil_constructors(fieldgeneratormap);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_java_field_h__
