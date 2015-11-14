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
#include <vector>

#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

const char kthickseparator[] =
  "// ===================================================================\n";
const char kthinseparator[] =
  "// -------------------------------------------------------------------\n";

namespace {

const char* kdefaultpackage = "";

const string& fieldname(const fielddescriptor* field) {
  // groups are hacky:  the name of the field is just the lower-cased name
  // of the group type.  in java, though, we would like to retain the original
  // capitalization of the type name.
  if (gettype(field) == fielddescriptor::type_group) {
    return field->message_type()->name();
  } else {
    return field->name();
  }
}

string underscorestocamelcaseimpl(const string& input, bool cap_next_letter) {
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
      if (i == 0 && !cap_next_letter) {
        // force first letter to lower-case unless explicitly told to
        // capitalize it.
        result += input[i] + ('a' - 'a');
      } else {
        // capital letters after the first are left as-is.
        result += input[i];
      }
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

}  // namespace

string underscorestocamelcase(const fielddescriptor* field) {
  return underscorestocamelcaseimpl(fieldname(field), false);
}

string underscorestocapitalizedcamelcase(const fielddescriptor* field) {
  return underscorestocamelcaseimpl(fieldname(field), true);
}

string underscorestocamelcase(const methoddescriptor* method) {
  return underscorestocamelcaseimpl(method->name(), false);
}

string stripproto(const string& filename) {
  if (hassuffixstring(filename, ".protodevel")) {
    return stripsuffixstring(filename, ".protodevel");
  } else {
    return stripsuffixstring(filename, ".proto");
  }
}

string fileclassname(const filedescriptor* file) {
  if (file->options().has_java_outer_classname()) {
    return file->options().java_outer_classname();
  } else {
    string basename;
    string::size_type last_slash = file->name().find_last_of('/');
    if (last_slash == string::npos) {
      basename = file->name();
    } else {
      basename = file->name().substr(last_slash + 1);
    }
    return underscorestocamelcaseimpl(stripproto(basename), true);
  }
}

string filejavapackage(const filedescriptor* file) {
  string result;

  if (file->options().has_java_package()) {
    result = file->options().java_package();
  } else {
    result = kdefaultpackage;
    if (!file->package().empty()) {
      if (!result.empty()) result += '.';
      result += file->package();
    }
  }


  return result;
}

string javapackagetodir(string package_name) {
  string package_dir =
    stringreplace(package_name, ".", "/", true);
  if (!package_dir.empty()) package_dir += "/";
  return package_dir;
}

string tojavaname(const string& full_name, const filedescriptor* file) {
  string result;
  if (file->options().java_multiple_files()) {
    result = filejavapackage(file);
  } else {
    result = classname(file);
  }
  if (!result.empty()) {
    result += '.';
  }
  if (file->package().empty()) {
    result += full_name;
  } else {
    // strip the proto package from full_name since we've replaced it with
    // the java package.
    result += full_name.substr(file->package().size() + 1);
  }
  return result;
}

string classname(const descriptor* descriptor) {
  return tojavaname(descriptor->full_name(), descriptor->file());
}

string classname(const enumdescriptor* descriptor) {
  return tojavaname(descriptor->full_name(), descriptor->file());
}

string classname(const servicedescriptor* descriptor) {
  return tojavaname(descriptor->full_name(), descriptor->file());
}

string classname(const filedescriptor* descriptor) {
  string result = filejavapackage(descriptor);
  if (!result.empty()) result += '.';
  result += fileclassname(descriptor);
  return result;
}

string fieldconstantname(const fielddescriptor *field) {
  string name = field->name() + "_field_number";
  upperstring(&name);
  return name;
}

fielddescriptor::type gettype(const fielddescriptor* field) {
  return field->type();
}

javatype getjavatype(const fielddescriptor* field) {
  switch (gettype(field)) {
    case fielddescriptor::type_int32:
    case fielddescriptor::type_uint32:
    case fielddescriptor::type_sint32:
    case fielddescriptor::type_fixed32:
    case fielddescriptor::type_sfixed32:
      return javatype_int;

    case fielddescriptor::type_int64:
    case fielddescriptor::type_uint64:
    case fielddescriptor::type_sint64:
    case fielddescriptor::type_fixed64:
    case fielddescriptor::type_sfixed64:
      return javatype_long;

    case fielddescriptor::type_float:
      return javatype_float;

    case fielddescriptor::type_double:
      return javatype_double;

    case fielddescriptor::type_bool:
      return javatype_boolean;

    case fielddescriptor::type_string:
      return javatype_string;

    case fielddescriptor::type_bytes:
      return javatype_bytes;

    case fielddescriptor::type_enum:
      return javatype_enum;

    case fielddescriptor::type_group:
    case fielddescriptor::type_message:
      return javatype_message;

    // no default because we want the compiler to complain if any new
    // types are added.
  }

  google_log(fatal) << "can't get here.";
  return javatype_int;
}

const char* boxedprimitivetypename(javatype type) {
  switch (type) {
    case javatype_int    : return "java.lang.integer";
    case javatype_long   : return "java.lang.long";
    case javatype_float  : return "java.lang.float";
    case javatype_double : return "java.lang.double";
    case javatype_boolean: return "java.lang.boolean";
    case javatype_string : return "java.lang.string";
    case javatype_bytes  : return "com.google.protobuf.bytestring";
    case javatype_enum   : return null;
    case javatype_message: return null;

    // no default because we want the compiler to complain if any new
    // javatypes are added.
  }

  google_log(fatal) << "can't get here.";
  return null;
}

bool allascii(const string& text) {
  for (int i = 0; i < text.size(); i++) {
    if ((text[i] & 0x80) != 0) {
      return false;
    }
  }
  return true;
}

string defaultvalue(const fielddescriptor* field) {
  // switch on cpptype since we need to know which default_value_* method
  // of fielddescriptor to call.
  switch (field->cpp_type()) {
    case fielddescriptor::cpptype_int32:
      return simpleitoa(field->default_value_int32());
    case fielddescriptor::cpptype_uint32:
      // need to print as a signed int since java has no unsigned.
      return simpleitoa(static_cast<int32>(field->default_value_uint32()));
    case fielddescriptor::cpptype_int64:
      return simpleitoa(field->default_value_int64()) + "l";
    case fielddescriptor::cpptype_uint64:
      return simpleitoa(static_cast<int64>(field->default_value_uint64())) +
             "l";
    case fielddescriptor::cpptype_double: {
      double value = field->default_value_double();
      if (value == numeric_limits<double>::infinity()) {
        return "double.positive_infinity";
      } else if (value == -numeric_limits<double>::infinity()) {
        return "double.negative_infinity";
      } else if (value != value) {
        return "double.nan";
      } else {
        return simpledtoa(value) + "d";
      }
    }
    case fielddescriptor::cpptype_float: {
      float value = field->default_value_float();
      if (value == numeric_limits<float>::infinity()) {
        return "float.positive_infinity";
      } else if (value == -numeric_limits<float>::infinity()) {
        return "float.negative_infinity";
      } else if (value != value) {
        return "float.nan";
      } else {
        return simpleftoa(value) + "f";
      }
    }
    case fielddescriptor::cpptype_bool:
      return field->default_value_bool() ? "true" : "false";
    case fielddescriptor::cpptype_string:
      if (gettype(field) == fielddescriptor::type_bytes) {
        if (field->has_default_value()) {
          // see comments in internal.java for gory details.
          return strings::substitute(
            "com.google.protobuf.internal.bytesdefaultvalue(\"$0\")",
            cescape(field->default_value_string()));
        } else {
          return "com.google.protobuf.bytestring.empty";
        }
      } else {
        if (allascii(field->default_value_string())) {
          // all chars are ascii.  in this case cescape() works fine.
          return "\"" + cescape(field->default_value_string()) + "\"";
        } else {
          // see comments in internal.java for gory details.
          return strings::substitute(
              "com.google.protobuf.internal.stringdefaultvalue(\"$0\")",
              cescape(field->default_value_string()));
        }
      }

    case fielddescriptor::cpptype_enum:
      return classname(field->enum_type()) + "." +
          field->default_value_enum()->name();

    case fielddescriptor::cpptype_message:
      return classname(field->message_type()) + ".getdefaultinstance()";

    // no default because we want the compiler to complain if any new
    // types are added.
  }

  google_log(fatal) << "can't get here.";
  return "";
}

bool isdefaultvaluejavadefault(const fielddescriptor* field) {
  // switch on cpptype since we need to know which default_value_* method
  // of fielddescriptor to call.
  switch (field->cpp_type()) {
    case fielddescriptor::cpptype_int32:
      return field->default_value_int32() == 0;
    case fielddescriptor::cpptype_uint32:
      return field->default_value_uint32() == 0;
    case fielddescriptor::cpptype_int64:
      return field->default_value_int64() == 0l;
    case fielddescriptor::cpptype_uint64:
      return field->default_value_uint64() == 0l;
    case fielddescriptor::cpptype_double:
      return field->default_value_double() == 0.0;
    case fielddescriptor::cpptype_float:
      return field->default_value_float() == 0.0;
    case fielddescriptor::cpptype_bool:
      return field->default_value_bool() == false;

    case fielddescriptor::cpptype_string:
    case fielddescriptor::cpptype_enum:
    case fielddescriptor::cpptype_message:
      return false;

    // no default because we want the compiler to complain if any new
    // types are added.
  }

  google_log(fatal) << "can't get here.";
  return false;
}

const char* bit_masks[] = {
  "0x00000001",
  "0x00000002",
  "0x00000004",
  "0x00000008",
  "0x00000010",
  "0x00000020",
  "0x00000040",
  "0x00000080",

  "0x00000100",
  "0x00000200",
  "0x00000400",
  "0x00000800",
  "0x00001000",
  "0x00002000",
  "0x00004000",
  "0x00008000",

  "0x00010000",
  "0x00020000",
  "0x00040000",
  "0x00080000",
  "0x00100000",
  "0x00200000",
  "0x00400000",
  "0x00800000",

  "0x01000000",
  "0x02000000",
  "0x04000000",
  "0x08000000",
  "0x10000000",
  "0x20000000",
  "0x40000000",
  "0x80000000",
};

string getbitfieldname(int index) {
  string varname = "bitfield";
  varname += simpleitoa(index);
  varname += "_";
  return varname;
}

string getbitfieldnameforbit(int bitindex) {
  return getbitfieldname(bitindex / 32);
}

namespace {

string generategetbitinternal(const string& prefix, int bitindex) {
  string varname = prefix + getbitfieldnameforbit(bitindex);
  int bitinvarindex = bitindex % 32;

  string mask = bit_masks[bitinvarindex];
  string result = "((" + varname + " & " + mask + ") == " + mask + ")";
  return result;
}

string generatesetbitinternal(const string& prefix, int bitindex) {
  string varname = prefix + getbitfieldnameforbit(bitindex);
  int bitinvarindex = bitindex % 32;

  string mask = bit_masks[bitinvarindex];
  string result = varname + " |= " + mask;
  return result;
}

}  // namespace

string generategetbit(int bitindex) {
  return generategetbitinternal("", bitindex);
}

string generatesetbit(int bitindex) {
  return generatesetbitinternal("", bitindex);
}

string generateclearbit(int bitindex) {
  string varname = getbitfieldnameforbit(bitindex);
  int bitinvarindex = bitindex % 32;

  string mask = bit_masks[bitinvarindex];
  string result = varname + " = (" + varname + " & ~" + mask + ")";
  return result;
}

string generategetbitfromlocal(int bitindex) {
  return generategetbitinternal("from_", bitindex);
}

string generatesetbittolocal(int bitindex) {
  return generatesetbitinternal("to_", bitindex);
}

string generategetbitmutablelocal(int bitindex) {
  return generategetbitinternal("mutable_", bitindex);
}

string generatesetbitmutablelocal(int bitindex) {
  return generatesetbitinternal("mutable_", bitindex);
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
