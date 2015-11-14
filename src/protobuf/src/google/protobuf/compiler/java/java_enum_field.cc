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

#include <google/protobuf/compiler/java/java_enum_field.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/stubs/common.h>
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
void setenumvariables(const fielddescriptor* descriptor,
                      int messagebitindex,
                      int builderbitindex,
                      map<string, string>* variables) {
  (*variables)["name"] =
    underscorestocamelcase(descriptor);
  (*variables)["capitalized_name"] =
    underscorestocapitalizedcamelcase(descriptor);
  (*variables)["constant_name"] = fieldconstantname(descriptor);
  (*variables)["number"] = simpleitoa(descriptor->number());
  (*variables)["type"] = classname(descriptor->enum_type());
  (*variables)["default"] = defaultvalue(descriptor);
  (*variables)["tag"] = simpleitoa(internal::wireformat::maketag(descriptor));
  (*variables)["tag_size"] = simpleitoa(
      internal::wireformat::tagsize(descriptor->number(), gettype(descriptor)));
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

enumfieldgenerator::
enumfieldgenerator(const fielddescriptor* descriptor,
                      int messagebitindex,
                      int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setenumvariables(descriptor, messagebitindex, builderbitindex, &variables_);
}

enumfieldgenerator::~enumfieldgenerator() {}

int enumfieldgenerator::getnumbitsformessage() const {
  return 1;
}

int enumfieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void enumfieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$boolean has$capitalized_name$();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$();\n");
}

void enumfieldgenerator::
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
}

void enumfieldgenerator::
generatebuildermembers(io::printer* printer) const {
  printer->print(variables_,
    "private $type$ $name$_ = $default$;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public boolean has$capitalized_name$() {\n"
    "  return $get_has_field_bit_builder$;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public $type$ get$capitalized_name$() {\n"
    "  return $name$_;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder set$capitalized_name$($type$ value) {\n"
    "  if (value == null) {\n"
    "    throw new nullpointerexception();\n"
    "  }\n"
    "  $set_has_field_bit_builder$;\n"
    "  $name$_ = value;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder clear$capitalized_name$() {\n"
    "  $clear_has_field_bit_builder$;\n"
    "  $name$_ = $default$;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
}

void enumfieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for enums
}

void enumfieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void enumfieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
      "$name$_ = $default$;\n"
      "$clear_has_field_bit_builder$;\n");
}

void enumfieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (other.has$capitalized_name$()) {\n"
    "  set$capitalized_name$(other.get$capitalized_name$());\n"
    "}\n");
}

void enumfieldgenerator::
generatebuildingcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_from_local$) {\n"
    "  $set_has_field_bit_to_local$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n");
}

void enumfieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "int rawvalue = input.readenum();\n"
    "$type$ value = $type$.valueof(rawvalue);\n");
  if (hasunknownfields(descriptor_->containing_type())) {
    printer->print(variables_,
      "if (value == null) {\n"
      "  unknownfields.mergevarintfield($number$, rawvalue);\n"
      "} else {\n");
  } else {
    printer->print(variables_,
      "if (value != null) {\n");
  }
  printer->print(variables_,
    "  $set_has_field_bit_message$;\n"
    "  $name$_ = value;\n"
    "}\n");
}

void enumfieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  // noop for enums
}

void enumfieldgenerator::
generateserializationcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  output.writeenum($number$, $name$_.getnumber());\n"
    "}\n");
}

void enumfieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  size += com.google.protobuf.codedoutputstream\n"
    "    .computeenumsize($number$, $name$_.getnumber());\n"
    "}\n");
}

void enumfieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result &&\n"
    "    (get$capitalized_name$() == other.get$capitalized_name$());\n");
}

void enumfieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "hash = (37 * hash) + $constant_name$;\n"
    "hash = (53 * hash) + hashenum(get$capitalized_name$());\n");
}

string enumfieldgenerator::getboxedtype() const {
  return classname(descriptor_->enum_type());
}

// ===================================================================

repeatedenumfieldgenerator::
repeatedenumfieldgenerator(const fielddescriptor* descriptor,
                           int messagebitindex,
                           int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setenumvariables(descriptor, messagebitindex, builderbitindex, &variables_);
}

repeatedenumfieldgenerator::~repeatedenumfieldgenerator() {}

int repeatedenumfieldgenerator::getnumbitsformessage() const {
  return 0;
}

int repeatedenumfieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void repeatedenumfieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.util.list<$type$> get$capitalized_name$list();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$int get$capitalized_name$count();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$(int index);\n");
}

void repeatedenumfieldgenerator::
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
    "$deprecation$public int get$capitalized_name$count() {\n"
    "  return $name$_.size();\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public $type$ get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");

  if (descriptor_->options().packed() &&
      hasgeneratedmethods(descriptor_->containing_type())) {
    printer->print(variables_,
      "private int $name$memoizedserializedsize;\n");
  }
}

