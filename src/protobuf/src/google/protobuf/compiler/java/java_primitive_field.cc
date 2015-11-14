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

#include <google/protobuf/compiler/java/java_primitive_field.h>
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

const char* primitivetypename(javatype type) {
  switch (type) {
    case javatype_int    : return "int";
    case javatype_long   : return "long";
    case javatype_float  : return "float";
    case javatype_double : return "double";
    case javatype_boolean: return "boolean";
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

bool isreferencetype(javatype type) {
  switch (type) {
    case javatype_int    : return false;
    case javatype_long   : return false;
    case javatype_float  : return false;
    case javatype_double : return false;
    case javatype_boolean: return false;
    case javatype_string : return true;
    case javatype_bytes  : return true;
    case javatype_enum   : return true;
    case javatype_message: return true;

    // no default because we want the compiler to complain if any new
    // javatypes are added.
  }

  google_log(fatal) << "can't get here.";
  return false;
}

const char* getcapitalizedtype(const fielddescriptor* field) {
  switch (gettype(field)) {
    case fielddescriptor::type_int32   : return "int32"   ;
    case fielddescriptor::type_uint32  : return "uint32"  ;
    case fielddescriptor::type_sint32  : return "sint32"  ;
    case fielddescriptor::type_fixed32 : return "fixed32" ;
    case fielddescriptor::type_sfixed32: return "sfixed32";
    case fielddescriptor::type_int64   : return "int64"   ;
    case fielddescriptor::type_uint64  : return "uint64"  ;
    case fielddescriptor::type_sint64  : return "sint64"  ;
    case fielddescriptor::type_fixed64 : return "fixed64" ;
    case fielddescriptor::type_sfixed64: return "sfixed64";
    case fielddescriptor::type_float   : return "float"   ;
    case fielddescriptor::type_double  : return "double"  ;
    case fielddescriptor::type_bool    : return "bool"    ;
    case fielddescriptor::type_string  : return "string"  ;
    case fielddescriptor::type_bytes   : return "bytes"   ;
    case fielddescriptor::type_enum    : return "enum"    ;
    case fielddescriptor::type_group   : return "group"   ;
    case fielddescriptor::type_message : return "message" ;

    // no default because we want the compiler to complain if any new
    // types are added.
  }

  google_log(fatal) << "can't get here.";
  return null;
}

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
                           int messagebitindex,
                           int builderbitindex,
                           map<string, string>* variables) {
  (*variables)["name"] =
    underscorestocamelcase(descriptor);
  (*variables)["capitalized_name"] =
    underscorestocapitalizedcamelcase(descriptor);
  (*variables)["constant_name"] = fieldconstantname(descriptor);
  (*variables)["number"] = simpleitoa(descriptor->number());
  (*variables)["type"] = primitivetypename(getjavatype(descriptor));
  (*variables)["boxed_type"] = boxedprimitivetypename(getjavatype(descriptor));
  (*variables)["field_type"] = (*variables)["type"];
  (*variables)["field_list_type"] = "java.util.list<" +
      (*variables)["boxed_type"] + ">";
  (*variables)["empty_list"] = "java.util.collections.emptylist()";
  (*variables)["default"] = defaultvalue(descriptor);
  (*variables)["default_init"] = isdefaultvaluejavadefault(descriptor) ?
      "" : ("= " + defaultvalue(descriptor));
  (*variables)["capitalized_type"] = getcapitalizedtype(descriptor);
  (*variables)["tag"] = simpleitoa(wireformat::maketag(descriptor));
  (*variables)["tag_size"] = simpleitoa(
      wireformat::tagsize(descriptor->number(), gettype(descriptor)));
  if (isreferencetype(getjavatype(descriptor))) {
    (*variables)["null_check"] =
        "  if (value == null) {\n"
        "    throw new nullpointerexception();\n"
        "  }\n";
  } else {
    (*variables)["null_check"] = "";
  }
  // todo(birdo): add @deprecated javadoc when generating javadoc is supported
  // by the proto compiler
  (*variables)["deprecation"] = descriptor->options().deprecated()
      ? "@java.lang.deprecated " : "";
  int fixed_size = fixedsize(gettype(descriptor));
  if (fixed_size != -1) {
    (*variables)["fixed_size"] = simpleitoa(fixed_size);
  }
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

primitivefieldgenerator::
primitivefieldgenerator(const fielddescriptor* descriptor,
                        int messagebitindex,
                        int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setprimitivevariables(descriptor, messagebitindex, builderbitindex,
                        &variables_);
}

primitivefieldgenerator::~primitivefieldgenerator() {}

int primitivefieldgenerator::getnumbitsformessage() const {
  return 1;
}

int primitivefieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void primitivefieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$boolean has$capitalized_name$();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$();\n");
}

void primitivefieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private $field_type$ $name$_;\n");

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

void primitivefieldgenerator::
generatebuildermembers(io::printer* printer) const {
  printer->print(variables_,
    "private $field_type$ $name$_ $default_init$;\n");

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
  javatype type = getjavatype(descriptor_);
  if (type == javatype_string || type == javatype_bytes) {
    // the default value is not a simple literal so we want to avoid executing
    // it multiple times.  instead, get the default out of the default instance.
    printer->print(variables_,
      "  $name$_ = getdefaultinstance().get$capitalized_name$();\n");
  } else {
    printer->print(variables_,
      "  $name$_ = $default$;\n");
  }
  printer->print(variables_,
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
}

void primitivefieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for primitives
}

void primitivefieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $default$;\n");
}

void primitivefieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = $default$;\n"
    "$clear_has_field_bit_builder$;\n");
}

void primitivefieldgenerator::
generatemergingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (other.has$capitalized_name$()) {\n"
    "  set$capitalized_name$(other.get$capitalized_name$());\n"
    "}\n");
}

void primitivefieldgenerator::
generatebuildingcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_from_local$) {\n"
    "  $set_has_field_bit_to_local$;\n"
    "}\n"
    "result.$name$_ = $name$_;\n");
}

void primitivefieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "$set_has_field_bit_message$;\n"
    "$name$_ = input.read$capitalized_type$();\n");
}

void primitivefieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  // noop for primitives.
}

void primitivefieldgenerator::
generateserializationcode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  output.write$capitalized_type$($number$, $name$_);\n"
    "}\n");
}

void primitivefieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_has_field_bit_message$) {\n"
    "  size += com.google.protobuf.codedoutputstream\n"
    "    .compute$capitalized_type$size($number$, $name$_);\n"
    "}\n");
}

void primitivefieldgenerator::
generateequalscode(io::printer* printer) const {
  switch (getjavatype(descriptor_)) {
    case javatype_int:
    case javatype_long:
    case javatype_boolean:
      printer->print(variables_,
        "result = result && (get$capitalized_name$()\n"
        "    == other.get$capitalized_name$());\n");
      break;

    case javatype_float:
      printer->print(variables_,
        "result = result && (float.floattointbits(get$capitalized_name$())"
        "    == float.floattointbits(other.get$capitalized_name$()));\n");
      break;

    case javatype_double:
      printer->print(variables_,
        "result = result && (double.doubletolongbits(get$capitalized_name$())"
        "    == double.doubletolongbits(other.get$capitalized_name$()));\n");
      break;

    case javatype_string:
    case javatype_bytes:
      printer->print(variables_,
        "result = result && get$capitalized_name$()\n"
        "    .equals(other.get$capitalized_name$());\n");
      break;

    case javatype_enum:
    case javatype_message:
    default:
      google_log(fatal) << "can't get here.";
      break;
  }
}

void primitivefieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "hash = (37 * hash) + $constant_name$;\n");
  switch (getjavatype(descriptor_)) {
    case javatype_int:
      printer->print(variables_,
        "hash = (53 * hash) + get$capitalized_name$();\n");
      break;

    case javatype_long:
      printer->print(variables_,
        "hash = (53 * hash) + hashlong(get$capitalized_name$());\n");
      break;

    case javatype_boolean:
      printer->print(variables_,
        "hash = (53 * hash) + hashboolean(get$capitalized_name$());\n");
      break;

    case javatype_float:
      printer->print(variables_,
        "hash = (53 * hash) + float.floattointbits(\n"
        "    get$capitalized_name$());\n");
      break;

    case javatype_double:
      printer->print(variables_,
        "hash = (53 * hash) + hashlong(\n"
        "    double.doubletolongbits(get$capitalized_name$()));\n");
      break;

    case javatype_string:
    case javatype_bytes:
      printer->print(variables_,
        "hash = (53 * hash) + get$capitalized_name$().hashcode();\n");
      break;

    case javatype_enum:
    case javatype_message:
    default:
      google_log(fatal) << "can't get here.";
      break;
  }
}

string primitivefieldgenerator::getboxedtype() const {
  return boxedprimitivetypename(getjavatype(descriptor_));
}

// ===================================================================

repeatedprimitivefieldgenerator::
repeatedprimitivefieldgenerator(const fielddescriptor* descriptor,
                                int messagebitindex,
                                int builderbitindex)
  : descriptor_(descriptor), messagebitindex_(messagebitindex),
    builderbitindex_(builderbitindex) {
  setprimitivevariables(descriptor, messagebitindex, builderbitindex,
                        &variables_);
}

repeatedprimitivefieldgenerator::~repeatedprimitivefieldgenerator() {}

int repeatedprimitivefieldgenerator::getnumbitsformessage() const {
  return 0;
}

int repeatedprimitivefieldgenerator::getnumbitsforbuilder() const {
  return 1;
}

void repeatedprimitivefieldgenerator::
generateinterfacemembers(io::printer* printer) const {
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$java.util.list<$boxed_type$> get$capitalized_name$list();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$int get$capitalized_name$count();\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$$type$ get$capitalized_name$(int index);\n");
}


void repeatedprimitivefieldgenerator::
generatemembers(io::printer* printer) const {
  printer->print(variables_,
    "private $field_list_type$ $name$_;\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<$boxed_type$>\n"
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
    "$deprecation$public $type$ get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");

  if (descriptor_->options().packed() &&
      hasgeneratedmethods(descriptor_->containing_type())) {
    printer->print(variables_,
      "private int $name$memoizedserializedsize = -1;\n");
  }
}

