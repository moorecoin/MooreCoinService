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
// author: jonp@google.com (jon perlow)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <map>
#include <string>

#include <google/protobuf/compiler/java/java_string_field.h>
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

using internal::wireformat;
using internal::wireformatlite;

namespace {

void setprimitivevariables(const fielddescriptor* descriptor,
                           int messagebitindex,
                           int builderbitindex,
                           map<string, string>* variables) {
  (*variables)["name"] =
    underscorestocamelcase(descriptor);
  (*variables)["capitalized_name"] =
    underscorestocapitalizedcamelcase(descriptor);
  (*variables)["constant_name"] = fieldconstantname(descriptor);
  (*variables)["number"] = simpleitoa(descriptor->number());
  (*variables)["empty_list"] = "com.google.protobuf.lazystringarraylist.empty";

  (*variables)["default"] = defaultvalue(descriptor);
  (*variables)["default_init"] = ("= " + defaultvalue(descriptor));
  (*variables)["capitalized_type"] = "string";
  (*variables)["tag"] = simpleitoa(wireformat::maketag(descriptor));
  (*variables)["tag_size"] = simpleitoa(
      wireformat::tagsize(descriptor->number(), gettype(descriptor)));
  (*variables)["null_check"] =
      "  if (value == null) {\n"
      "    throw new nullpointerexception();\n"
      "  }\n";

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

stringfieldgenerator::
stringfieldgenerator(const fielddescriptor* descriptor,
                     int messagebitindex,
                     int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setprimitivevariables(descriptor, messagebitindex, builderbitindex,
                        &variables_);
}

stringfieldgenerator::~stringfieldgenerator() {}

int stringfieldgenerator::getnumbitsformessage() const {
  return 1;
}

int stringfieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

// a note about how strings are handled. this code used to just store a string
// in the message. this had two issues:
//
//  1. it wouldn't roundtrip byte arrays that were not vaid utf-8 encoded
//     strings, but rather fields that were raw bytes incorrectly marked
//     as strings in the proto file. this is common because in the proto1
//     syntax, string was the way to indicate bytes and c++ engineers can
//     easily make this mistake without affecting the c++ api. by converting to
//     strings immediately, some java code might corrupt these byte arrays as
//     it passes through a java server even if the field was never accessed by
//     application code.
//
//  2. there's a performance hit to converting between bytes and strings and
//     it many cases, the field is never even read by the application code. this
//     avoids unnecessary conversions in the common use cases.
//
// so now, the field for string is maintained as an object reference which can
// either store a string or a bytestring. the code uses an instanceof check
// to see which one it has and converts to the other one if needed. it remembers
// the last value requested (in a thread safe manner) as this is most likely
// the one needed next. the thread safety is such that if two threads both
// convert the field because the changes made by each thread were not visible to
// the other, they may cause a conversion to happen more times than would
// otherwise be necessary. this was deemed better than adding synchronization
// overhead. it will not cause any corruption issues or affect the behavior of
// the api. the instanceof check is also highly optimized in the jvm and we
// decided it was better to reduce the memory overhead by not having two
// separate fields but rather use dynamic type checking.
//
// for single fields, the logic for this is done inside the generated code. for
// repeated fields, the logic is done in lazystringarraylist and
// unmodifiablelazystringlist.
void stringfieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$boolean has$capitalized_name$();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.lang.string get$capitalized_name$();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes();\n");
}

void stringfieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private java.lang.object $name$_;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public boolean has$capitalized_name$() {\n"
    "  return $get_has_field_bit_message$;\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.lang.string get$capitalized_name$() {\n"
    "  java.lang.object ref = $name$_;\n"
    "  if (ref instanceof java.lang.string) {\n"
    "    return (java.lang.string) ref;\n"
    "  } else {\n"
    "    com.google.protobuf.bytestring bs = \n"
    "        (com.google.protobuf.bytestring) ref;\n"
    "    java.lang.string s = bs.tostringutf8();\n"
    "    if (bs.isvalidutf8()) {\n"
    "      $name$_ = s;\n"
    "    }\n"
    "    return s;\n"
    "  }\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes() {\n"
    "  java.lang.object ref = $name$_;\n"
    "  if (ref instanceof java.lang.string) {\n"
    "    com.google.protobuf.bytestring b = \n"
    "        com.google.protobuf.bytestring.copyfromutf8(\n"
    "            (java.lang.string) ref);\n"
    "    $name$_ = b;\n"
    "    return b;\n"
    "  } else {\n"
    "    return (com.google.protobuf.bytestring) ref;\n"
    "  }\n"
    "}\n");
}

