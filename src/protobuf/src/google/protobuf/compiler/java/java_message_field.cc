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

#include <google/protobuf/compiler/java/java_message_field.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {

// todo(kenton):  factor out a "setcommonfieldvariables()" to get rid of
//   repeat code between this and the other field types.
void setmessagevariables(const fielddescriptor* descriptor,
                         int messagebitindex,
                         int builderbitindex,
                         map<string, string>* variables) {
  (*variables)["name"] =
    underscorestocamelcase(descriptor);
  (*variables)["capitalized_name"] =
    underscorestocapitalizedcamelcase(descriptor);
  (*variables)["constant_name"] = fieldconstantname(descriptor);
  (*variables)["number"] = simpleitoa(descriptor->number());
  (*variables)["type"] = classname(descriptor->message_type());
  (*variables)["group_or_message"] =
    (gettype(descriptor) == fielddescriptor::type_group) ?
    "group" : "message";
  // todo(birdo): add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] = descriptor->options().deprecated()
      ? "@java.lang.deprecated " : "";
  (*variables)["on_changed"] =
      hasdescriptormethods(descriptor->containing_type()) ? "onchanged();" : "";

  // for singular messages and builders, one bit is used for the hasfield bit.
  (*variables)["get_has_field_bit_message"] = generategetbit(messagebitindex);
  (*variables)["set_has_field_bit_message"] = generatesetbit(messagebitindex);

  (*variables)["get_has_field_bit_builder"] = generategetbit(builderbitindex);
  (*variables)["set_has_field_bit_builder"] = generatesetbit(builderbitindex);
  (*variables)["clear_has_field_bit_builder"] =
      generateclearbit(builderbitindex);

  // for repated builders, one bit is used for whether the array is immutable.
  (*variables)["get_mutable_bit_builder"] = generategetbit(builderbitindex);
  (*variables)["set_mutable_bit_builder"] = generatesetbit(builderbitindex);
  (*variables)["clear_mutable_bit_builder"] = generateclearbit(builderbitindex);

  // for repeated fields, one bit is used for whether the array is immutable
  // in the parsing constructor.
  (*variables)["get_mutable_bit_parser"] =
      generategetbitmutablelocal(builderbitindex);
  (*variables)["set_mutable_bit_parser"] =
      generatesetbitmutablelocal(builderbitindex);

  (*variables)["get_has_field_bit_from_local"] =
      generategetbitfromlocal(builderbitindex);
  (*variables)["set_has_field_bit_to_local"] =
      generatesetbittolocal(messagebitindex);
}

}  // namespace

// ===================================================================

messagefieldgenerator::
messagefieldgenerator(const fielddescriptor* descriptor,
                      int messagebitindex,
                      int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setmessagevariables(descriptor, messagebitindex, builderbitindex,
                      &variables_);
}

messagefieldgenerator::~messagefieldgenerator() {}

int messagefieldgenerator::getnumbitsformessage() const {
  return 1;
}

int messagefieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void messagefieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  // todo(jonp): in the future, consider having a method specific to the
  // interface so that builders can choose dynamically to either return a
  // message or a nested builder, so that asking for the interface doesn't
  // cause a message to ever be built.
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$boolean has$capitalized_name$();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$();\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$$type$orbuilder get$capitalized_name$orbuilder();\n");
  }
}

void messagefieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private $type$ $name$_;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public boolean has$capitalized_name$() {\n"
    "  return $get_has_field_bit_message$;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public $type$ get$capitalized_name$() {\n"
    "  return $name$_;\n"
    "}\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$public $type$orbuilder get$capitalized_name$orbuilder() {\n"
      "  return $name$_;\n"
      "}\n");
  }
}

void messagefieldgenerator::printnestedbuildercondition(
    io::printer* printer,
    const char* regular_case,
    const char* nested_builder_case) const {
  if (hasnestedbuilders(descriptor_->containing_type())) {
     printer->print(variables_, "if ($name$builder_ == null) {\n");
     printer->indent();
     printer->print(variables_, regular_case);
     printer->outdent();
     printer->print("} else {\n");
     printer->indent();
     printer->print(variables_, nested_builder_case);
     printer->outdent();
     printer->print("}\n");
   } else {
     printer->print(variables_, regular_case);
   }
}