void repeatedprimitivefieldgenerator::
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
    "private $field_list_type$ $name$_ = $empty_list$;\n");

  printer->print(variables_,
    "private void ensure$capitalized_name$ismutable() {\n"
    "  if (!$get_mutable_bit_builder$) {\n"
    "    $name$_ = new java.util.arraylist<$boxed_type$>($name$_);\n"
    "    $set_mutable_bit_builder$;\n"
    "   }\n"
    "}\n");

    // note:  we return an unmodifiable list because otherwise the caller
    //   could hold on to the returned list and modify it after the message
    //   has been built, thus mutating the message which is supposed to be
    //   immutable.
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public java.util.list<$boxed_type$>\n"
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
    "$deprecation$public $type$ get$capitalized_name$(int index) {\n"
    "  return $name$_.get(index);\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder set$capitalized_name$(\n"
    "    int index, $type$ value) {\n"
    "$null_check$"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.set(index, value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder add$capitalized_name$($type$ value) {\n"
    "$null_check$"
    "  ensure$capitalized_name$ismutable();\n"
    "  $name$_.add(value);\n"
    "  $on_changed$\n"
    "  return this;\n"
    "}\n");
  writefielddoccomment(printer, descriptor_);
  printer->print(variables_,
    "$deprecation$public builder addall$capitalized_name$(\n"
    "    java.lang.iterable<? extends $boxed_type$> values) {\n"
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
}

void repeatedprimitivefieldgenerator::
generatefieldbuilderinitializationcode(io::printer* printer)  const {
  // noop for primitives
}

void repeatedprimitivefieldgenerator::
generateinitializationcode(io::printer* printer) const {
  printer->print(variables_, "$name$_ = $empty_list$;\n");
}

void repeatedprimitivefieldgenerator::
generatebuilderclearcode(io::printer* printer) const {
  printer->print(variables_,
    "$name$_ = $empty_list$;\n"
    "$clear_mutable_bit_builder$;\n");
}

void repeatedprimitivefieldgenerator::
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

void repeatedprimitivefieldgenerator::
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

void repeatedprimitivefieldgenerator::
generateparsingcode(io::printer* printer) const {
  printer->print(variables_,
    "if (!$get_mutable_bit_parser$) {\n"
    "  $name$_ = new java.util.arraylist<$boxed_type$>();\n"
    "  $set_mutable_bit_parser$;\n"
    "}\n"
    "$name$_.add(input.read$capitalized_type$());\n");
}

void repeatedprimitivefieldgenerator::
generateparsingcodefrompacked(io::printer* printer) const {
  printer->print(variables_,
    "int length = input.readrawvarint32();\n"
    "int limit = input.pushlimit(length);\n"
    "if (!$get_mutable_bit_parser$ && input.getbytesuntillimit() > 0) {\n"
    "  $name$_ = new java.util.arraylist<$boxed_type$>();\n"
    "  $set_mutable_bit_parser$;\n"
    "}\n"
    "while (input.getbytesuntillimit() > 0) {\n"
    "  $name$_.add(input.read$capitalized_type$());\n"
    "}\n"
    "input.poplimit(limit);\n");
}

void repeatedprimitivefieldgenerator::
generateparsingdonecode(io::printer* printer) const {
  printer->print(variables_,
    "if ($get_mutable_bit_parser$) {\n"
    "  $name$_ = java.util.collections.unmodifiablelist($name$_);\n"
    "}\n");
}

void repeatedprimitivefieldgenerator::
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
      "  output.write$capitalized_type$($number$, $name$_.get(i));\n"
      "}\n");
  }
}

void repeatedprimitivefieldgenerator::
generateserializedsizecode(io::printer* printer) const {
  printer->print(variables_,
    "{\n"
    "  int datasize = 0;\n");
  printer->indent();

  if (fixedsize(gettype(descriptor_)) == -1) {
    printer->print(variables_,
      "for (int i = 0; i < $name$_.size(); i++) {\n"
      "  datasize += com.google.protobuf.codedoutputstream\n"
      "    .compute$capitalized_type$sizenotag($name$_.get(i));\n"
      "}\n");
  } else {
    printer->print(variables_,
      "datasize = $fixed_size$ * get$capitalized_name$list().size();\n");
  }

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

void repeatedprimitivefieldgenerator::
generateequalscode(io::printer* printer) const {
  printer->print(variables_,
    "result = result && get$capitalized_name$list()\n"
    "    .equals(other.get$capitalized_name$list());\n");
}

void repeatedprimitivefieldgenerator::
generatehashcode(io::printer* printer) const {
  printer->print(variables_,
    "if (get$capitalized_name$count() > 0) {\n"
    "  hash = (37 * hash) + $constant_name$;\n"
    "  hash = (53 * hash) + get$capitalized_name$list().hashcode();\n"
    "}\n");
}

string repeatedprimitivefieldgenerator::getboxedtype() const {
  return boxedprimitivetypename(getjavatype(descriptor_));
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
