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

#ifndef google_protobuf_compiler_cpp_message_field_h__
#define google_protobuf_compiler_cpp_message_field_h__

#include <map>
#include <string>
#include <google/protobuf/compiler/cpp/cpp_field.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

class messagefieldgenerator : public fieldgenerator {
 public:
  explicit messagefieldgenerator(const fielddescriptor* descriptor,
                                 const options& options);
  ~messagefieldgenerator();

  // implements fieldgenerator ---------------------------------------
  void generateprivatemembers(io::printer* printer) const;
  void generateaccessordeclarations(io::printer* printer) const;
  void generateinlineaccessordefinitions(io::printer* printer) const;
  void generateclearingcode(io::printer* printer) const;
  void generatemergingcode(io::printer* printer) const;
  void generateswappingcode(io::printer* printer) const;
  void generateconstructorcode(io::printer* printer) const;
  void generatemergefromcodedstream(io::printer* printer) const;
  void generateserializewithcachedsizes(io::printer* printer) const;
  void generateserializewithcachedsizestoarray(io::printer* printer) const;
  void generatebytesize(io::printer* printer) const;

 private:
  const fielddescriptor* descriptor_;
  map<string, string> variables_;

  google_disallow_evil_constructors(messagefieldgenerator);
};

class repeatedmessagefieldgenerator : public fieldgenerator {
 public:
  explicit repeatedmessagefieldgenerator(const fielddescriptor* descriptor,
                                         const options& options);
  ~repeatedmessagefieldgenerator();

  // implements fieldgenerator ---------------------------------------
  void generateprivatemembers(io::printer* printer) const;
  void generateaccessordeclarations(io::printer* printer) const;
  void generateinlineaccessordefinitions(io::printer* printer) const;
  void generateclearingcode(io::printer* printer) const;
  void generatemergingcode(io::printer* printer) const;
  void generateswappingcode(io::printer* printer) const;
  void generateconstructorcode(io::printer* printer) const;
  void generatemergefromcodedstream(io::printer* printer) const;
  void generateserializewithcachedsizes(io::printer* printer) const;
  void generateserializewithcachedsizestoarray(io::printer* printer) const;
  void generatebytesize(io::printer* printer) const;

 private:
  const fielddescriptor* descriptor_;
  map<string, string> variables_;

  google_disallow_evil_constructors(repeatedmessagefieldgenerator);
};

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_message_field_h__
