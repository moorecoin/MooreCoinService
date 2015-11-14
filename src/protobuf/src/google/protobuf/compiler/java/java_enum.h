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

#ifndef google_protobuf_compiler_java_enum_h__
#define google_protobuf_compiler_java_enum_h__

#include <string>
#include <vector>
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

class enumgenerator {
 public:
  explicit enumgenerator(const enumdescriptor* descriptor);
  ~enumgenerator();

  void generate(io::printer* printer);

 private:
  const enumdescriptor* descriptor_;

  // the proto language allows multiple enum constants to have the same numeric
  // value.  java, however, does not allow multiple enum constants to be
  // considered equivalent.  we treat the first defined constant for any
  // given numeric value as "canonical" and the rest as aliases of that
  // canonical value.
  vector<const enumvaluedescriptor*> canonical_values_;

  struct alias {
    const enumvaluedescriptor* value;
    const enumvaluedescriptor* canonical_value;
  };
  vector<alias> aliases_;

  bool canuseenumvalues();

  google_disallow_evil_constructors(enumgenerator);
};

}  // namespace java
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_java_enum_h__