void stringfieldgenerator::
generatebuildermembers(io::printer* printer) const {
  printer->print(variables_,
    "private java.lang.object $name$_ $default_init$;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public boolean has$capitalized_name$() {\n"
    "  return $get_has_field_bit_builder$;\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.lang.string get$capitalized_name$() {\n"
    "  java.lang.object ref = $name$_;\n"
    "  if (!(ref instanceof java.lang.string)) {\n"
    "    java.lang.string s = ((com.google.protobuf.bytestring) ref)\n"
    "        .tostringutf8();\n"
    "    $name$_ = s;\n"
    "    return s;\n"
    "  } else {\n"
    "    return (java.lang.string) ref;\n"
    "  }\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes() {\n"
    "  java.lang.object ref = $name$_;\n"
    "  if (ref instanceof string) {\n"
    "    com.google.protobuf.bytestring b = \n"
    "        com.google.protobuf.bytestring.copyfromutf8(\n"
    "            (java.lang.string) ref);\n"
    "    $name$_ = b;\n"
    "    return b;\n"
    "  } else {\n"
    "    return (com.google.protobuf.bytestring) ref;\n"
    "  }\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    java.lang.string value) {\n"
    "$null_check$"
    "  $set_has_field_bit_builder$;\n"
    "  $name$_ = value;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder clear$capitalized_name$() {\n"
    "  $clear_has_field_bit_builder$;\n");
  // the default value is not a simple literal so we want to avoid executing
  // it multiple times.  instead, get the default out of the default instance.
  printer->print(variables_,
    "  $name$_ = getdefaultinstance().get$capitalized_name$();\n");
  printer->print(variables_,
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder set$capitalized_name$bytes(\n"
    "    com.google.protobuf.bytestring value) {\n"
    "$null_check$"
    "  $set_has_field_bit_builder$;\n"
    "  $name$_ = value;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
}

void stringfieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for primitives
}

void stringfieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void stringfieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = $default$;\n"
    "$clear_has_field_bit_builder$;\n");
}

void stringfieldgenerator::
generatemergingcode(io::printer* printer) const {
  // allow a slight breach of abstraction here in order to avoid forcing
  // all string fields to strings when copying fields from a message.
  printer->print(variables_,
    "if (other.has$capitalized_name$()) {\n"
    "  $set_has_field_bit_builder$;\n"
    "  $name$_ = other.$name$_;\n"
    "  $on_changed$\n"
    "}\n");
}

void stringfieldgenerator::
generatebuildingcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_from_local$) {\n"
    "  $set_has_field_bit_to_local$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n");
}

void stringfieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "$set_has_field_bit_message$;\n"
    "$name$_ = input.readbytes();\n");
}

void stringfieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  // noop for strings.
}

void stringfieldgenerator::
generateserializationcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  output.writebytes($number$, get$capitalized_name$bytes());\n"
    "}\n");
}

void stringfieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  size += com.google.protobuf.codedoutputstream\n"
    "    .computebytessize($number$, get$capitalized_name$bytes());\n"
    "}\n");
}

void stringfieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$()\n"
    "    .equals(other.get$capitalized_name$());\n");
}

void stringfieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "hash = (37 * hash) + $constant_name$;\n");
  printer->print(variables_,
    "hash = (53 * hash) + get$capitalized_name$().hashcode();\n");
}

string stringfieldgenerator::getboxedtype() const {
  return "java.lang.string";
}


// ===================================================================

repeatedstringfieldgenerator::
repeatedstringfieldgenerator(const fielddescriptor* descriptor,
                             int messagebitindex,
                             int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setprimitivevariables(descriptor, messagebitindex, builderbitindex,
                        &variables_);
}

repeatedstringfieldgenerator::~repeatedstringfieldgenerator() {}

int repeatedstringfieldgenerator::getnumbitsformessage() const {
  return 0;
}

int repeatedstringfieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void repeatedstringfieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.util.list<java.lang.string>\n"
    "get$capitalized_name$list();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$int get$capitalized_name$count();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.lang.string get$capitalized_name$(int index);\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes(int index);\n");
}


void repeatedstringfieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private com.google.protobuf.lazystringlist $name$_;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<java.lang.string>\n"
    "    get$capitalized_name$list() {\n"
    "  return $name$_;\n"   // note:  unmodifiable list
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public int get$capitalized_name$count() {\n"
    "  return $name$_.size();\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.lang.string get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes(int index) {\n"
    "  return $name$_.getbytestring(index);\n"
    "}\n");

  if (descriptor_->options().packed() &&
      hasgeneratedmethods(descriptor_->containing_type())) {
    printer->print(variables_,
      "private int $name$memoizedserializedsize = -1;\n");
  }
}

void repeatedstringfieldgenerator::
generatebuildermembers(io::printer* printer) const {
  // one field is the list and the bit field keeps track of whether the
  // list is immutable. if it's immutable, the invariant is that it must
  // either an instance of collections.emptylist() or it's an arraylist
  // wrapped in a collections.unmodifiablelist() wrapper and nobody else has
  // a refererence to the underlying arraylist. this invariant allows us to
  // share instances of lists between protocol buffers avoiding expensive
  // memory allocations. note, immutable is a strong guarantee here -- not
  // just that the list cannot be modified via the reference but that the
  // list can never be modified.
  printer->print(variables_,
    "private com.google.protobuf.lazystringlist $name$_ = $empty_list$;\n");

  printer->print(variables_,
    "private void ensure$capitalized_name$ismutable() {\n"
    "  if (!$get_mutable_bit_builder$) {\n"
    "    $name$_ = new com.google.protobuf.lazystringarraylist($name$_);\n"
    "    $set_mutable_bit_builder$;\n"
    "   }\n"
    "}\n");

    // note:  we return an unmodifiable list because otherwise the caller
    //   could hold on to the returned list and modify it after the message
    //   has been built, thus mutating the message which is supposed to be
    //   immutable.
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<java.lang.string>\n"
    "    get$capitalized_name$list() {\n"
    "  return java.util.collections.unmodifiablelist($name$_);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public int get$capitalized_name$count() {\n"
    "  return $name$_.size();\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.lang.string get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public com.google.protobuf.bytestring\n"
    "    get$capitalized_name$bytes(int index) {\n"
    "  return $name$_.getbytestring(index);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    int index, java.lang.string value) {\n"
    "$null_check$"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.set(index, value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder add$capitalized_name$(\n"
    "    java.lang.string value) {\n"
    "$null_check$"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.add(value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder addall$capitalized_name$(\n"
    "    java.lang.iterable<java.lang.string> values) {\n"
    "  ensure$capitalized_name$ismutable();\n"
    "  super.addall(values, $name$_);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder clear$capitalized_name$() {\n"
    "  $name$_ = $empty_list$;\n"
    "  $clear_mutable_bit_builder$;\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");

  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder add$capitalized_name$bytes(\n"
    "    com.google.protobuf.bytestring value) {\n"
    "$null_check$"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.add(value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
}

void repeatedstringfieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for primitives
}

void repeatedstringfieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $empty_list$;\n");
}

void repeatedstringfieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = $empty_list$;\n"
    "$clear_mutable_bit_builder$;\n");
}

void repeatedstringfieldgenerator::
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

void repeatedstringfieldgenerator::
generatebuildingcode(io::printer* printer) const {
  // the code below ensures that the result has an immutable list. if our
  // list is immutable, we can just reuse it. if not, we make it immutable.

  printer->print(variables_,
    "if ($get_mutable_bit_builder$) {\n"
    "  $name$_ = new com.google.protobuf.unmodifiablelazystringlist(\n"
    "      $name$_);\n"
    "  $clear_mutable_bit_builder$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n");
}

void repeatedstringfieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (!$get_mutable_bit_parser$) {\n"
    "  $name$_ = new com.google.protobuf.lazystringarraylist();\n"
    "  $set_mutable_bit_parser$;\n"
    "}\n"
    "$name$_.add(input.readbytes());\n");
}