void messagefieldgenerator::printnestedbuilderfunction(
    io::printer* printer,
    const char* method_prototype,
    const char* regular_case,
    const char* nested_builder_case,
    const char* trailing_code) const {
  printer->print(variables_, method_prototype);
  printer->print(" {\n");
  printer->indent();
  printnestedbuildercondition(printer, regular_case, nested_builder_case);
  if (trailing_code != null) {
    printer->print(variables_, trailing_code);
  }
  printer->outdent();
  printer->print("}\n");
}

void messagefieldgenerator::
generatebuildermembers(io::printer* printer) const {
  // when using nested-builders, the code initially works just like the
  // non-nested builder case. it only creates a nested builder lazily on
  // demand and then forever delegates to it after creation.

  printer->print(variables_,
    // used when the builder is null.
    "private $type$ $name$_ = $type$.getdefaultinstance();\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    printer->print(variables_,
      // if this builder is non-null, it is used and the other fields are
      // ignored.
      "private com.google.protobuf.singlefieldbuilder<\n"
      "    $type$, $type$.builder, $type$orbuilder> $name$builder_;"
      "\n");
  }

  // the comments above the methods below are based on a hypothetical
  // field of type "field" called "field".

  // boolean hasfield()
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public boolean has$capitalized_name$() {\n"
    "  return $get_has_field_bit_builder$;\n"
    "}\n");

  // field getfield()
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public $type$ get$capitalized_name$()",

    "return $name$_;\n",

    "return $name$builder_.getmessage();\n",

    null);

  // field.builder setfield(field value)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder set$capitalized_name$($type$ value)",

    "if (value == null) {\n"
    "  throw new nullpointerexception();\n"
    "}\n"
    "$name$_ = value;\n"
    "$on_changed$\n",

    "$name$builder_.setmessage(value);\n",

    "$set_has_field_bit_builder$;\n"
    "return this;\n");

  // field.builder setfield(field.builder builderforvalue)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    $type$.builder builderforvalue)",

    "$name$_ = builderforvalue.build();\n"
    "$on_changed$\n",

    "$name$builder_.setmessage(builderforvalue.build());\n",

    "$set_has_field_bit_builder$;\n"
    "return this;\n");

  // field.builder mergefield(field value)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder merge$capitalized_name$($type$ value)",

    "if ($get_has_field_bit_builder$ &&\n"
    "    $name$_ != $type$.getdefaultinstance()) {\n"
    "  $name$_ =\n"
    "    $type$.newbuilder($name$_).mergefrom(value).buildpartial();\n"
    "} else {\n"
    "  $name$_ = value;\n"
    "}\n"
    "$on_changed$\n",

    "$name$builder_.mergefrom(value);\n",

    "$set_has_field_bit_builder$;\n"
    "return this;\n");

  // field.builder clearfield()
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder clear$capitalized_name$()",

    "$name$_ = $type$.getdefaultinstance();\n"
    "$on_changed$\n",

    "$name$builder_.clear();\n",

    "$clear_has_field_bit_builder$;\n"
    "return this;\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$public $type$.builder get$capitalized_name$builder() {\n"
      "  $set_has_field_bit_builder$;\n"
      "  $on_changed$\n"
      "  return get$capitalized_name$fieldbuilder().getbuilder();\n"
      "}\n");
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$public $type$orbuilder get$capitalized_name$orbuilder() {\n"
      "  if ($name$builder_ != null) {\n"
      "    return $name$builder_.getmessageorbuilder();\n"
      "  } else {\n"
      "    return $name$_;\n"
      "  }\n"
      "}\n");
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "private com.google.protobuf.singlefieldbuilder<\n"
      "    $type$, $type$.builder, $type$orbuilder> \n"
      "    get$capitalized_name$fieldbuilder() {\n"
      "  if ($name$builder_ == null) {\n"
      "    $name$builder_ = new com.google.protobuf.singlefieldbuilder<\n"
      "        $type$, $type$.builder, $type$orbuilder>(\n"
      "            $name$_,\n"
      "            getparentforchildren(),\n"
      "            isclean());\n"
      "    $name$_ = null;\n"
      "  }\n"
      "  return $name$builder_;\n"
      "}\n");
  }
}

void messagefieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  printer->print(variables_,
    "get$capitalized_name$fieldbuilder();\n");
}


void messagefieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $type$.getdefaultinstance();\n");
}

void messagefieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printnestedbuildercondition(printer,
    "$name$_ = $type$.getdefaultinstance();\n",

    "$name$builder_.clear();\n");
  printer->print(variables_, "$clear_has_field_bit_builder$;\n");
}

void messagefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (other.has$capitalized_name$()) {\n"
    "  merge$capitalized_name$(other.get$capitalized_name$());\n"
    "}\n");
}

void messagefieldgenerator::
generatebuildingcode(io::printer* printer) const {

  printer->print(variables_,
      "if ($get_has_field_bit_from_local$) {\n"
      "  $set_has_field_bit_to_local$;\n"
      "}\n");

  printnestedbuildercondition(printer,
    "result.$name$_ = $name$_;\n",

    "result.$name$_ = $name$builder_.build();\n");
}

void messagefieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "$type$.builder subbuilder = null;\n"
    "if ($get_has_field_bit_message$) {\n"
    "  subbuilder = $name$_.tobuilder();\n"
    "}\n");

  if (gettype(descriptor_) == fielddescriptor::type_group) {
    printer->print(variables_,
      "$name$_ = input.readgroup($number$, $type$.parser,\n"
      "    extensionregistry);\n");
  } else {
    printer->print(variables_,
      "$name$_ = input.readmessage($type$.parser, extensionregistry);\n");
  }

  printer->print(variables_,
    "if (subbuilder != null) {\n"
    "  subbuilder.mergefrom($name$_);\n"
    "  $name$_ = subbuilder.buildpartial();\n"
    "}\n");
  printer->print(variables_,
    "$set_has_field_bit_message$;\n");
}

void messagefieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  // noop for messages.
}

void messagefieldgenerator::
generateserializationcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  output.write$group_or_message$($number$, $name$_);\n"
    "}\n");
}

void messagefieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  size += com.google.protobuf.codedoutputstream\n"
    "    .compute$group_or_message$size($number$, $name$_);\n"
    "}\n");
}

void messagefieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$()\n"
    "    .equals(other.get$capitalized_name$());\n");
}

void messagefieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "hash = (37 * hash) + $constant_name$;\n"
    "hash = (53 * hash) + get$capitalized_name$().hashcode();\n");
}

string messagefieldgenerator::getboxedtype() const {
  return classname(descriptor_->message_type());
}

// ===================================================================

repeatedmessagefieldgenerator::
repeatedmessagefieldgenerator(const fielddescriptor* descriptor,
                              int messagebitindex,
                              int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setmessagevariables(descriptor, messagebitindex, builderbitindex,
                      &variables_);
}

repeatedmessagefieldgenerator::~repeatedmessagefieldgenerator() {}

int repeatedmessagefieldgenerator::getnumbitsformessage() const {
  return 0;
}

int repeatedmessagefieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void repeatedmessagefieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  // todo(jonp): in the future, consider having methods specific to the
  // interface so that builders can choose dynamically to either return a
  // message or a nested builder, so that asking for the interface doesn't
  // cause a message to ever be built.
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.util.list<$type$> \n"
    "    get$capitalized_name$list();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$(int index);\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$int get$capitalized_name$count();\n");
  if (hasnestedbuilders(descriptor_->containing_type())) {
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$java.util.list<? extends $type$orbuilder> \n"
      "    get$capitalized_name$orbuilderlist();\n");
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$$type$orbuilder get$capitalized_name$orbuilder(\n"
      "    int index);\n");
  }
}

void repeatedmessagefieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private java.util.list<$type$> $name$_;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<$type$> get$capitalized_name$list() {\n"
    "  return $name$_;\n"   // note:  unmodifiable list
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<? extends $type$orbuilder> \n"
    "    get$capitalized_name$orbuilderlist() {\n"
    "  return $name$_;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public int get$capitalized_name$count() {\n"
    "  return $name$_.size();\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public $type$ get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public $type$orbuilder get$capitalized_name$orbuilder(\n"
    "    int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");

}

void repeatedmessagefieldgenerator::printnestedbuildercondition(
    io::printer* printer,
    const char* regular_case,
    const char* nested_builder_case) const {
  if (hasnestedbuilders(descriptor_->containing_type())) {
     printer->print(variables_, "if ($name$builder_ == null) {\n");
     printer->indent();
     printer->print(variables_, regular_case);
     printer->outdent();
     printer->print("} else {\n");
     printer->indent();
     printer->print(variables_, nested_builder_case);
     printer->outdent();
     printer->print("}\n");
   } else {
     printer->print(variables_, regular_case);
   }
}

