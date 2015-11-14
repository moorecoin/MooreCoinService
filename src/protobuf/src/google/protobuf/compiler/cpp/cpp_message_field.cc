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

#include <google/protobuf/compiler/cpp/cpp_message_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void setmessagevariables(const fielddescriptor* descriptor,
                         map<string, string>* variables,
                         const options& options) {
  setcommonfieldvariables(descriptor, variables, options);
  (*variables)["type"] = fieldmessagetypename(descriptor);
  (*variables)["stream_writer"] = (*variables)["declared_type"] +
      (hasfastarrayserialization(descriptor->message_type()->file()) ?
       "maybetoarray" :
       "");
}

}  // namespace

// ===================================================================

messagefieldgenerator::
messagefieldgenerator(const fielddescriptor* descriptor,
                      const options& options)
  : descriptor_(descriptor) {
  setmessagevariables(descriptor, &variables_, options);
}

messagefieldgenerator::~messagefieldgenerator() {}

void messagefieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_, "$type$* $name$_;\n");
}

void messagefieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline const $type$& $name$() const$deprecation$;\n"
    "inline $type$* mutable_$name$()$deprecation$;\n"
    "inline $type$* release_$name$()$deprecation$;\n"
    "inline void set_allocated_$name$($type$* $name$)$deprecation$;\n");
}

void messagefieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline const $type$& $classname$::$name$() const {\n");

  printhandlingoptionalstaticinitializers(
    variables_, descriptor_->file(), printer,
    // with static initializers.
    "  return $name$_ != null ? *$name$_ : *default_instance_->$name$_;\n",
    // without.
    "  return $name$_ != null ? *$name$_ : *default_instance().$name$_;\n");

  printer->print(variables_,
    "}\n"
    "inline $type$* $classname$::mutable_$name$() {\n"
    "  set_has_$name$();\n"
    "  if ($name$_ == null) $name$_ = new $type$;\n"
    "  return $name$_;\n"
    "}\n"
    "inline $type$* $classname$::release_$name$() {\n"
    "  clear_has_$name$();\n"
    "  $type$* temp = $name$_;\n"
    "  $name$_ = null;\n"
    "  return temp;\n"
    "}\n"
    "inline void $classname$::set_allocated_$name$($type$* $name$) {\n"
    "  delete $name$_;\n"
    "  $name$_ = $name$;\n"
    "  if ($name$) {\n"
    "    set_has_$name$();\n"
    "  } else {\n"
    "    clear_has_$name$();\n"
    "  }\n"
    "}\n");
}

void messagefieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($name$_ != null) $name$_->$type$::clear();\n");
}

void messagefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_,
    "mutable_$name$()->$type$::mergefrom(from.$name$());\n");
}

void messagefieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void messagefieldgenerator::
generateconstructorcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = null;\n");
}

void messagefieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  if (descriptor_->type() == fielddescriptor::type_message) {
    printer->print(variables_,
      "do_(::google::protobuf::internal::wireformatlite::readmessagenovirtual(\n"
      "     input, mutable_$name$()));\n");
  } else {
    printer->print(variables_,
      "do_(::google::protobuf::internal::wireformatlite::readgroupnovirtual(\n"
      "      $number$, input, mutable_$name$()));\n");
  }
}

void messagefieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::internal::wireformatlite::write$stream_writer$(\n"
    "  $number$, this->$name$(), output);\n");
}

void messagefieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  printer->print(variables_,
    "target = ::google::protobuf::internal::wireformatlite::\n"
    "  write$declared_type$novirtualtoarray(\n"
    "    $number$, this->$name$(), target);\n");
}

void messagefieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "total_size += $tag_size$ +\n"
    "  ::google::protobuf::internal::wireformatlite::$declared_type$sizenovirtual(\n"
    "    this->$name$());\n");
}

// ===================================================================

repeatedmessagefieldgenerator::
repeatedmessagefieldgenerator(const fielddescriptor* descriptor,
                              const options& options)
  : descriptor_(descriptor) {
  setmessagevariables(descriptor, &variables_, options);
}

repeatedmessagefieldgenerator::~repeatedmessagefieldgenerator() {}

void repeatedmessagefieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::repeatedptrfield< $type$ > $name$_;\n");
}

void repeatedmessagefieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  printer->print(variables_,
    "inline const $type$& $name$(int index) const$deprecation$;\n"
    "inline $type$* mutable_$name$(int index)$deprecation$;\n"
    "inline $type$* add_$name$()$deprecation$;\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedptrfield< $type$ >&\n"
    "    $name$() const$deprecation$;\n"
    "inline ::google::protobuf::repeatedptrfield< $type$ >*\n"
    "    mutable_$name$()$deprecation$;\n");
}

void repeatedmessagefieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline const $type$& $classname$::$name$(int index) const {\n"
    "  return $name$_.$cppget$(index);\n"
    "}\n"
    "inline $type$* $classname$::mutable_$name$(int index) {\n"
    "  return $name$_.mutable(index);\n"
    "}\n"
    "inline $type$* $classname$::add_$name$() {\n"
    "  return $name$_.add();\n"
    "}\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedptrfield< $type$ >&\n"
    "$classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline ::google::protobuf::repeatedptrfield< $type$ >*\n"
    "$classname$::mutable_$name$() {\n"
    "  return &$name$_;\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.clear();\n");
}

void repeatedmessagefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.mergefrom(from.$name$_);\n");
}

void repeatedmessagefieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.swap(&other->$name$_);\n");
}

void repeatedmessagefieldgenerator::
generateconstructorcode(io::printer* printer) const {
  // not needed for repeated fields.
}

void repeatedmessagefieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  if (descriptor_->type() == fielddescriptor::type_message) {
    printer->print(variables_,
      "do_(::google::protobuf::internal::wireformatlite::readmessagenovirtual(\n"
      "      input, add_$name$()));\n");
  } else {
    printer->print(variables_,
      "do_(::google::protobuf::internal::wireformatlite::readgroupnovirtual(\n"
      "      $number$, input, add_$name$()));\n");
  }
}

void repeatedmessagefieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  ::google::protobuf::internal::wireformatlite::write$stream_writer$(\n"
    "    $number$, this->$name$(i), output);\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  target = ::google::protobuf::internal::wireformatlite::\n"
    "    write$declared_type$novirtualtoarray(\n"
    "      $number$, this->$name$(i), target);\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "total_size += $tag_size$ * this->$name$_size();\n"
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  total_size +=\n"
    "    ::google::protobuf::internal::wireformatlite::$declared_type$sizenovirtual(\n"
    "      this->$name$(i));\n"
    "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
