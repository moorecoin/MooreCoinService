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

#include <map>
#include <string>

#include <google/protobuf/compiler/java/java_enum.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

enumgenerator::enumgenerator(const enumdescriptor* descriptor)
  : descriptor_(descriptor) {
  for (int i = 0; i < descriptor_->value_count(); i++) {
    const enumvaluedescriptor* value = descriptor_->value(i);
    const enumvaluedescriptor* canonical_value =
      descriptor_->findvaluebynumber(value->number());

    if (value == canonical_value) {
      canonical_values_.push_back(value);
    } else {
      alias alias;
      alias.value = value;
      alias.canonical_value = canonical_value;
      aliases_.push_back(alias);
    }
  }
}

enumgenerator::~enumgenerator() {}

void enumgenerator::generate(io::printer* printer) {
  writeenumdoccomment(printer, descriptor_);
  if (hasdescriptormethods(descriptor_)) {
    printer->print(
      "public enum $classname$\n"
      "    implements com.google.protobuf.protocolmessageenum {\n",
      "classname", descriptor_->name());
  } else {
    printer->print(
      "public enum $classname$\n"
      "    implements com.google.protobuf.internal.enumlite {\n",
      "classname", descriptor_->name());
  }
  printer->indent();

  for (int i = 0; i < canonical_values_.size(); i++) {
    map<string, string> vars;
    vars["name"] = canonical_values_[i]->name();
    vars["index"] = simpleitoa(canonical_values_[i]->index());
    vars["number"] = simpleitoa(canonical_values_[i]->number());
    writeenumvaluedoccomment(printer, canonical_values_[i]);
    printer->print(vars,
      "$name$($index$, $number$),\n");
  }

  printer->print(
    ";\n"
    "\n");

  // -----------------------------------------------------------------

  for (int i = 0; i < aliases_.size(); i++) {
    map<string, string> vars;
    vars["classname"] = descriptor_->name();
    vars["name"] = aliases_[i].value->name();
    vars["canonical_name"] = aliases_[i].canonical_value->name();
    writeenumvaluedoccomment(printer, aliases_[i].value);
    printer->print(vars,
      "public static final $classname$ $name$ = $canonical_name$;\n");
  }

  for (int i = 0; i < descriptor_->value_count(); i++) {
    map<string, string> vars;
    vars["name"] = descriptor_->value(i)->name();
    vars["number"] = simpleitoa(descriptor_->value(i)->number());
    writeenumvaluedoccomment(printer, descriptor_->value(i));
    printer->print(vars,
      "public static final int $name$_value = $number$;\n");
  }
  printer->print("\n");

  // -----------------------------------------------------------------

  printer->print(
    "\n"
    "public final int getnumber() { return value; }\n"
    "\n"
    "public static $classname$ valueof(int value) {\n"
    "  switch (value) {\n",
    "classname", descriptor_->name());
  printer->indent();
  printer->indent();

  for (int i = 0; i < canonical_values_.size(); i++) {
    printer->print(
      "case $number$: return $name$;\n",
      "name", canonical_values_[i]->name(),
      "number", simpleitoa(canonical_values_[i]->number()));
  }

  printer->outdent();
  printer->outdent();
  printer->print(
    "    default: return null;\n"
    "  }\n"
    "}\n"
    "\n"
    "public static com.google.protobuf.internal.enumlitemap<$classname$>\n"
    "    internalgetvaluemap() {\n"
    "  return internalvaluemap;\n"
    "}\n"
    "private static com.google.protobuf.internal.enumlitemap<$classname$>\n"
    "    internalvaluemap =\n"
    "      new com.google.protobuf.internal.enumlitemap<$classname$>() {\n"
    "        public $classname$ findvaluebynumber(int number) {\n"
    "          return $classname$.valueof(number);\n"
    "        }\n"
    "      };\n"
    "\n",
    "classname", descriptor_->name());

  // -----------------------------------------------------------------
  // reflection

  if (hasdescriptormethods(descriptor_)) {
    printer->print(
      "public final com.google.protobuf.descriptors.enumvaluedescriptor\n"
      "    getvaluedescriptor() {\n"
      "  return getdescriptor().getvalues().get(index);\n"
      "}\n"
      "public final com.google.protobuf.descriptors.enumdescriptor\n"
      "    getdescriptorfortype() {\n"
      "  return getdescriptor();\n"
      "}\n"
      "public static final com.google.protobuf.descriptors.enumdescriptor\n"
      "    getdescriptor() {\n");

    // todo(kenton):  cache statically?  note that we can't access descriptors
    //   at module init time because it wouldn't work with descriptor.proto, but
    //   we can cache the value the first time getdescriptor() is called.
    if (descriptor_->containing_type() == null) {
      printer->print(
        "  return $file$.getdescriptor().getenumtypes().get($index$);\n",
        "file", classname(descriptor_->file()),
        "index", simpleitoa(descriptor_->index()));
    } else {
      printer->print(
        "  return $parent$.getdescriptor().getenumtypes().get($index$);\n",
        "parent", classname(descriptor_->containing_type()),
        "index", simpleitoa(descriptor_->index()));
    }

    printer->print(
      "}\n"
      "\n"
      "private static final $classname$[] values = ",
      "classname", descriptor_->name());

    if (canuseenumvalues()) {
      // if the constants we are going to output are exactly the ones we
      // have declared in the java enum in the same order, then we can use
      // the values() method that the java compiler automatically generates
      // for every enum.
      printer->print("values();\n");
    } else {
      printer->print(
        "{\n"
        "  ");
      for (int i = 0; i < descriptor_->value_count(); i++) {
        printer->print("$name$, ",
          "name", descriptor_->value(i)->name());
      }
      printer->print(
          "\n"
          "};\n");
    }

    printer->print(
      "\n"
      "public static $classname$ valueof(\n"
      "    com.google.protobuf.descriptors.enumvaluedescriptor desc) {\n"
      "  if (desc.gettype() != getdescriptor()) {\n"
      "    throw new java.lang.illegalargumentexception(\n"
      "      \"enumvaluedescriptor is not for this type.\");\n"
      "  }\n"
      "  return values[desc.getindex()];\n"
      "}\n"
      "\n",
      "classname", descriptor_->name());

    // index is only used for reflection; lite implementation does not need it
    printer->print("private final int index;\n");
  }

  // -----------------------------------------------------------------

  printer->print(
    "private final int value;\n\n"
    "private $classname$(int index, int value) {\n",
    "classname", descriptor_->name());
  if (hasdescriptormethods(descriptor_)) {
    printer->print("  this.index = index;\n");
  }
  printer->print(
    "  this.value = value;\n"
    "}\n");

  printer->print(
    "\n"
    "// @@protoc_insertion_point(enum_scope:$full_name$)\n",
    "full_name", descriptor_->full_name());

  printer->outdent();
  printer->print("}\n\n");
}

bool enumgenerator::canuseenumvalues() {
  if (canonical_values_.size() != descriptor_->value_count()) {
    return false;
  }
  for (int i = 0; i < descriptor_->value_count(); i++) {
    if (descriptor_->value(i)->name() != canonical_values_[i]->name()) {
      return false;
    }
  }
  return true;
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
