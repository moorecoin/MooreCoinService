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

#include <google/protobuf/compiler/cpp/cpp_string_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

void setstringvariables(const fielddescriptor* descriptor,
                        map<string, string>* variables,
                        const options& options) {
  setcommonfieldvariables(descriptor, variables, options);
  (*variables)["default"] = defaultvalue(descriptor);
  (*variables)["default_length"] =
      simpleitoa(descriptor->default_value_string().length());
  (*variables)["default_variable"] = descriptor->default_value_string().empty()
      ? "&::google::protobuf::internal::kemptystring"
      : "_default_" + fieldname(descriptor) + "_";
  (*variables)["pointer_type"] =
      descriptor->type() == fielddescriptor::type_bytes ? "void" : "char";
}

}  // namespace

// ===================================================================

stringfieldgenerator::
stringfieldgenerator(const fielddescriptor* descriptor,
                     const options& options)
  : descriptor_(descriptor) {
  setstringvariables(descriptor, &variables_, options);
}

stringfieldgenerator::~stringfieldgenerator() {}

void stringfieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_, "::std::string* $name$_;\n");
  if (!descriptor_->default_value_string().empty()) {
    printer->print(variables_, "static ::std::string* $default_variable$;\n");
  }
}

void stringfieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  // if we're using stringfieldgenerator for a field with a ctype, it's
  // because that ctype isn't actually implemented.  in particular, this is
  // true of ctype=cord and ctype=string_piece in the open source release.
  // we aren't releasing cord because it has too many google-specific
  // dependencies and we aren't releasing stringpiece because it's hardly
  // useful outside of google and because it would get confusing to have
  // multiple instances of the stringpiece class in different libraries (pcre
  // already includes it for their c++ bindings, which came from google).
  //
  // in any case, we make all the accessors private while still actually
  // using a string to represent the field internally.  this way, we can
  // guarantee that if we do ever implement the ctype, it won't break any
  // existing users who might be -- for whatever reason -- already using .proto
  // files that applied the ctype.  the field can still be accessed via the
  // reflection interface since the reflection interface is independent of
  // the string's underlying representation.
  if (descriptor_->options().ctype() != fieldoptions::string) {
    printer->outdent();
    printer->print(
      " private:\n"
      "  // hidden due to unknown ctype option.\n");
    printer->indent();
  }

  printer->print(variables_,
    "inline const ::std::string& $name$() const$deprecation$;\n"
    "inline void set_$name$(const ::std::string& value)$deprecation$;\n"
    "inline void set_$name$(const char* value)$deprecation$;\n"
    "inline void set_$name$(const $pointer_type$* value, size_t size)"
                 "$deprecation$;\n"
    "inline ::std::string* mutable_$name$()$deprecation$;\n"
    "inline ::std::string* release_$name$()$deprecation$;\n"
    "inline void set_allocated_$name$(::std::string* $name$)$deprecation$;\n");


  if (descriptor_->options().ctype() != fieldoptions::string) {
    printer->outdent();
    printer->print(" public:\n");
    printer->indent();
  }
}

void stringfieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline const ::std::string& $classname$::$name$() const {\n"
    "  return *$name$_;\n"
    "}\n"
    "inline void $classname$::set_$name$(const ::std::string& value) {\n"
    "  set_has_$name$();\n"
    "  if ($name$_ == $default_variable$) {\n"
    "    $name$_ = new ::std::string;\n"
    "  }\n"
    "  $name$_->assign(value);\n"
    "}\n"
    "inline void $classname$::set_$name$(const char* value) {\n"
    "  set_has_$name$();\n"
    "  if ($name$_ == $default_variable$) {\n"
    "    $name$_ = new ::std::string;\n"
    "  }\n"
    "  $name$_->assign(value);\n"
    "}\n"
    "inline "
    "void $classname$::set_$name$(const $pointer_type$* value, size_t size) {\n"
    "  set_has_$name$();\n"
    "  if ($name$_ == $default_variable$) {\n"
    "    $name$_ = new ::std::string;\n"
    "  }\n"
    "  $name$_->assign(reinterpret_cast<const char*>(value), size);\n"
    "}\n"
    "inline ::std::string* $classname$::mutable_$name$() {\n"
    "  set_has_$name$();\n"
    "  if ($name$_ == $default_variable$) {\n");
  if (descriptor_->default_value_string().empty()) {
    printer->print(variables_,
      "    $name$_ = new ::std::string;\n");
  } else {
    printer->print(variables_,
      "    $name$_ = new ::std::string(*$default_variable$);\n");
  }
  printer->print(variables_,
    "  }\n"
    "  return $name$_;\n"
    "}\n"
    "inline ::std::string* $classname$::release_$name$() {\n"
    "  clear_has_$name$();\n"
    "  if ($name$_ == $default_variable$) {\n"
    "    return null;\n"
    "  } else {\n"
    "    ::std::string* temp = $name$_;\n"
    "    $name$_ = const_cast< ::std::string*>($default_variable$);\n"
    "    return temp;\n"
    "  }\n"
    "}\n"
    "inline void $classname$::set_allocated_$name$(::std::string* $name$) {\n"
    "  if ($name$_ != $default_variable$) {\n"
    "    delete $name$_;\n"
    "  }\n"
    "  if ($name$) {\n"
    "    set_has_$name$();\n"
    "    $name$_ = $name$;\n"
    "  } else {\n"
    "    clear_has_$name$();\n"
    "    $name$_ = const_cast< ::std::string*>($default_variable$);\n"
    "  }\n"
    "}\n");
}