void repeatedstringfieldgenerator::
generateparsingcodefrompacked(io::printer* printer) const {
  printer->print(variables_,
    "int length = input.readrawvarint32();\n"
    "int limit = input.pushlimit(length);\n"
    "if (!$get_mutable_bit_parser$ && input.getbytesuntillimit() > 0) {\n"
    "  $name$_ = new com.google.protobuf.lazystringarraylist();\n"
    "  $set_mutable_bit_parser$;\n"
    "}\n"
    "while (input.getbytesuntillimit() > 0) {\n"
    "  $name$.add(input.read$capitalized_type$());\n"
    "}\n"
    "input.poplimit(limit);\n");
}

void repeatedstringfieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_mutable_bit_parser$) {\n"
    "  $name$_ = new com.google.protobuf.unmodifiablelazystringlist($name$_);\n"
    "}\n");
}

void repeatedstringfieldgenerator::
generateserializationcode(io::printer* printer) const {
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "if (get$capitalized_name$list().size() > 0) {\n"
      "  output.writerawvarint32($tag$);\n"
      "  output.writerawvarint32($name$memoizedserializedsize);\n"
      "}\n"
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  output.write$capitalized_type$notag($name$_.get(i));\n"
      "}\n");
  } else {
    printer->print(variables_,
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  output.writebytes($number$, $name$_.getbytestring(i));\n"
      "}\n");
  }
}

void repeatedstringfieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "{\n"
    "  int datasize = 0;\n");
  printer->indent();

  printer->print(variables_,
    "for (int i = 0; i < $name$_.size(); i++) {\n"
    "  datasize += com.google.protobuf.codedoutputstream\n"
    "    .computebytessizenotag($name$_.getbytestring(i));\n"
    "}\n");

  printer->print(
      "size += datasize;\n");

  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "if (!get$capitalized_name$list().isempty()) {\n"
      "  size += $tag_size$;\n"
      "  size += com.google.protobuf.codedoutputstream\n"
      "      .computeint32sizenotag(datasize);\n"
      "}\n");
  } else {
    printer->print(variables_,
      "size += $tag_size$ * get$capitalized_name$list().size();\n");
  }

  // cache the data size for packed fields.
  if (descriptor_->options().packed()) {
    printer->print(variables_,
      "$name$memoizedserializedsize = datasize;\n");
  }

  printer->outdent();
  printer->print("}\n");
}

void repeatedstringfieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$list()\n"
    "    .equals(other.get$capitalized_name$list());\n");
}

void repeatedstringfieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "if (get$capitalized_name$count() > 0) {\n"
    "  hash = (37 * hash) + $constant_name$;\n"
    "  hash = (53 * hash) + get$capitalized_name$list().hashcode();\n"
    "}\n");
}

string repeatedstringfieldgenerator::getboxedtype() const {
  return "string";
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
