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

#include <limits>
#include <map>
#include <vector>
#include <google/protobuf/stubs/hash.h>

#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

string dotstounderscores(const string& name) {
  return stringreplace(name, ".", "_", true);
}

string dotstocolons(const string& name) {
  return stringreplace(name, ".", "::", true);
}

const char* const kkeywordlist[] = {
  "and", "and_eq", "asm", "auto", "bitand", "bitor", "bool", "break", "case",
  "catch", "char", "class", "compl", "const", "const_cast", "continue",
  "default", "delete", "do", "double", "dynamic_cast", "else", "enum",
  "explicit", "extern", "false", "float", "for", "friend", "goto", "if",
  "inline", "int", "long", "mutable", "namespace", "new", "not", "not_eq",
  "operator", "or", "or_eq", "private", "protected", "public", "register",
  "reinterpret_cast", "return", "short", "signed", "sizeof", "static",
  "static_cast", "struct", "switch", "template", "this", "throw", "true", "try",
  "typedef", "typeid", "typename", "union", "unsigned", "using", "virtual",
  "void", "volatile", "wchar_t", "while", "xor", "xor_eq"
};

hash_set<string> makekeywordsmap() {
  hash_set<string> result;
  for (int i = 0; i < google_arraysize(kkeywordlist); i++) {
    result.insert(kkeywordlist[i]);
  }
  return result;
}

hash_set<string> kkeywords = makekeywordsmap();

string underscorestocamelcase(const string& input, bool cap_next_letter) {
  string result;
  // note:  i distrust ctype.h due to locales.
  for (int i = 0; i < input.size(); i++) {
    if ('a' <= input[i] && input[i] <= 'z') {
      if (cap_next_letter) {
        result += input[i] + ('a' - 'a');
      } else {
        result += input[i];
      }
      cap_next_letter = false;
    } else if ('a' <= input[i] && input[i] <= 'z') {
      // capital letters are left as-is.
      result += input[i];
      cap_next_letter = false;
    } else if ('0' <= input[i] && input[i] <= '9') {
      result += input[i];
      cap_next_letter = true;
    } else {
      cap_next_letter = true;
    }
  }
  return result;
}

// returns whether the provided descriptor has an extension. this includes its
// nested types.
bool hasextension(const descriptor* descriptor) {
  if (descriptor->extension_count() > 0) {
    return true;
  }
  for (int i = 0; i < descriptor->nested_type_count(); ++i) {
    if (hasextension(descriptor->nested_type(i))) {
      return true;
    }
  }
  return false;
}

}  // namespace

const char kthickseparator[] =
  "// ===================================================================\n";
const char kthinseparator[] =
  "// -------------------------------------------------------------------\n";

string classname(const descriptor* descriptor, bool qualified) {

  // find "outer", the descriptor of the top-level message in which
  // "descriptor" is embedded.
  const descriptor* outer = descriptor;
  while (outer->containing_type() != null) outer = outer->containing_type();

  const string& outer_name = outer->full_name();
  string inner_name = descriptor->full_name().substr(outer_name.size());

  if (qualified) {
    return "::" + dotstocolons(outer_name) + dotstounderscores(inner_name);
  } else {
    return outer->name() + dotstounderscores(inner_name);
  }
}

string classname(const enumdescriptor* enum_descriptor, bool qualified) {
  if (enum_descriptor->containing_type() == null) {
    if (qualified) {
      return "::" + dotstocolons(enum_descriptor->full_name());
    } else {
      return enum_descriptor->name();
    }
  } else {
    string result = classname(enum_descriptor->containing_type(), qualified);
    result += '_';
    result += enum_descriptor->name();
    return result;
  }
}


string superclassname(const descriptor* descriptor) {
  return hasdescriptormethods(descriptor->file()) ?
      "::google::protobuf::message" : "::google::protobuf::messagelite";
}

string fieldname(const fielddescriptor* field) {
  string result = field->name();
  lowerstring(&result);
  if (kkeywords.count(result) > 0) {
    result.append("_");
  }
  return result;
}

string fieldconstantname(const fielddescriptor *field) {
  string field_name = underscorestocamelcase(field->name(), true);
  string result = "k" + field_name + "fieldnumber";

  if (!field->is_extension() &&
      field->containing_type()->findfieldbycamelcasename(
        field->camelcase_name()) != field) {
    // this field's camelcase name is not unique.  as a hack, add the field
    // number to the constant name.  this makes the constant rather useless,
    // but what can we do?
    result += "_" + simpleitoa(field->number());
  }

  return result;
}

