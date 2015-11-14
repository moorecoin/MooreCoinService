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

#ifndef google_protobuf_compiler_java_helpers_h__
#define google_protobuf_compiler_java_helpers_h__

#include <string>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

// commonly-used separator comments.  thick is a line of '=', thin is a line
// of '-'.
extern const char kthickseparator[];
extern const char kthinseparator[];

// converts the field's name to camel-case, e.g. "foo_bar_baz" becomes
// "foobarbaz" or "foobarbaz", respectively.
string underscorestocamelcase(const fielddescriptor* field);
string underscorestocapitalizedcamelcase(const fielddescriptor* field);

// similar, but for method names.  (typically, this merely has the effect
// of lower-casing the first letter of the name.)
string underscorestocamelcase(const methoddescriptor* method);

// strips ".proto" or ".protodevel" from the end of a filename.
string stripproto(const string& filename);

// gets the unqualified class name for the file.  each .proto file becomes a
// single java class, with all its contents nested in that class.
string fileclassname(const filedescriptor* file);

// returns the file's java package name.
string filejavapackage(const filedescriptor* file);

// returns output directory for the given package name.
string javapackagetodir(string package_name);

// converts the given fully-qualified name in the proto namespace to its
// fully-qualified name in the java namespace, given that it is in the given
// file.
string tojavaname(const string& full_name, const filedescriptor* file);

// these return the fully-qualified class name corresponding to the given
// descriptor.
string classname(const descriptor* descriptor);
string classname(const enumdescriptor* descriptor);
string classname(const servicedescriptor* descriptor);
string classname(const filedescriptor* descriptor);

inline string extensionidentifiername(const fielddescriptor* descriptor) {
  return tojavaname(descriptor->full_name(), descriptor->file());
}

// get the unqualified name that should be used for a field's field
// number constant.
string fieldconstantname(const fielddescriptor *field);

// returns the type of the fielddescriptor.
// this does nothing interesting for the open source release, but is used for
// hacks that improve compatability with version 1 protocol buffers at google.
fielddescriptor::type gettype(const fielddescriptor* field);

enum javatype {
  javatype_int,
  javatype_long,
  javatype_float,
  javatype_double,
  javatype_boolean,
  javatype_string,
  javatype_bytes,
  javatype_enum,
  javatype_message
};

javatype getjavatype(const fielddescriptor* field);

// get the fully-qualified class name for a boxed primitive type, e.g.
// "java.lang.integer" for javatype_int.  returns null for enum and message
// types.
const char* boxedprimitivetypename(javatype type);

string defaultvalue(const fielddescriptor* field);
bool isdefaultvaluejavadefault(const fielddescriptor* field);

// does this message class keep track of unknown fields?
inline bool hasunknownfields(const descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           fileoptions::lite_runtime;
}

// does this message class have generated parsing, serialization, and other
// standard methods for which reflection-based fallback implementations exist?
inline bool hasgeneratedmethods(const descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           fileoptions::code_size;
}

// does this message have specialized equals() and hashcode() methods?
inline bool hasequalsandhashcode(const descriptor* descriptor) {
  return descriptor->file()->options().java_generate_equals_and_hash();
}

// does this message class have descriptor and reflection methods?
inline bool hasdescriptormethods(const descriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           fileoptions::lite_runtime;
}
inline bool hasdescriptormethods(const enumdescriptor* descriptor) {
  return descriptor->file()->options().optimize_for() !=
           fileoptions::lite_runtime;
}
inline bool hasdescriptormethods(const filedescriptor* descriptor) {
  return descriptor->options().optimize_for() !=
           fileoptions::lite_runtime;
}

inline bool hasnestedbuilders(const descriptor* descriptor) {
  // the proto-lite version doesn't support nested builders.
  return descriptor->file()->options().optimize_for() !=
           fileoptions::lite_runtime;
}

// should we generate generic services for this file?
inline bool hasgenericservices(const filedescriptor *file) {
  return file->service_count() > 0 &&
         file->options().optimize_for() != fileoptions::lite_runtime &&
         file->options().java_generic_services();
}


// methods for shared bitfields.

// gets the name of the shared bitfield for the given index.
string getbitfieldname(int index);

// gets the name of the shared bitfield for the given bit index.
// effectively, getbitfieldname(bitindex / 32)
string getbitfieldnameforbit(int bitindex);

// generates the java code for the expression that returns the boolean value
// of the bit of the shared bitfields for the given bit index.
// example: "((bitfield1_ & 0x04) == 0x04)"
string generategetbit(int bitindex);

// generates the java code for the expression that sets the bit of the shared
// bitfields for the given bit index.
// example: "bitfield1_ = (bitfield1_ | 0x04)"
string generatesetbit(int bitindex);

// generates the java code for the expression that clears the bit of the shared
// bitfields for the given bit index.
// example: "bitfield1_ = (bitfield1_ & ~0x04)"
string generateclearbit(int bitindex);

// does the same as generategetbit but operates on the bit field on a local
// variable. this is used by the builder to copy the value in the builder to
// the message.
// example: "((from_bitfield1_ & 0x04) == 0x04)"
string generategetbitfromlocal(int bitindex);

// does the same as generatesetbit but operates on the bit field on a local
// variable. this is used by the builder to copy the value in the builder to
// the message.
// example: "to_bitfield1_ = (to_bitfield1_ | 0x04)"
string generatesetbittolocal(int bitindex);

// does the same as generategetbit but operates on the bit field on a local
// variable. this is used by the parsing constructor to record if a repeated
// field is mutable.
// example: "((mutable_bitfield1_ & 0x04) == 0x04)"
string generategetbitmutablelocal(int bitindex);

// does the same as generatesetbit but operates on the bit field on a local
// variable. this is used by the parsing constructor to record if a repeated
// field is mutable.
// example: "mutable_bitfield1_ = (mutable_bitfield1_ | 0x04)"
string generatesetbitmutablelocal(int bitindex);

}  // namespace java
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_java_helpers_h__