void repeatedmessagefieldgenerator::printnestedbuilderfunction(
    io::printer* printer,
    const char* method_prototype,
    const char* regular_case,
    const char* nested_builder_case,
    const char* trailing_code) const {
  printer->print(variables_, method_prototype);
  printer->print(" {\n");
  printer->indent();
  printnestedbuildercondition(printer, regular_case, nested_builder_case);
  if (trailing_code != null) {
    printer->print(variables_, trailing_code);
  }
  printer->outdent();
  printer->print("}\n");
}

void repeatedmessagefieldgenerator::
generatebuildermembers(io::printer* printer) const {
  // when using nested-builders, the code initially works just like the
  // non-nested builder case. it only creates a nested builder lazily on
  // demand and then forever delegates to it after creation.

  printer->print(variables_,
    // used when the builder is null.
    // one field is the list and the other field keeps track of whether the
    // list is immutable. if it's immutable, the invariant is that it must
    // either an instance of collections.emptylist() or it's an arraylist
    // wrapped in a collections.unmodifiablelist() wrapper and nobody else has
    // a refererence to the underlying arraylist. this invariant allows us to
    // share instances of lists between protocol buffers avoiding expensive
    // memory allocations. note, immutable is a strong guarantee here -- not
    // just that the list cannot be modified via the reference but that the
    // list can never be modified.
    "private java.util.list<$type$> $name$_ =\n"
    "  java.util.collections.emptylist();\n"

    "private void ensure$capitalized_name$ismutable() {\n"
    "  if (!$get_mutable_bit_builder$) {\n"
    "    $name$_ = new java.util.arraylist<$type$>($name$_);\n"
    "    $set_mutable_bit_builder$;\n"
    "   }\n"
    "}\n"
    "\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    printer->print(variables_,
      // if this builder is non-null, it is used and the other fields are
      // ignored.
      "private com.google.protobuf.repeatedfieldbuilder<\n"
      "    $type$, $type$.builder, $type$orbuilder> $name$builder_;\n"
      "\n");
  }

  // the comments above the methods below are based on a hypothetical
  // repeated field of type "field" called "repeatedfield".

  // list<field> getrepeatedfieldlist()
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public java.util.list<$type$> get$capitalized_name$list()",

    "return java.util.collections.unmodifiablelist($name$_);\n",
    "return $name$builder_.getmessagelist();\n",

    null);

  // int getrepeatedfieldcount()
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public int get$capitalized_name$count()",

    "return $name$_.size();\n",
    "return $name$builder_.getcount();\n",

    null);

  // field getrepeatedfield(int index)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public $type$ get$capitalized_name$(int index)",

    "return $name$_.get(index);\n",

    "return $name$builder_.getmessage(index);\n",

    null);

  // builder setrepeatedfield(int index, field value)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    int index, $type$ value)",
    "if (value == null) {\n"
    "  throw new nullpointerexception();\n"
    "}\n"
    "ensure$capitalized_name$ismutable();\n"
    "$name$_.set(index, value);\n"
    "$on_changed$\n",
    "$name$builder_.setmessage(index, value);\n",
    "return this;\n");

  // builder setrepeatedfield(int index, field.builder builderforvalue)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    int index, $type$.builder builderforvalue)",

    "ensure$capitalized_name$ismutable();\n"
    "$name$_.set(index, builderforvalue.build());\n"
    "$on_changed$\n",

    "$name$builder_.setmessage(index, builderforvalue.build());\n",

    "return this;\n");

  // builder addrepeatedfield(field value)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder add$capitalized_name$($type$ value)",

    "if (value == null) {\n"
    "  throw new nullpointerexception();\n"
    "}\n"
    "ensure$capitalized_name$ismutable();\n"
    "$name$_.add(value);\n"

    "$on_changed$\n",

    "$name$builder_.addmessage(value);\n",

    "return this;\n");

  // builder addrepeatedfield(int index, field value)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder add$capitalized_name$(\n"
    "    int index, $type$ value)",

    "if (value == null) {\n"
    "  throw new nullpointerexception();\n"
    "}\n"
    "ensure$capitalized_name$ismutable();\n"
    "$name$_.add(index, value);\n"
    "$on_changed$\n",

    "$name$builder_.addmessage(index, value);\n",

    "return this;\n");

  // builder addrepeatedfield(field.builder builderforvalue)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder add$capitalized_name$(\n"
    "    $type$.builder builderforvalue)",

    "ensure$capitalized_name$ismutable();\n"
    "$name$_.add(builderforvalue.build());\n"
    "$on_changed$\n",

    "$name$builder_.addmessage(builderforvalue.build());\n",

    "return this;\n");

  // builder addrepeatedfield(int index, field.builder builderforvalue)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder add$capitalized_name$(\n"
    "    int index, $type$.builder builderforvalue)",

    "ensure$capitalized_name$ismutable();\n"
    "$name$_.add(index, builderforvalue.build());\n"
    "$on_changed$\n",

    "$name$builder_.addmessage(index, builderforvalue.build());\n",

    "return this;\n");

  // builder addallrepeatedfield(iterable<field> values)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder addall$capitalized_name$(\n"
    "    java.lang.iterable<? extends $type$> values)",

    "ensure$capitalized_name$ismutable();\n"
    "super.addall(values, $name$_);\n"
    "$on_changed$\n",

    "$name$builder_.addallmessages(values);\n",

    "return this;\n");

  // builder clearallrepeatedfield()
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder clear$capitalized_name$()",

    "$name$_ = java.util.collections.emptylist();\n"
    "$clear_mutable_bit_builder$;\n"
    "$on_changed$\n",

    "$name$builder_.clear();\n",

    "return this;\n");

  // builder removerepeatedfield(int index)
  writefielddoccomment(printer, descriptor_);
  printnestedbuilderfunction(printer,
    "$deprecation$public builder remove$capitalized_name$(int index)",

    "ensure$capitalized_name$ismutable();\n"
    "$name$_.remove(index);\n"
    "$on_changed$\n",

    "$name$builder_.remove(index);\n",

    "return this;\n");

  if (hasnestedbuilders(descriptor_->containing_type())) {
    writefielddoccomment(printer, descriptor_);
    printer->print(variables_,
      "$deprecation$public $type$.builder get$capitalized_name$builder(\n"
      "    int index) {\n"
      "  return get$capitalized_name$fieldbuilder().getbuilder(index);\n"
      "}\n");

    writefielddoccomment(printer, descriptor_);
        printer->print(variables_,
      "$deprecation$public $type$orbuilder get$capitalized_name$orbuilder(\n"
      "    int index) {\n"
      "  if ($name$builder_ == null) {\n"
      "    return $name$_.get(index);"
      "  } else {\n"
      "    return $name$builder_.getmessageorbuilder(index);\n"
      "  }\n"
      "}\n");

    writefielddoccomment(printer, descriptor_);
        printer->print(variables_,
      "$deprecation$public java.util.list<? extends $type$orbuilder> \n"
      "     get$capitalized_name$orbuilderlist() {\n"
      "  if ($name$builder_ != null) {\n"
      "    return $name$builder_.getmessageorbuilderlist();\n"
      "  } else {\n"
      "    return java.util.collections.unmodifiablelist($name$_);\n"
      "  }\n"
      "}\n");

    writefielddoccomment(printer, descriptor_);
        printer->print(variables_,
      "$deprecation$public $type$.builder add$capitalized_name$builder() {\n"
      "  return get$capitalized_name$fieldbuilder().addbuilder(\n"
      "      $type$.getdefaultinstance());\n"
      "}\n");
    writefielddoccomment(printer, descriptor_);
        printer->print(variables_,
      "$deprecation$public $type$.builder add$capitalized_name$builder(\n"
      "    int index) {\n"
      "  return get$capitalized_name$fieldbuilder().addbuilder(\n"
      "      index, $type$.getdefaultinstance());\n"
      "}\n");
    writefielddoccomment(printer, descriptor_);
        printer->print(variables_,
      "$deprecation$public java.util.list<$type$.builder> \n"
      "     get$capitalized_name$builderlist() {\n"
      "  return get$capitalized_name$fieldbuilder().getbuilderlist();\n"
      "}\n"
      "private com.google.protobuf.repeatedfieldbuilder<\n"
      "    $type$, $type$.builder, $type$orbuilder> \n"
      "    get$capitalized_name$fieldbuilder() {\n"
      "  if ($name$builder_ == null) {\n"
      "    $name$builder_ = new com.google.protobuf.repeatedfieldbuilder<\n"
      "        $type$, $type$.builder, $type$orbuilder>(\n"
      "            $name$_,\n"
      "            $get_mutable_bit_builder$,\n"
      "            getparentforchildren(),\n"
      "            isclean());\n"
      "    $name$_ = null;\n"
      "  }\n"
      "  return $name$builder_;\n"
      "}\n");
  }
}

void repeatedmessagefieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  printer->print(variables_,
    "get$capitalized_name$fieldbuilder();\n");
}

void repeatedmessagefieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = java.util.collections.emptylist();\n");
}

void repeatedmessagefieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printnestedbuildercondition(printer,
    "$name$_ = java.util.collections.emptylist();\n"
    "$clear_mutable_bit_builder$;\n",

    "$name$builder_.clear();\n");
}

void repeatedmessagefieldgenerator::
generatemergingcode(io::printer* printer) const {
  // the code below does two optimizations (non-nested builder case):
  //   1. if the other list is empty, there's nothing to do. this ensures we
  //      don't allocate a new array if we already have an immutable one.
  //   2. if the other list is non-empty and our current list is empty, we can
  //      reuse the other list which is guaranteed to be immutable.
  printnestedbuildercondition(printer,
    "if (!other.$name$_.isempty()) {\n"
    "  if ($name$_.isempty()) {\n"
    "    $name$_ = other.$name$_;\n"
    "    $clear_mutable_bit_builder$;\n"
    "  } else {\n"
    "    ensure$capitalized_name$ismutable();\n"
    "    $name$_.addall(other.$name$_);\n"
    "  }\n"
    "  $on_changed$\n"
    "}\n",

    "if (!other.$name$_.isempty()) {\n"
    "  if ($name$builder_.isempty()) {\n"
    "    $name$builder_.dispose();\n"
    "    $name$builder_ = null;\n"
    "    $name$_ = other.$name$_;\n"
    "    $clear_mutable_bit_builder$;\n"
    "    $name$builder_ = \n"
    "      com.google.protobuf.generatedmessage.alwaysusefieldbuilders ?\n"
    "         get$capitalized_name$fieldbuilder() : null;\n"
    "  } else {\n"
    "    $name$builder_.addallmessages(other.$name$_);\n"
    "  }\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generatebuildingcode(io::printer* printer) const {
  // the code below (non-nested builder case) ensures that the result has an
  // immutable list. if our list is immutable, we can just reuse it. if not,
  // we make it immutable.
  printnestedbuildercondition(printer,
    "if ($get_mutable_bit_builder$) {\n"
    "  $name$_ = java.util.collections.unmodifiablelist($name$_);\n"
    "  $clear_mutable_bit_builder$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n",

    "result.$name$_ = $name$builder_.build();\n");
}

void repeatedmessagefieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (!$get_mutable_bit_parser$) {\n"
    "  $name$_ = new java.util.arraylist<$type$>();\n"
    "  $set_mutable_bit_parser$;\n"
    "}\n");

  if (gettype(descriptor_) == fielddescriptor::type_group) {
    printer->print(variables_,
      "$name$_.add(input.readgroup($number$, $type$.parser,\n"
      "    extensionregistry));\n");
  } else {
    printer->print(variables_,
      "$name$_.add(input.readmessage($type$.parser, extensionregistry));\n");
  }
}

void repeatedmessagefieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_mutable_bit_parser$) {\n"
    "  $name$_ = java.util.collections.unmodifiablelist($name$_);\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generateserializationcode(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < $name$_.size(); i++) {\n"
    "  output.write$group_or_message$($number$, $name$_.get(i));\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "for (int i = 0; i < $name$_.size(); i++) {\n"
    "  size += com.google.protobuf.codedoutputstream\n"
    "    .compute$group_or_message$size($number$, $name$_.get(i));\n"
    "}\n");
}

void repeatedmessagefieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$list()\n"
    "    .equals(other.get$capitalized_name$list());\n");
}

void repeatedmessagefieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "if (get$capitalized_name$count() > 0) {\n"
    "  hash = (37 * hash) + $constant_name$;\n"
    "  hash = (53 * hash) + get$capitalized_name$list().hashcode();\n"
    "}\n");
}

string repeatedmessagefieldgenerator::getboxedtype() const {
  return classname(descriptor_->message_type());
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
