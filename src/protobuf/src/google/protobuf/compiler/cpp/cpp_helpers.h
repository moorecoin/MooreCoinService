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

#ifndef google_protobuf_compiler_cpp_helpers_h__
#define google_protobuf_compiler_cpp_helpers_h__

#include <map>
#include <string>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {

namespace io {
class printer;
}

namespace compiler {
namespace cpp {

// commonly-used separator comments.  thick is a line of '=', thin is a line
// of '-'.
extern const char kthickseparator[];
extern const char kthinseparator[];

// returns the non-nested type name for the given type.  if "qualified" is
// true, prefix the type with the full namespace.  for example, if you had:
//   package foo.bar;
//   message baz { message qux {} }
// then the qualified classname for qux would be:
//   ::foo::bar::baz_qux
// while the non-qualified version would be:
//   baz_qux
string classname(const descriptor* descriptor, bool qualified);
string classname(const enumdescriptor* enum_descriptor, bool qualified);

string superclassname(const descriptor* descriptor);

// get the (unqualified) name that should be used for this field in c++ code.
// the name is coerced to lower-case to emulate proto1 behavior.  people
// should be using lowercase-with-underscores style for proto field names
// anyway, so normally this just returns field->name().
string fieldname(const fielddescriptor* field);

// get the unqualified name that should be used for a field's field
// number constant.
string fieldconstantname(const fielddescriptor *field);

// returns the scope where the field was defined (for extensions, this is
// different from the message type to which the field applies).
inline const descriptor* fieldscope(const fielddescriptor* field) {
  return field->is_extension() ?
    field->extension_scope() : field->containing_type();
}

// returns the fully-qualified type name field->message_type().  usually this
// is just classname(field->message_type(), true);
string fieldmessagetypename(const fielddescriptor* field);

// strips ".proto" or ".protodevel" from the end of a filename.
string stripproto(const string& filename);

// get the c++ type name for a primitive type (e.g. "double", "::google::protobuf::int32", etc.).
// note:  non-built-in type names will be qualified, meaning they will start
// with a ::.  if you are using the type as a template parameter, you will
// need to insure there is a space between the < and the ::, because the
// ridiculous c++ standard defines "<:" to be a synonym for "[".
const char* primitivetypename(fielddescriptor::cpptype type);

// get the declared type name in camelcase format, as is used e.g. for the
// methods of wireformat.  for example, type_int32 becomes "int32".
const char* declaredtypemethodname(fielddescriptor::type type);

// get code that evaluates to the field's default value.
string defaultvalue(const fielddescriptor* field);

// convert a file name into a valid identifier.
string filenameidentifier(const string& filename);

// return the name of the adddescriptors() function for a given file.
string globaladddescriptorsname(const string& filename);

// return the name of the assigndescriptors() function for a given file.
string globalassigndescriptorsname(const string& filename);

// return the name of the shutdownfile() function for a given file.
string globalshutdownfilename(const string& filename);

// escape c++ trigraphs by escaping question marks to \?
string escapetrigraphs(const string& to_escape);

// do message classes in this file keep track of unknown fields?
inline bool hasunknownfields(const filedescriptor* file) {
  return file->options().optimize_for() != fileoptions::lite_runtime;
}


// does this file have any enum type definitions?
bool hasenumdefinitions(const filedescriptor* file);

// does this file have generated parsing, serialization, and other
// standard methods for which reflection-based fallback implementations exist?
inline bool hasgeneratedmethods(const filedescriptor* file) {
  return file->options().optimize_for() != fileoptions::code_size;
}

// do message classes in this file have descriptor and reflection methods?
inline bool hasdescriptormethods(const filedescriptor* file) {
  return file->options().optimize_for() != fileoptions::lite_runtime;
}

// should we generate generic services for this file?
inline bool hasgenericservices(const filedescriptor* file) {
  return file->service_count() > 0 &&
         file->options().optimize_for() != fileoptions::lite_runtime &&
         file->options().cc_generic_services();
}

// should string fields in this file verify that their contents are utf-8?
inline bool hasutf8verification(const filedescriptor* file) {
  return file->options().optimize_for() != fileoptions::lite_runtime;
}

// should we generate a separate, super-optimized code path for serializing to
// flat arrays?  we don't do this in lite mode because we'd rather reduce code
// size.
inline bool hasfastarrayserialization(const filedescriptor* file) {
  return file->options().optimize_for() == fileoptions::speed;
}

// returns whether we have to generate code with static initializers.
bool staticinitializersforced(const filedescriptor* file);

// prints 'with_static_init' if static initializers have to be used for the
// provided file. otherwise emits both 'with_static_init' and
// 'without_static_init' using #ifdef.
void printhandlingoptionalstaticinitializers(
    const filedescriptor* file, io::printer* printer,
    const char* with_static_init, const char* without_static_init,
    const char* var1 = null, const string& val1 = "",
    const char* var2 = null, const string& val2 = "");

void printhandlingoptionalstaticinitializers(
    const map<string, string>& vars, const filedescriptor* file,
    io::printer* printer, const char* with_static_init,
    const char* without_static_init);


}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_cpp_helpers_h__
