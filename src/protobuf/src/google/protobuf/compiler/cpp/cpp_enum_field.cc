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

#include <google/protobuf/compiler/cpp/cpp_enum_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void setenumvariables(const fielddescriptor* descriptor,
                      map<string, string>* variables,
                      const options& options) {
  setcommonfieldvariables(descriptor, variables, options);
  const enumvaluedescriptor* default_value = descriptor->default_value_enum();
  (*variables)["type"] = classname(descriptor->enum_type(), true);
  (*variables)["default"] = simpleitoa(default_value->number());
}

}  // namespace

// ===================================================================

enumfieldgenerator::
enumfieldgenerator(const fielddescriptor* descriptor,
                   const options& options)
  : descriptor_(descriptor) {
  setenumvariables(descriptor, &variables_, options);
}

enumfieldgenerator::~enumfieldgenerator() {}

void enumfieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_, "int $name$_;\n");
}

void enumfieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $name$() const$deprecation$;\n"
    "inline void set_$name$($type$ value)$deprecation$;\n");
}

void enumfieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $classname$::$name$() const {\n"
    "  return static_cast< $type$ >($name$_);\n"
    "}\n"
    "inline void $classname$::set_$name$($type$ value) {\n"
    "  assert($type$_isvalid(value));\n"
    "  set_has_$name$();\n"
    "  $name$_ = value;\n"
    "}\n");
}

void enumfieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void enumfieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "set_$name$(from.$name$());\n");
}

void enumfieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void enumfieldgenerator::
generateconstructorcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void enumfieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  printer->print(variables_,
    "int value;\n"
    "do_((::google::protobuf::internal::wireformatlite::readprimitive<\n"
    "         int, ::google::protobuf::internal::wireformatlite::type_enum>(\n"
    "       input, &value)));\n"
    "if ($type$_isvalid(value)) {\n"
    "  set_$name$(static_cast< $type$ >(value));\n");
  if (hasunknownfields(descriptor_->file())) {
    printer->print(variables_,
      "} else {\n"
      "  mutable_unknown_fields()->addvarint($number$, value);\n");
  }
  printer->print(variables_,
    "}\n");
}

void enumfieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::internal::wireformatlite::writeenum(\n"
    "  $number$, this->$name$(), output);\n");
}

void enumfieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  printer->print(variables_,
    "target = ::google::protobuf::internal::wireformatlite::writeenumtoarray(\n"
    "  $number$, this->$name$(), target);\n");
}

void enumfieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "total_size += $tag_size$ +\n"
    "  ::google::protobuf::internal::wireformatlite::enumsize(this->$name$());\n");
}

// ===================================================================

repeatedenumfieldgenerator::
repeatedenumfieldgenerator(const fielddescriptor* descriptor,
                           const options& options)
  : descriptor_(descriptor) {
  setenumvariables(descriptor, &variables_, options);
}

repeatedenumfieldgenerator::~repeatedenumfieldgenerator() {}

void repeatedenumfieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::repeatedfield<int> $name$_;\n");
  if (descriptor_->options().packed() && hasgeneratedmethods(descriptor_->file())) {
    printer->print(variables_,
      "mutable int _$name$_cached_byte_size_;\n");
  }
}

void repeatedenumfieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $name$(int index) const$deprecation$;\n"
    "inline void set_$name$(int index, $type$ value)$deprecation$;\n"
    "inline void add_$name$($type$ value)$deprecation$;\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedfield<int>& $name$() const$deprecation$;\n"
    "inline ::google::protobuf::repeatedfield<int>* mutable_$name$()$deprecation$;\n");
}

void repeatedenumfieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline $type$ $classname$::$name$(int index) const {\n"
    "  return static_cast< $type$ >($name$_.get(index));\n"
    "}\n"
    "inline void $classname$::set_$name$(int index, $type$ value) {\n"
    "  assert($type$_isvalid(value));\n"
    "  $name$_.set(index, value);\n"
    "}\n"
    "inline void $classname$::add_$name$($type$ value) {\n"
    "  assert($type$_isvalid(value));\n"
    "  $name$_.add(value);\n"
    "}\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedfield<int>&\n"
    "$classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline ::google::protobuf::repeatedfield<int>*\n"
    "$classname$::mutable_$name$() {\n"
    "  return &$name$_;\n"
    "}\n");
}

void repeatedenumfieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.clear();\n");
}

void repeatedenumfieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.mergefrom(from.$name$_);\n");
}

void repeatedenumfieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.swap(&other->$name$_);\n");
}

void repeatedenumfieldgenerator::
generateconstructorcode(io::printer* printer) const {
  // not needed for repeated fields.
}

void repeatedenumfieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  // don't use readrepeatedprimitive here so that the enum can be validated.
  printer->print(variables_,
    "int value;\n"
    "do_((::google::protobuf::internal::wireformatlite::readprimitive<\n"
    "         int, ::google::protobuf::internal::wireformatlite::type_enum>(\n"
    "       input, &value)));\n"
    "if ($type$_isvalid(value)) {\n"
    "  add_$name$(static_cast< $type$ >(value));\n");
  if (hasunknownfields(descriptor_->file())) {
    printer->print(variables_,
      "} else {\n"
      "  mutable_unknown_fields()->addvarint($number$, value);\n");
  }
  printer->print("}\n");
}

void repeatedenumfieldgenerator::
generatemergefromcodedstreamwithpacking(io::printer* printer) const {
  if (!descriptor_->options().packed()) {
    // we use a non-inlined implementation in this case, since this path will
    // rarely be executed.
    printer->print(variables_,
      "do_((::google::protobuf::internal::wireformatlite::readpackedenumnoinline(\n"
      "       input,\n"
      "       &$type$_isvalid,\n"
      "       this->mutable_$name$())));\n");
  } else {
    printer->print(variables_,
      "::google::protobuf::uint32 length;\n"
      "do_(input->readvarint32(&length));\n"
      "::google::protobuf::io::codedinputstream::limit limit = "
          "input->pushlimit(length);\n"
      "while (input->bytesuntillimit() > 0) {\n"
      "  int value;\n"
      "  do_((::google::protobuf::internal::wireformatlite::readprimitive<\n"
      "         int, ::google::protobuf::internal::wireformatlite::type_enum>(\n"
      "       input, &value)));\n"
      "  if ($type$_isvalid(value)) {\n"
      "    add_$name$(static_cast< $type$ >(value));\n"
      "  }\n"
      "}\n"
      "input->poplimit(limit);\n");
  }
}

void repeatedenumfieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    // write the tag and the size.
    printer->print(variables_,
      "if (this->$name$_size() > 0) {\n"
      "  ::google::protobuf::internal::wireformatlite::writetag(\n"
      "    $number$,\n"
      "    ::google::protobuf::internal::wireformatlite::wiretype_length_delimited,\n"
      "    output);\n"
      "  output->writevarint32(_$name$_cached_byte_size_);\n"
      "}\n");
  }
  printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "  ::google::protobuf::internal::wireformatlite::writeenumnotag(\n"
      "    this->$name$(i), output);\n");
  } else {
    printer->print(variables_,
      "  ::google::protobuf::internal::wireformatlite::writeenum(\n"
      "    $number$, this->$name$(i), output);\n");
  }
  printer->print("}\n");
}

void repeatedenumfieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    // write the tag and the size.
    printer->print(variables_,
      "if (this->$name$_size() > 0) {\n"
      "  target = ::google::protobuf::internal::wireformatlite::writetagtoarray(\n"
      "    $number$,\n"
      "    ::google::protobuf::internal::wireformatlite::wiretype_length_delimited,\n"
      "    target);\n"
      "  target = ::google::protobuf::io::codedoutputstream::writevarint32toarray("
      "    _$name$_cached_byte_size_, target);\n"
      "}\n");
  }
  printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "  target = ::google::protobuf::internal::wireformatlite::writeenumnotagtoarray(\n"
      "    this->$name$(i), target);\n");
  } else {
    printer->print(variables_,
      "  target = ::google::protobuf::internal::wireformatlite::writeenumtoarray(\n"
      "    $number$, this->$name$(i), target);\n");
  }
  printer->print("}\n");
}

void repeatedenumfieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "{\n"
    "  int data_size = 0;\n");
  printer->indent();
  printer->print(variables_,
      "for (int i = 0; i < this->$name$_size(); i++) {\n"
      "  data_size += ::google::protobuf::internal::wireformatlite::enumsize(\n"
      "    this->$name$(i));\n"
      "}\n");

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
