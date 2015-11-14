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

#include <google/protobuf/compiler/java/java_extension.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/io/printer.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {

const char* typename(fielddescriptor::type field_type) {
  switch (field_type) {
    case fielddescriptor::type_int32   : return "int32";
    case fielddescriptor::type_uint32  : return "uint32";
    case fielddescriptor::type_sint32  : return "sint32";
    case fielddescriptor::type_fixed32 : return "fixed32";
    case fielddescriptor::type_sfixed32: return "sfixed32";
    case fielddescriptor::type_int64   : return "int64";
    case fielddescriptor::type_uint64  : return "uint64";
    case fielddescriptor::type_sint64  : return "sint64";
    case fielddescriptor::type_fixed64 : return "fixed64";
    case fielddescriptor::type_sfixed64: return "sfixed64";
    case fielddescriptor::type_float   : return "float";
    case fielddescriptor::type_double  : return "double";
    case fielddescriptor::type_bool    : return "bool";
    case fielddescriptor::type_string  : return "string";
    case fielddescriptor::type_bytes   : return "bytes";
    case fielddescriptor::type_enum    : return "enum";
    case fielddescriptor::type_group   : return "group";
    case fielddescriptor::type_message : return "message";

    // no default because we want the compiler to complain if any new
    // types are added.
  }

  google_log(fatal) << "can't get here.";
  return null;
}

}

extensiongenerator::extensiongenerator(const fielddescriptor* descriptor)
  : descriptor_(descriptor) {
  if (descriptor_->extension_scope() != null) {
    scope_ = classname(descriptor_->extension_scope());
  } else {
    scope_ = classname(descriptor_->file());
  }
}

extensiongenerator::~extensiongenerator() {}

// initializes the vars referenced in the generated code templates.
void inittemplatevars(const fielddescriptor* descriptor,
                      const string& scope,
                      map<string, string>* vars_pointer) {
  map<string, string> &vars = *vars_pointer;
  vars["scope"] = scope;
  vars["name"] = underscorestocamelcase(descriptor);
  vars["containing_type"] = classname(descriptor->containing_type());
  vars["number"] = simpleitoa(descriptor->number());
  vars["constant_name"] = fieldconstantname(descriptor);
  vars["index"] = simpleitoa(descriptor->index());
  vars["default"] =
      descriptor->is_repeated() ? "" : defaultvalue(descriptor);
  vars["type_constant"] = typename(gettype(descriptor));
  vars["packed"] = descriptor->options().packed() ? "true" : "false";
  vars["enum_map"] = "null";
  vars["prototype"] = "null";

  javatype java_type = getjavatype(descriptor);
  string singular_type;
  switch (java_type) {
    case javatype_message:
      singular_type = classname(descriptor->message_type());
      vars["prototype"] = singular_type + ".getdefaultinstance()";
      break;
    case javatype_enum:
      singular_type = classname(descriptor->enum_type());
      vars["enum_map"] = singular_type + ".internalgetvaluemap()";
      break;
    default:
      singular_type = boxedprimitivetypename(java_type);
      break;
  }
  vars["type"] = descriptor->is_repeated() ?
      "java.util.list<" + singular_type + ">" : singular_type;
  vars["singular_type"] = singular_type;
}

void extensiongenerator::generate(io::printer* printer) {
  map<string, string> vars;
  inittemplatevars(descriptor_, scope_, &vars);
  printer->print(vars,
      "public static final int $constant_name$ = $number$;\n");

  writefielddoccomment(printer, descriptor_);
  if (hasdescriptormethods(descriptor_->file())) {
    // non-lite extensions
    if (descriptor_->extension_scope() == null) {
      // non-nested
      printer->print(
          vars,
          "public static final\n"
          "  com.google.protobuf.generatedmessage.generatedextension<\n"
          "    $containing_type$,\n"
          "    $type$> $name$ = com.google.protobuf.generatedmessage\n"
          "        .newfilescopedgeneratedextension(\n"
          "      $singular_type$.class,\n"
          "      $prototype$);\n");
    } else {
      // nested
      printer->print(
          vars,
          "public static final\n"
          "  com.google.protobuf.generatedmessage.generatedextension<\n"
          "    $containing_type$,\n"
          "    $type$> $name$ = com.google.protobuf.generatedmessage\n"
          "        .newmessagescopedgeneratedextension(\n"
          "      $scope$.getdefaultinstance(),\n"
          "      $index$,\n"
          "      $singular_type$.class,\n"
          "      $prototype$);\n");
    }
  } else {
    // lite extensions
    if (descriptor_->is_repeated()) {
      printer->print(
          vars,
          "public static final\n"
          "  com.google.protobuf.generatedmessagelite.generatedextension<\n"
          "    $containing_type$,\n"
          "    $type$> $name$ = com.google.protobuf.generatedmessagelite\n"
          "        .newrepeatedgeneratedextension(\n"
          "      $containing_type$.getdefaultinstance(),\n"
          "      $prototype$,\n"
          "      $enum_map$,\n"
          "      $number$,\n"
          "      com.google.protobuf.wireformat.fieldtype.$type_constant$,\n"
          "      $packed$);\n");
    } else {
      printer->print(
          vars,
          "public static final\n"
          "  com.google.protobuf.generatedmessagelite.generatedextension<\n"
          "    $containing_type$,\n"
          "    $type$> $name$ = com.google.protobuf.generatedmessagelite\n"
          "        .newsingulargeneratedextension(\n"
          "      $containing_type$.getdefaultinstance(),\n"
          "      $default$,\n"
          "      $prototype$,\n"
          "      $enum_map$,\n"
          "      $number$,\n"
          "      com.google.protobuf.wireformat.fieldtype.$type_constant$);\n");
    }
  }
}

void extensiongenerator::generatenonnestedinitializationcode(
    io::printer* printer) {
  if (descriptor_->extension_scope() == null &&
      hasdescriptormethods(descriptor_->file())) {
    // only applies to non-nested, non-lite extensions.
    printer->print(
        "$name$.internalinit(descriptor.getextensions().get($index$));\n",
        "name", underscorestocamelcase(descriptor_),
        "index", simpleitoa(descriptor_->index()));
  }
}

void extensiongenerator::generateregistrationcode(io::printer* printer) {
  printer->print(
    "registry.add($scope$.$name$);\n",
    "scope", scope_,
    "name", underscorestocamelcase(descriptor_));
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