string fieldmessagetypename(const fielddescriptor* field) {
  // note:  the google-internal version of protocol buffers uses this function
  //   as a hook point for hacks to support legacy code.
  return classname(field->message_type(), true);
}

string stripproto(const string& filename) {
  if (hassuffixstring(filename, ".protodevel")) {
    return stripsuffixstring(filename, ".protodevel");
  } else {
    return stripsuffixstring(filename, ".proto");
  }
}

const char* primitivetypename(fielddescriptor::cpptype type) {
  switch (type) {
    case fielddescriptor::cpptype_int32  : return "::google::protobuf::int32";
    case fielddescriptor::cpptype_int64  : return "::google::protobuf::int64";
    case fielddescriptor::cpptype_uint32 : return "::google::protobuf::uint32";
    case fielddescriptor::cpptype_uint64 : return "::google::protobuf::uint64";
    case fielddescriptor::cpptype_double : return "double";
    case fielddescriptor::cpptype_float  : return "float";
    case fielddescriptor::cpptype_bool   : return "bool";
    case fielddescriptor::cpptype_enum   : return "int";
    case fielddescriptor::cpptype_string : return "::std::string";
    case fielddescriptor::cpptype_message: return null;

    // no default because we want the compiler to complain if any new
    // cpptypes are added.
  }

  google_log(fatal) << "can't get here.";
  return null;
}

const char* declaredtypemethodname(fielddescriptor::type type) {
  switch (type) {
    case fielddescriptor::type_int32   : return "int32";
    case fielddescriptor::type_int64   : return "int64";
    case fielddescriptor::type_uint32  : return "uint32";
    case fielddescriptor::type_uint64  : return "uint64";
    case fielddescriptor::type_sint32  : return "sint32";
    case fielddescriptor::type_sint64  : return "sint64";
    case fielddescriptor::type_fixed32 : return "fixed32";
    case fielddescriptor::type_fixed64 : return "fixed64";
    case fielddescriptor::type_sfixed32: return "sfixed32";
    case fielddescriptor::type_sfixed64: return "sfixed64";
    case fielddescriptor::type_float   : return "float";
    case fielddescriptor::type_double  : return "double";

    case fielddescriptor::type_bool    : return "bool";
    case fielddescriptor::type_enum    : return "enum";

    case fielddescriptor::type_string  : return "string";
    case fielddescriptor::type_bytes   : return "bytes";
    case fielddescriptor::type_group   : return "group";
    case fielddescriptor::type_message : return "message";

    // no default because we want the compiler to complain if any new
    // types are added.
  }
  google_log(fatal) << "can't get here.";
  return "";
}

string defaultvalue(const fielddescriptor* field) {
  switch (field->cpp_type()) {
    case fielddescriptor::cpptype_int32:
      // gcc rejects the decimal form of kint32min and kint64min.
      if (field->default_value_int32() == kint32min) {
        // make sure we are in a 2's complement system.
        google_compile_assert(kint32min == -0x80000000, kint32min_value_error);
        return "-0x80000000";
      }
      return simpleitoa(field->default_value_int32());
    case fielddescriptor::cpptype_uint32:
      return simpleitoa(field->default_value_uint32()) + "u";
    case fielddescriptor::cpptype_int64:
      // see the comments for cpptype_int32.
      if (field->default_value_int64() == kint64min) {
        // make sure we are in a 2's complement system.
        google_compile_assert(kint64min == google_longlong(-0x8000000000000000),
                       kint64min_value_error);
        return "google_longlong(-0x8000000000000000)";
      }
      return "google_longlong(" + simpleitoa(field->default_value_int64()) + ")";
    case fielddescriptor::cpptype_uint64:
      return "google_ulonglong(" + simpleitoa(field->default_value_uint64())+ ")";
    case fielddescriptor::cpptype_double: {
      double value = field->default_value_double();
      if (value == numeric_limits<double>::infinity()) {
        return "::google::protobuf::internal::infinity()";
      } else if (value == -numeric_limits<double>::infinity()) {
        return "-::google::protobuf::internal::infinity()";
      } else if (value != value) {
        return "::google::protobuf::internal::nan()";
      } else {
        return simpledtoa(value);
      }
    }
    case fielddescriptor::cpptype_float:
      {
        float value = field->default_value_float();
        if (value == numeric_limits<float>::infinity()) {
          return "static_cast<float>(::google::protobuf::internal::infinity())";
        } else if (value == -numeric_limits<float>::infinity()) {
          return "static_cast<float>(-::google::protobuf::internal::infinity())";
        } else if (value != value) {
          return "static_cast<float>(::google::protobuf::internal::nan())";
        } else {
          string float_value = simpleftoa(value);
          // if floating point value contains a period (.) or an exponent
          // (either e or e), then append suffix 'f' to make it a float
          // literal.
          if (float_value.find_first_of(".ee") != string::npos) {
            float_value.push_back('f');
          }
          return float_value;
        }
      }
    case fielddescriptor::cpptype_bool:
      return field->default_value_bool() ? "true" : "false";
    case fielddescriptor::cpptype_enum:
      // lazy:  generate a static_cast because we don't have a helper function
      //   that constructs the full name of an enum value.
      return strings::substitute(
          "static_cast< $0 >($1)",
          classname(field->enum_type(), true),
          field->default_value_enum()->number());
    case fielddescriptor::cpptype_string:
      return "\"" + escapetrigraphs(
        cescape(field->default_value_string())) +
        "\"";
    case fielddescriptor::cpptype_message:
      return fieldmessagetypename(field) + "::default_instance()";
  }
  // can't actually get here; make compiler happy.  (we could add a default
  // case above but then we wouldn't get the nice compiler warning when a
  // new type is added.)
  google_log(fatal) << "can't get here.";
  return "";
}