void stringfieldgenerator::
generatenoninlineaccessordefinitions(io::printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    // initialized in generatedefaultinstanceallocator.
    printer->print(variables_,
      "::std::string* $classname$::$default_variable$ = null;\n");
  }
}

void stringfieldgenerator::
generateclearingcode(io::printer* printer) const {
  if (descriptor_->default_value_string().empty()) {
    printer->print(variables_,
      "if ($name$_ != $default_variable$) {\n"
      "  $name$_->clear();\n"
      "}\n");
  } else {
    printer->print(variables_,
      "if ($name$_ != $default_variable$) {\n"
      "  $name$_->assign(*$default_variable$);\n"
      "}\n");
  }
}

void stringfieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "set_$name$(from.$name$());\n");
}

void stringfieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "std::swap($name$_, other->$name$_);\n");
}

void stringfieldgenerator::
generateconstructorcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = const_cast< ::std::string*>($default_variable$);\n");
}

void stringfieldgenerator::
generatedestructorcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($name$_ != $default_variable$) {\n"
    "  delete $name$_;\n"
    "}\n");
}

void stringfieldgenerator::
generatedefaultinstanceallocator(io::printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->print(variables_,
      "$classname$::$default_variable$ =\n"
      "    new ::std::string($default$, $default_length$);\n");
  }
}

void stringfieldgenerator::
generateshutdowncode(io::printer* printer) const {
  if (!descriptor_->default_value_string().empty()) {
    printer->print(variables_,
      "delete $classname$::$default_variable$;\n");
  }
}

void stringfieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  printer->print(variables_,
    "do_(::google::protobuf::internal::wireformatlite::read$declared_type$(\n"
    "      input, this->mutable_$name$()));\n");
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "  this->$name$().data(), this->$name$().length(),\n"
      "  ::google::protobuf::internal::wireformat::parse);\n");
  }
}

void stringfieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "  this->$name$().data(), this->$name$().length(),\n"
      "  ::google::protobuf::internal::wireformat::serialize);\n");
  }
  printer->print(variables_,
    "::google::protobuf::internal::wireformatlite::write$declared_type$(\n"
    "  $number$, this->$name$(), output);\n");
}

void stringfieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "  this->$name$().data(), this->$name$().length(),\n"
      "  ::google::protobuf::internal::wireformat::serialize);\n");
  }
  printer->print(variables_,
    "target =\n"
    "  ::google::protobuf::internal::wireformatlite::write$declared_type$toarray(\n"
    "    $number$, this->$name$(), target);\n");
}

void stringfieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "total_size += $tag_size$ +\n"
    "  ::google::protobuf::internal::wireformatlite::$declared_type$size(\n"
    "    this->$name$());\n");
}

// ===================================================================

repeatedstringfieldgenerator::
repeatedstringfieldgenerator(const fielddescriptor* descriptor,
                             const options& options)
  : descriptor_(descriptor) {
  setstringvariables(descriptor, &variables_, options);
}

repeatedstringfieldgenerator::~repeatedstringfieldgenerator() {}

void repeatedstringfieldgenerator::
generateprivatemembers(io::printer* printer) const {
  printer->print(variables_,
    "::google::protobuf::repeatedptrfield< ::std::string> $name$_;\n");
}

void repeatedstringfieldgenerator::
generateaccessordeclarations(io::printer* printer) const {
  // see comment above about unknown ctypes.
  if (descriptor_->options().ctype() != fieldoptions::string) {
    printer->outdent();
    printer->print(
      " private:\n"
      "  // hidden due to unknown ctype option.\n");
    printer->indent();
  }

  printer->print(variables_,
    "inline const ::std::string& $name$(int index) const$deprecation$;\n"
    "inline ::std::string* mutable_$name$(int index)$deprecation$;\n"
    "inline void set_$name$(int index, const ::std::string& value)$deprecation$;\n"
    "inline void set_$name$(int index, const char* value)$deprecation$;\n"
    "inline "
    "void set_$name$(int index, const $pointer_type$* value, size_t size)"
                 "$deprecation$;\n"
    "inline ::std::string* add_$name$()$deprecation$;\n"
    "inline void add_$name$(const ::std::string& value)$deprecation$;\n"
    "inline void add_$name$(const char* value)$deprecation$;\n"
    "inline void add_$name$(const $pointer_type$* value, size_t size)"
                 "$deprecation$;\n");

  printer->print(variables_,
    "inline const ::google::protobuf::repeatedptrfield< ::std::string>& $name$() const"
                 "$deprecation$;\n"
    "inline ::google::protobuf::repeatedptrfield< ::std::string>* mutable_$name$()"
                 "$deprecation$;\n");

  if (descriptor_->options().ctype() != fieldoptions::string) {
    printer->outdent();
    printer->print(" public:\n");
    printer->indent();
  }
}

