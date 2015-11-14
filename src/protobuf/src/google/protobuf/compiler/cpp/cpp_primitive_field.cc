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

#include <google/protobuf/compiler/cpp/cpp_primitive_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

using internal::wireformatlite;

namespace {

// for encodings with fixed sizes, returns that size in bytes.  otherwise
// returns -1.
int fixedsize(fielddescriptor::type type) {
  switch (type) {
    case fielddescriptor::type_int32   : return -1;
    case fielddescriptor::type_int64   : return -1;
    case fielddescriptor::type_uint32  : return -1;
    case fielddescriptor::type_uint64  : return -1;
    case fielddescriptor::type_sint32  : return -1;
    case fielddescriptor::type_sint64  : return -1;
    case fielddescriptor::type_fixed32 : return wireformatlite::kfixed32size;
    case fielddescriptor::type_fixed64 : return wireformatlite::kfixed64size;
    case fielddescriptor::type_sfixed32: return wireformatlite::ksfixed32size;
    case fielddescriptor::type_sfixed64: return wireformatlite::ksfixed64size;
    case fielddescriptor::type_float   : return wireformatlite::kfloatsize;
    case fielddescriptor::type_double  : return wireformatlite::kdoublesize;

    case fielddescriptor::type_bool    : return wireformatlite::kboolsize;
    case fielddescriptor::type_enum    : return -1;

    case fielddescriptor::type_string  : return -1;
    case fielddescriptor::type_bytes   : return -1;
    case fielddescriptor::type_group   : return -1;
    case fielddescriptor::type_message : return -1;

    // no default because we want the compiler to complain if any new
    // types are added.
  }
  google_log(fatal) << "can't get here.";
  return -1;
}

void setprimitivevariables(const fielddescriptor* descriptor,
                           map<string, string>* variables,
                           const options& options) {
  setcommonfieldvariables(descriptor, variables, options);
  (*variables)["type"] = primitivetypename(descriptor->cpp_type());
  (*variables)["default"] = defaultvalue(descriptor);
  (*variables)["tag"] = simpleitoa(internal::wireformat::maketag(descriptor));
  int fixed_size = fixedsize(descriptor->type());
  if (fixed_size != -1) {
    (*variables)["fixed_size"] = simpleitoa(fixed_size);
  }
  (*variables)["wire_format_field_type"] =
      "::google::protobuf::internal::wireformatlite::" + fielddescriptorproto_type_name(
          static_cast<fielddescriptorproto_type>(descriptor->type()));
}

}  // namespace

// ===================================================================

primitivefieldgenerator::
primitivefieldgenerator(const fielddescriptor* descriptor,
                        const options& options)
  : descriptor_(descriptor) {
  setprimitivevariables(descriptor, &variables_, options);
}

primitivefieldgenerator::~primitivefieldgenerator() {}

void primitivefieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_, "$type$ $name$_;\n");
}

void primitivefieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $name$() const$deprecation$;\n"
    "inline void set_$name$($type$ value)$deprecation$;\n");
}

void primitivefieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline void $classname$::set_$name$($type$ value) {\n"
    "  set_has_$name$();\n"
    "  $name$_ = value;\n"
    "}\n");
}

void primitivefieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void primitivefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "set_$name$(from.$name$());\n");
}

void primitivefieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void primitivefieldgenerator::
generateconstructorcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void primitivefieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  printer->print(variables_,
    "do_((::google::protobuf::internal::wireformatlite::readprimitive<\n"
    "         $type$, $wire_format_field_type$>(\n"
    "       input, &$name$_)));\n"
    "set_has_$name$();\n");
}

void primitivefieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::internal::wireformatlite::write$declared_type$("
      "$number$, this->$name$(), output);\n");
}

void primitivefieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  printer->print(variables_,
    "target = ::google::protobuf::internal::wireformatlite::write$declared_type$toarray("
      "$number$, this->$name$(), target);\n");
}

void primitivefieldgenerator::
generatebytesize(io::printer* printer) const {
  int fixed_size = fixedsize(descriptor_->type());
  if (fixed_size == -1) {
    printer->print(variables_,
      "total_size += $tag_size$ +\n"
      "  ::google::protobuf::internal::wireformatlite::$declared_type$size(\n"
      "    this->$name$());\n");
  } else {
    printer->print(variables_,
      "total_size += $tag_size$ + $fixed_size$;\n");
  }
}

// ===================================================================

repeatedprimitivefieldgenerator::
repeatedprimitivefieldgenerator(const fielddescriptor* descriptor,
                                const options& options)
  : descriptor_(descriptor) {
  setprimitivevariables(descriptor, &variables_, options);

  if (descriptor->options().packed()) {
    variables_["packed_reader"] = "readpackedprimitive";
    variables_["repeated_reader"] = "readrepeatedprimitivenoinline";
  } else {
    variables_["packed_reader"] = "readpackedprimitivenoinline";
    variables_["repeated_reader"] = "readrepeatedprimitive";
  }
}

repeatedprimitivefieldgenerator::~repeatedprimitivefieldgenerator() {}

void repeatedprimitivefieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::repeatedfield< $type$ > $name$_;\n");
  if (descriptor_->options().packed() && hasgeneratedmethods(descriptor_->file())) {
    printer->print(variables_,
      "mutable int _$name$_cached_byte_size_;\n");
  }
}

void repeatedprimitivefieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $name$(int index) const$deprecation$;\n"
    "inline void set_$name$(int index, $type$ value)$deprecation$;\n"
    "inline void add_$name$($type$ value)$deprecation$;\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedfield< $type$ >&\n"
    "    $name$() const$deprecation$;\n"
    "inline ::google::protobuf::repeatedfield< $type$ >*\n"
    "    mutable_$name$()$deprecation$;\n");
}

void repeatedprimitivefieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $classname$::$name$(int index) const {\n"
    "  return $name$_.get(index);\n"
    "}\n"
    "inline void $classname$::set_$name$(int index, $type$ value) {\n"
    "  $name$_.set(index, value);\n"
    "}\n"
    "inline void $classname$::add_$name$($type$ value) {\n"
    "  $name$_.add(value);\n"
    "}\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedfield< $type$ >&\n"
    "$classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline ::google::protobuf::repeatedfield< $type$ >*\n"
    "$classname$::mutable_$name$() {\n"
    "  return &$name$_;\n"
    "}\n");
}

void repeatedprimitivefieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.clear();\n");
}

void repeatedprimitivefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.mergefrom(from.$name$_);\n");
}

void repeatedprimitivefieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.swap(&other->$name$_);\n");
}

void repeatedprimitivefieldgenerator::
generateconstructorcode(io::printer* printer) const {
  // not needed for repeated fields.
}

void repeatedprimitivefieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  printer->print(variables_,
    "do_((::google::protobuf::internal::wireformatlite::$repeated_reader$<\n"
    "         $type$, $wire_format_field_type$>(\n"
    "       $tag_size$, $tag$, input, this->mutable_$name$())));\n");
}

void repeatedprimitivefieldgenerator::
generatemergefromcodedstreamwithpacking(io::printer* printer) const {
  printer->print(variables_,
    "do_((::google::protobuf::internal::wireformatlite::$packed_reader$<\n"
    "         $type$, $wire_format_field_type$>(\n"
    "       input, this->mutable_$name$())));\n");
}

void repeatedprimitivefieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    // write the tag and the size.
    printer->print(variables_,
      "if (this->$name$_size() > 0) {\n"
      "  ::google::protobuf::internal::wireformatlite::writetag("
          "$number$, "
          "::google::protobuf::internal::wireformatlite::wiretype_length_delimited, "
          "output);\n"
      "  output->writevarint32(_$name$_cached_byte_size_);\n"
      "}\n");
  }
  printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "  ::google::protobuf::internal::wireformatlite::write$declared_type$notag(\n"
      "    this->$name$(i), output);\n");
  } else {
    printer->print(variables_,
      "  ::google::protobuf::internal::wireformatlite::write$declared_type$(\n"
      "    $number$, this->$name$(i), output);\n");
  }
  printer->print("}\n");
}

void repeatedprimitivefieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    // write the tag and the size.
    printer->print(variables_,
      "if (this->$name$_size() > 0) {\n"
      "  target = ::google::protobuf::internal::wireformatlite::writetagtoarray(\n"
      "    $number$,\n"
      "    ::google::protobuf::internal::wireformatlite::wiretype_length_delimited,\n"
      "    target);\n"
      "  target = ::google::protobuf::io::codedoutputstream::writevarint32toarray(\n"
      "    _$name$_cached_byte_size_, target);\n"
      "}\n");
  }
  printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "  target = ::google::protobuf::internal::wireformatlite::\n"
      "    write$declared_type$notagtoarray(this->$name$(i), target);\n");
  } else {
    printer->print(variables_,
      "  target = ::google::protobuf::internal::wireformatlite::\n"
      "    write$declared_type$toarray($number$, this->$name$(i), target);\n");
  }
  printer->print("}\n");
}

void repeatedprimitivefieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "{\n"
    "  int data_size = 0;\n");
  printer->indent();
  int fixed_size = fixedsize(descriptor_->type());
  if (fixed_size == -1) {
    printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n"
      "  data_size += ::google::protobuf::internal::wireformatlite::\n"
      "    $declared_type$size(this->$name$(i));\n"
      "}\n");
  } else {
    printer->print(variables_,
      "data_size = $fixed_size$ * this->$name$_size();\n");
  }

  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "if (data_size > 0) {\n"
      "  total_size += $tag_size$ +\n"
      "    ::google::protobuf::internal::wireformatlite::int32size(data_size);\n"
      "}\n"
      "google_safe_concurrent_writes_begin();\n"
      "_$name$_cached_byte_size_ = data_size;\n"
      "google_safe_concurrent_writes_end();\n"
      "total_size += data_size;\n");
  } else {
    printer->print(variables_,
      "total_size += $tag_size$ * this->$name$_size() + data_size;\n");
  }
  printer->outdent();
  printer->print("}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