// convert a file name into a valid identifier.
string filenameidentifier(const string& filename) {
  string result;
  for (int i = 0; i < filename.size(); i++) {
    if (ascii_isalnum(filename[i])) {
      result.push_back(filename[i]);
    } else {
      // not alphanumeric.  to avoid any possibility of name conflicts we
      // use the hex code for the character.
      result.push_back('_');
      char buffer[kfasttobuffersize];
      result.append(fasthextobuffer(static_cast<uint8>(filename[i]), buffer));
    }
  }
  return result;
}

// return the name of the adddescriptors() function for a given file.
string globaladddescriptorsname(const string& filename) {
  return "protobuf_adddesc_" + filenameidentifier(filename);
}

// return the name of the assigndescriptors() function for a given file.
string globalassigndescriptorsname(const string& filename) {
  return "protobuf_assigndesc_" + filenameidentifier(filename);
}

// return the name of the shutdownfile() function for a given file.
string globalshutdownfilename(const string& filename) {
  return "protobuf_shutdownfile_" + filenameidentifier(filename);
}

// escape c++ trigraphs by escaping question marks to \?
string escapetrigraphs(const string& to_escape) {
  return stringreplace(to_escape, "?", "\\?", true);
}

bool staticinitializersforced(const filedescriptor* file) {
  if (hasdescriptormethods(file) || file->extension_count() > 0) {
    return true;
  }
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (hasextension(file->message_type(i))) {
      return true;
    }
  }
  return false;
}

void printhandlingoptionalstaticinitializers(
    const filedescriptor* file, io::printer* printer,
    const char* with_static_init, const char* without_static_init,
    const char* var1, const string& val1,
    const char* var2, const string& val2) {
  map<string, string> vars;
  if (var1) {
    vars[var1] = val1;
  }
  if (var2) {
    vars[var2] = val2;
  }
  printhandlingoptionalstaticinitializers(
      vars, file, printer, with_static_init, without_static_init);
}

void printhandlingoptionalstaticinitializers(
    const map<string, string>& vars, const filedescriptor* file,
    io::printer* printer, const char* with_static_init,
    const char* without_static_init) {
  if (staticinitializersforced(file)) {
    printer->print(vars, with_static_init);
  } else {
    printer->print(vars, (string(
      "#ifdef google_protobuf_no_static_initializer\n") +
      without_static_init +
      "#else\n" +
      with_static_init +
      "#endif\n").c_str());
  }
}


static bool hasenumdefinitions(const descriptor* message_type) {
  if (message_type->enum_type_count() > 0) return true;
  for (int i = 0; i < message_type->nested_type_count(); ++i) {
    if (hasenumdefinitions(message_type->nested_type(i))) return true;
  }
  return false;
}

bool hasenumdefinitions(const filedescriptor* file) {
  if (file->enum_type_count() > 0) return true;
  for (int i = 0; i < file->message_type_count(); ++i) {
    if (hasenumdefinitions(file->message_type(i))) return true;
  }
  return false;
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