void repeatedstringfieldgenerator::
generateinlineaccessordefinitions(io::printer* printer) const {
  printer->print(variables_,
    "inline const ::std::string& $classname$::$name$(int index) const {\n"
    "  return $name$_.$cppget$(index);\n"
    "}\n"
    "inline ::std::string* $classname$::mutable_$name$(int index) {\n"
    "  return $name$_.mutable(index);\n"
    "}\n"
    "inline void $classname$::set_$name$(int index, const ::std::string& value) {\n"
    "  $name$_.mutable(index)->assign(value);\n"
    "}\n"
    "inline void $classname$::set_$name$(int index, const char* value) {\n"
    "  $name$_.mutable(index)->assign(value);\n"
    "}\n"
    "inline void "
    "$classname$::set_$name$"
    "(int index, const $pointer_type$* value, size_t size) {\n"
    "  $name$_.mutable(index)->assign(\n"
    "    reinterpret_cast<const char*>(value), size);\n"
    "}\n"
    "inline ::std::string* $classname$::add_$name$() {\n"
    "  return $name$_.add();\n"
    "}\n"
    "inline void $classname$::add_$name$(const ::std::string& value) {\n"
    "  $name$_.add()->assign(value);\n"
    "}\n"
    "inline void $classname$::add_$name$(const char* value) {\n"
    "  $name$_.add()->assign(value);\n"
    "}\n"
    "inline void "
    "$classname$::add_$name$(const $pointer_type$* value, size_t size) {\n"
    "  $name$_.add()->assign(reinterpret_cast<const char*>(value), size);\n"
    "}\n");
  printer->print(variables_,
    "inline const ::google::protobuf::repeatedptrfield< ::std::string>&\n"
    "$classname$::$name$() const {\n"
    "  return $name$_;\n"
    "}\n"
    "inline ::google::protobuf::repeatedptrfield< ::std::string>*\n"
    "$classname$::mutable_$name$() {\n"
    "  return &$name$_;\n"
    "}\n");
}

void repeatedstringfieldgenerator::
generateclearingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.clear();\n");
}

void repeatedstringfieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.mergefrom(from.$name$_);\n");
}

void repeatedstringfieldgenerator::
generateswappingcode(io::printer* printer) const {
  printer->print(variables_, "$name$_.swap(&other->$name$_);\n");
}

void repeatedstringfieldgenerator::
generateconstructorcode(io::printer* printer) const {
  // not needed for repeated fields.
}

void repeatedstringfieldgenerator::
generatemergefromcodedstream(io::printer* printer) const {
  printer->print(variables_,
    "do_(::google::protobuf::internal::wireformatlite::read$declared_type$(\n"
    "      input, this->add_$name$()));\n");
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "  this->$name$(this->$name$_size() - 1).data(),\n"
      "  this->$name$(this->$name$_size() - 1).length(),\n"
      "  ::google::protobuf::internal::wireformat::parse);\n");
  }
}

void repeatedstringfieldgenerator::
generateserializewithcachedsizes(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "  this->$name$(i).data(), this->$name$(i).length(),\n"
      "  ::google::protobuf::internal::wireformat::serialize);\n");
  }
  printer->print(variables_,
    "  ::google::protobuf::internal::wireformatlite::write$declared_type$(\n"
    "    $number$, this->$name$(i), output);\n"
    "}\n");
}

void repeatedstringfieldgenerator::
generateserializewithcachedsizestoarray(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < this->$name$_size(); i++) {\n");
  if (hasutf8verification(descriptor_->file()) &&
      descriptor_->type() == fielddescriptor::type_string) {
    printer->print(variables_,
      "  ::google::protobuf::internal::wireformat::verifyutf8string(\n"
      "    this->$name$(i).data(), this->$name$(i).length(),\n"
      "    ::google::protobuf::internal::wireformat::serialize);\n");
  }
  printer->print(variables_,
    "  target = ::google::protobuf::internal::wireformatlite::\n"
    "    write$declared_type$toarray($number$, this->$name$(i), target);\n"
    "}\n");
}

void repeatedstringfieldgenerator::
generatebytesize(io::printer* printer) const {
  printer->print(variables_,
    "total_size += $tag_size$ * this->$name$_size();\n"
    "for (int i = 0; i < this->$name$_size(); i++) {\n"
    "  total_size += ::google::protobuf::internal::wireformatlite::$declared_type$size(\n"
    "    this->$name$(i));\n"
    "}\n");
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
