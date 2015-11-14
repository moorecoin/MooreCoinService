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

#include <set>
#include <map>

#include <google/protobuf/compiler/cpp/cpp_enum.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

enumgenerator::enumgenerator(const enumdescriptor* descriptor,
                             const options& options)
  : descriptor_(descriptor),
    classname_(classname(descriptor, false)),
    options_(options) {
}

enumgenerator::~enumgenerator() {}

void enumgenerator::generatedefinition(io::printer* printer) {
  map<string, string> vars;
  vars["classname"] = classname_;
  vars["short_name"] = descriptor_->name();

  printer->print(vars, "enum $classname$ {\n");
  printer->indent();

  const enumvaluedescriptor* min_value = descriptor_->value(0);
  const enumvaluedescriptor* max_value = descriptor_->value(0);

  for (int i = 0; i < descriptor_->value_count(); i++) {
    vars["name"] = descriptor_->value(i)->name();
    vars["number"] = simpleitoa(descriptor_->value(i)->number());
    vars["prefix"] = (descriptor_->containing_type() == null) ?
      "" : classname_ + "_";

    if (i > 0) printer->print(",\n");
    printer->print(vars, "$prefix$$name$ = $number$");

    if (descriptor_->value(i)->number() < min_value->number()) {
      min_value = descriptor_->value(i);
    }
    if (descriptor_->value(i)->number() > max_value->number()) {
      max_value = descriptor_->value(i);
    }
  }

  printer->outdent();
  printer->print("\n};\n");

  vars["min_name"] = min_value->name();
  vars["max_name"] = max_value->name();

  if (options_.dllexport_decl.empty()) {
    vars["dllexport"] = "";
  } else {
    vars["dllexport"] = options_.dllexport_decl + " ";
  }

  printer->print(vars,
    "$dllexport$bool $classname$_isvalid(int value);\n"
    "const $classname$ $prefix$$short_name$_min = $prefix$$min_name$;\n"
    "const $classname$ $prefix$$short_name$_max = $prefix$$max_name$;\n"
    "const int $prefix$$short_name$_arraysize = $prefix$$short_name$_max + 1;\n"
    "\n");

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(vars,
      "$dllexport$const ::google::protobuf::enumdescriptor* $classname$_descriptor();\n");
    // the _name and _parse methods
    printer->print(vars,
      "inline const ::std::string& $classname$_name($classname$ value) {\n"
      "  return ::google::protobuf::internal::nameofenum(\n"
      "    $classname$_descriptor(), value);\n"
      "}\n");
    printer->print(vars,
      "inline bool $classname$_parse(\n"
      "    const ::std::string& name, $classname$* value) {\n"
      "  return ::google::protobuf::internal::parsenamedenum<$classname$>(\n"
      "    $classname$_descriptor(), name, value);\n"
      "}\n");
  }
}

void enumgenerator::
generategetenumdescriptorspecializations(io::printer* printer) {
  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(
      "template <>\n"
      "inline const enumdescriptor* getenumdescriptor< $classname$>() {\n"
      "  return $classname$_descriptor();\n"
      "}\n",
      "classname", classname(descriptor_, true));
  }
}

void enumgenerator::generatesymbolimports(io::printer* printer) {
  map<string, string> vars;
  vars["nested_name"] = descriptor_->name();
  vars["classname"] = classname_;
  printer->print(vars, "typedef $classname$ $nested_name$;\n");

  for (int j = 0; j < descriptor_->value_count(); j++) {
    vars["tag"] = descriptor_->value(j)->name();
    printer->print(vars,
      "static const $nested_name$ $tag$ = $classname$_$tag$;\n");
  }

  printer->print(vars,
    "static inline bool $nested_name$_isvalid(int value) {\n"
    "  return $classname$_isvalid(value);\n"
    "}\n"
    "static const $nested_name$ $nested_name$_min =\n"
    "  $classname$_$nested_name$_min;\n"
    "static const $nested_name$ $nested_name$_max =\n"
    "  $classname$_$nested_name$_max;\n"
    "static const int $nested_name$_arraysize =\n"
    "  $classname$_$nested_name$_arraysize;\n");

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(vars,
      "static inline const ::google::protobuf::enumdescriptor*\n"
      "$nested_name$_descriptor() {\n"
      "  return $classname$_descriptor();\n"
      "}\n");
    printer->print(vars,
      "static inline const ::std::string& $nested_name$_name($nested_name$ value) {\n"
      "  return $classname$_name(value);\n"
      "}\n");
    printer->print(vars,
      "static inline bool $nested_name$_parse(const ::std::string& name,\n"
      "    $nested_name$* value) {\n"
      "  return $classname$_parse(name, value);\n"
      "}\n");
  }
}

void enumgenerator::generatedescriptorinitializer(
    io::printer* printer, int index) {
  map<string, string> vars;
  vars["classname"] = classname_;
  vars["index"] = simpleitoa(index);

  if (descriptor_->containing_type() == null) {
    printer->print(vars,
      "$classname$_descriptor_ = file->enum_type($index$);\n");
  } else {
    vars["parent"] = classname(descriptor_->containing_type(), false);
    printer->print(vars,
      "$classname$_descriptor_ = $parent$_descriptor_->enum_type($index$);\n");
  }
}

void enumgenerator::generatemethods(io::printer* printer) {
  map<string, string> vars;
  vars["classname"] = classname_;

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(vars,
      "const ::google::protobuf::enumdescriptor* $classname$_descriptor() {\n"
      "  protobuf_assigndescriptorsonce();\n"
      "  return $classname$_descriptor_;\n"
      "}\n");
  }

  printer->print(vars,
    "bool $classname$_isvalid(int value) {\n"
    "  switch(value) {\n");

  // multiple values may have the same number.  make sure we only cover
  // each number once by first constructing a set containing all valid
  // numbers, then printing a case statement for each element.

  set<int> numbers;
  for (int j = 0; j < descriptor_->value_count(); j++) {
    const enumvaluedescriptor* value = descriptor_->value(j);
    numbers.insert(value->number());
  }

  for (set<int>::iterator iter = numbers.begin();
       iter != numbers.end(); ++iter) {
    printer->print(
      "    case $number$:\n",
      "number", simpleitoa(*iter));
  }

  printer->print(vars,
    "      return true;\n"
    "    default:\n"
    "      return false;\n"
    "  }\n"
    "}\n"
    "\n");

  if (descriptor_->containing_type() != null) {
    // we need to "define" the static constants which were declared in the
    // header, to give the linker a place to put them.  or at least the c++
    // standard says we have to.  msvc actually insists tha we do _not_ define
    // them again in the .cc file.
    printer->print("#ifndef _msc_ver\n");

    vars["parent"] = classname(descriptor_->containing_type(), false);
    vars["nested_name"] = descriptor_->name();
    for (int i = 0; i < descriptor_->value_count(); i++) {
      vars["value"] = descriptor_->value(i)->name();
      printer->print(vars,
        "const $classname$ $parent$::$value$;\n");
    }
    printer->print(vars,
      "const $classname$ $parent$::$nested_name$_min;\n"
      "const $classname$ $parent$::$nested_name$_max;\n"
      "const int $parent$::$nested_name$_arraysize;\n");

    printer->print("#endif  // _msc_ver\n");
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
