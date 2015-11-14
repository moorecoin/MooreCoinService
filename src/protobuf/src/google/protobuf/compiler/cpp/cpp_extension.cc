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

#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <map>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

namespace {

// returns the fully-qualified class name of the message that this field
// extends. this function is used in the google-internal code to handle some
// legacy cases.
string extendeeclassname(const fielddescriptor* descriptor) {
  const descriptor* extendee = descriptor->containing_type();
  return classname(extendee, true);
}

}  // anonymous namespace

extensiongenerator::extensiongenerator(const fielddescriptor* descriptor,
                                       const options& options)
  : descriptor_(descriptor),
    options_(options) {
  // construct type_traits_.
  if (descriptor_->is_repeated()) {
    type_traits_ = "repeated";
  }

  switch (descriptor_->cpp_type()) {
    case fielddescriptor::cpptype_enum:
      type_traits_.append("enumtypetraits< ");
      type_traits_.append(classname(descriptor_->enum_type(), true));
      type_traits_.append(", ");
      type_traits_.append(classname(descriptor_->enum_type(), true));
      type_traits_.append("_isvalid>");
      break;
    case fielddescriptor::cpptype_string:
      type_traits_.append("stringtypetraits");
      break;
    case fielddescriptor::cpptype_message:
      type_traits_.append("messagetypetraits< ");
      type_traits_.append(classname(descriptor_->message_type(), true));
      type_traits_.append(" >");
      break;
    default:
      type_traits_.append("primitivetypetraits< ");
      type_traits_.append(primitivetypename(descriptor_->cpp_type()));
      type_traits_.append(" >");
      break;
  }
}

extensiongenerator::~extensiongenerator() {}

void extensiongenerator::generatedeclaration(io::printer* printer) {
  map<string, string> vars;
  vars["extendee"     ] = extendeeclassname(descriptor_);
  vars["number"       ] = simpleitoa(descriptor_->number());
  vars["type_traits"  ] = type_traits_;
  vars["name"         ] = descriptor_->name();
  vars["field_type"   ] = simpleitoa(static_cast<int>(descriptor_->type()));
  vars["packed"       ] = descriptor_->options().packed() ? "true" : "false";
  vars["constant_name"] = fieldconstantname(descriptor_);

  // if this is a class member, it needs to be declared "static".  otherwise,
  // it needs to be "extern".  in the latter case, it also needs the dll
  // export/import specifier.
  if (descriptor_->extension_scope() == null) {
    vars["qualifier"] = "extern";
    if (!options_.dllexport_decl.empty()) {
      vars["qualifier"] = options_.dllexport_decl + " " + vars["qualifier"];
    }
  } else {
    vars["qualifier"] = "static";
  }

  printer->print(vars,
    "static const int $constant_name$ = $number$;\n"
    "$qualifier$ ::google::protobuf::internal::extensionidentifier< $extendee$,\n"
    "    ::google::protobuf::internal::$type_traits$, $field_type$, $packed$ >\n"
    "  $name$;\n"
    );

}

void extensiongenerator::generatedefinition(io::printer* printer) {
  // if this is a class member, it needs to be declared in its class scope.
  string scope = (descriptor_->extension_scope() == null) ? "" :
    classname(descriptor_->extension_scope(), false) + "::";
  string name = scope + descriptor_->name();

  map<string, string> vars;
  vars["extendee"     ] = extendeeclassname(descriptor_);
  vars["type_traits"  ] = type_traits_;
  vars["name"         ] = name;
  vars["constant_name"] = fieldconstantname(descriptor_);
  vars["default"      ] = defaultvalue(descriptor_);
  vars["field_type"   ] = simpleitoa(static_cast<int>(descriptor_->type()));
  vars["packed"       ] = descriptor_->options().packed() ? "true" : "false";
  vars["scope"        ] = scope;

  if (descriptor_->cpp_type() == fielddescriptor::cpptype_string) {
    // we need to declare a global string which will contain the default value.
    // we cannot declare it at class scope because that would require exposing
    // it in the header which would be annoying for other reasons.  so we
    // replace :: with _ in the name and declare it as a global.
    string global_name = stringreplace(name, "::", "_", true);
    vars["global_name"] = global_name;
    printer->print(vars,
      "const ::std::string $global_name$_default($default$);\n");

    // update the default to refer to the string global.
    vars["default"] = global_name + "_default";
  }

  // likewise, class members need to declare the field constant variable.
  if (descriptor_->extension_scope() != null) {
    printer->print(vars,
      "#ifndef _msc_ver\n"
      "const int $scope$$constant_name$;\n"
      "#endif\n");
  }

  printer->print(vars,
    "::google::protobuf::internal::extensionidentifier< $extendee$,\n"
    "    ::google::protobuf::internal::$type_traits$, $field_type$, $packed$ >\n"
    "  $name$($constant_name$, $default$);\n");
}

void extensiongenerator::generateregistration(io::printer* printer) {
  map<string, string> vars;
  vars["extendee"   ] = extendeeclassname(descriptor_);
  vars["number"     ] = simpleitoa(descriptor_->number());
  vars["field_type" ] = simpleitoa(static_cast<int>(descriptor_->type()));
  vars["is_repeated"] = descriptor_->is_repeated() ? "true" : "false";
  vars["is_packed"  ] = (descriptor_->is_repeated() &&
                         descriptor_->options().packed())
                        ? "true" : "false";

  switch (descriptor_->cpp_type()) {
    case fielddescriptor::cpptype_enum:
      printer->print(vars,
        "::google::protobuf::internal::extensionset::registerenumextension(\n"
        "  &$extendee$::default_instance(),\n"
        "  $number$, $field_type$, $is_repeated$, $is_packed$,\n");
      printer->print(
        "  &$type$_isvalid);\n",
        "type", classname(descriptor_->enum_type(), true));
      break;
    case fielddescriptor::cpptype_message:
      printer->print(vars,
        "::google::protobuf::internal::extensionset::registermessageextension(\n"
        "  &$extendee$::default_instance(),\n"
        "  $number$, $field_type$, $is_repeated$, $is_packed$,\n");
      printer->print(
        "  &$type$::default_instance());\n",
        "type", classname(descriptor_->message_type(), true));
      break;
    default:
      printer->print(vars,
        "::google::protobuf::internal::extensionset::registerextension(\n"
        "  &$extendee$::default_instance(),\n"
        "  $number$, $field_type$, $is_repeated$, $is_packed$);\n");
      break;
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