void repeatedenumfieldgenerator::
generatebuildermembers(io::printer* printer) const {
  printer->print(variables_,
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
    "  }\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    // note:  we return an unmodifiable list because otherwise the caller
    //   could hold on to the returned list and modify it after the message
    //   has been built, thus mutating the message which is supposed to be
    //   immutable.
    "$deprecation$public java.util.list<$type$> get$capitalized_name$list() {\n"
    "  return java.util.collections.unmodifiablelist($name$_);\n"
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
    "$deprecation$public builder set$capitalized_name$(\n"
    "    int index, $type$ value) {\n"
    "  if (value == null) {\n"
    "    throw new nullpointerexception();\n"
    "  }\n"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.set(index, value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder add$capitalized_name$($type$ value) {\n"
    "  if (value == null) {\n"
    "    throw new nullpointerexception();\n"
    "  }\n"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.add(value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder addall$capitalized_name$(\n"
    "    java.lang.iterable<? extends $type$> values) {\n"
    "  ensure$capitalized_name$ismutable();\n"
    "  super.addall(values, $name$_);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder clear$capitalized_name$() {\n"
    "  $name$_ = java.util.collections.emptylist();\n"
    "  $clear_mutable_bit_builder$;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
}

void repeatedenumfieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for enums
}

void repeatedenumfieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = java.util.collections.emptylist();\n");
}

void repeatedenumfieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = java.util.collections.emptylist();\n"
    "$clear_mutable_bit_builder$;\n");
}

void repeatedenumfieldgenerator::
generatemergingcode(io::printer* printer) const {
  // the code below does two optimizations:
  //   1. if the other list is empty, there's nothing to do. this ensures we
  //      don't allocate a new array if we already have an immutable one.
  //   2. if the other list is non-empty and our current list is empty, we can
  //      reuse the other list which is guaranteed to be immutable.
  printer->print(variables_,
    "if (!other.$name$_.isempty()) {\n"
    "  if ($name$_.isempty()) {\n"
    "    $name$_ = other.$name$_;\n"
    "    $clear_mutable_bit_builder$;\n"
    "  } else {\n"
    "    ensure$capitalized_name$ismutable();\n"
    "    $name$_.addall(other.$name$_);\n"
    "  }\n"
    "  $on_changed$\n"
    "}\n");
}

void repeatedenumfieldgenerator::
generatebuildingcode(io::printer* printer) const {
  // the code below ensures that the result has an immutable list. if our
  // list is immutable, we can just reuse it. if not, we make it immutable.
  printer->print(variables_,
    "if ($get_mutable_bit_builder$) {\n"
    "  $name$_ = java.util.collections.unmodifiablelist($name$_);\n"
    "  $clear_mutable_bit_builder$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n");
}

void repeatedenumfieldgenerator::
generateparsingcode(io::printer* printer) const {
  // read and store the enum
  printer->print(variables_,
    "int rawvalue = input.readenum();\n"
    "$type$ value = $type$.valueof(rawvalue);\n");
  if (hasunknownfields(descriptor_->containing_type())) {
    printer->print(variables_,
      "if (value == null) {\n"
      "  unknownfields.mergevarintfield($number$, rawvalue);\n"
      "} else {\n");
  } else {
    printer->print(variables_,
      "if (value != null) {\n");
  }
  printer->print(variables_,
    "  if (!$get_mutable_bit_parser$) {\n"
    "    $name$_ = new java.util.arraylist<$type$>();\n"
    "    $set_mutable_bit_parser$;\n"
    "  }\n"
    "  $name$_.add(value);\n"
    "}\n");
}

void repeatedenumfieldgenerator::
generateparsingcodefrompacked(io::printer* printer) const {
  // wrap generateparsingcode's contents with a while loop.

  printer->print(variables_,
    "int length = input.readrawvarint32();\n"
    "int oldlimit = input.pushlimit(length);\n"
    "while(input.getbytesuntillimit() > 0) {\n");
  printer->indent();

  generateparsingcode(printer);

  printer->outdent();
  printer->print(variables_,
    "}\n"
    "input.poplimit(oldlimit);\n");
}

void repeatedenumfieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_mutable_bit_parser$) {\n"
    "  $name$_ = java.util.collections.unmodifiablelist($name$_);\n"
    "}\n");
}

void repeatedenumfieldgenerator::
generateserializationcode(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "if (get$capitalized_name$list().size() > 0) {\n"
      "  output.writerawvarint32($tag$);\n"
      "  output.writerawvarint32($name$memoizedserializedsize);\n"
      "}\n"
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  output.writeenumnotag($name$_.get(i).getnumber());\n"
      "}\n");
  } else {
    printer->print(variables_,
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  output.writeenum($number$, $name$_.get(i).getnumber());\n"
      "}\n");
  }
}

void repeatedenumfieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "{\n"
    "  int datasize = 0;\n");
  printer->indent();

  printer->print(variables_,
    "for (int i = 0; i < $name$_.size(); i++) {\n"
    "  datasize += com.google.protobuf.codedoutputstream\n"
    "    .computeenumsizenotag($name$_.get(i).getnumber());\n"
    "}\n");
  printer->print(
    "size += datasize;\n");
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "if (!get$capitalized_name$list().isempty()) {"
      "  size += $tag_size$;\n"
      "  size += com.google.protobuf.codedoutputstream\n"
      "    .computerawvarint32size(datasize);\n"
      "}");
  } else {
    printer->print(variables_,
      "size += $tag_size$ * $name$_.size();\n");
  }

  // cache the data size for packed fields.
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "$name$memoizedserializedsize = datasize;\n");
  }

  printer->outdent();
  printer->print("}\n");
}

void repeatedenumfieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$list()\n"
    "    .equals(other.get$capitalized_name$list());\n");
}

void repeatedenumfieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "if (get$capitalized_name$count() > 0) {\n"
    "  hash = (37 * hash) + $constant_name$;\n"
    "  hash = (53 * hash) + hashenumlist(get$capitalized_name$list());\n"
    "}\n");
}

string repeatedenumfieldgenerator::getboxedtype() const {
  return classname(descriptor_->enum_type());
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
