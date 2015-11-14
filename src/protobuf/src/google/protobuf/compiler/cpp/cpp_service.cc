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

#include <google/protobuf/compiler/cpp/cpp_service.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

servicegenerator::servicegenerator(const servicedescriptor* descriptor,
                                   const options& options)
  : descriptor_(descriptor) {
  vars_["classname"] = descriptor_->name();
  vars_["full_name"] = descriptor_->full_name();
  if (options.dllexport_decl.empty()) {
    vars_["dllexport"] = "";
  } else {
    vars_["dllexport"] = options.dllexport_decl + " ";
  }
}

servicegenerator::~servicegenerator() {}

void servicegenerator::generatedeclarations(io::printer* printer) {
  // forward-declare the stub type.
  printer->print(vars_,
    "class $classname$_stub;\n"
    "\n");

  generateinterface(printer);
  generatestubdefinition(printer);
}

void servicegenerator::generateinterface(io::printer* printer) {
  printer->print(vars_,
    "class $dllexport$$classname$ : public ::google::protobuf::service {\n"
    " protected:\n"
    "  // this class should be treated as an abstract interface.\n"
    "  inline $classname$() {};\n"
    " public:\n"
    "  virtual ~$classname$();\n");
  printer->indent();

  printer->print(vars_,
    "\n"
    "typedef $classname$_stub stub;\n"
    "\n"
    "static const ::google::protobuf::servicedescriptor* descriptor();\n"
    "\n");

  generatemethodsignatures(virtual, printer);

  printer->print(
    "\n"
    "// implements service ----------------------------------------------\n"
    "\n"
    "const ::google::protobuf::servicedescriptor* getdescriptor();\n"
    "void callmethod(const ::google::protobuf::methoddescriptor* method,\n"
    "                ::google::protobuf::rpccontroller* controller,\n"
    "                const ::google::protobuf::message* request,\n"
    "                ::google::protobuf::message* response,\n"
    "                ::google::protobuf::closure* done);\n"
    "const ::google::protobuf::message& getrequestprototype(\n"
    "  const ::google::protobuf::methoddescriptor* method) const;\n"
    "const ::google::protobuf::message& getresponseprototype(\n"
    "  const ::google::protobuf::methoddescriptor* method) const;\n");

  printer->outdent();
  printer->print(vars_,
    "\n"
    " private:\n"
    "  google_disallow_evil_constructors($classname$);\n"
    "};\n"
    "\n");
}

void servicegenerator::generatestubdefinition(io::printer* printer) {
  printer->print(vars_,
    "class $dllexport$$classname$_stub : public $classname$ {\n"
    " public:\n");

  printer->indent();

  printer->print(vars_,
    "$classname$_stub(::google::protobuf::rpcchannel* channel);\n"
    "$classname$_stub(::google::protobuf::rpcchannel* channel,\n"
    "                 ::google::protobuf::service::channelownership ownership);\n"
    "~$classname$_stub();\n"
    "\n"
    "inline ::google::protobuf::rpcchannel* channel() { return channel_; }\n"
    "\n"
    "// implements $classname$ ------------------------------------------\n"
    "\n");

  generatemethodsignatures(non_virtual, printer);

  printer->outdent();
  printer->print(vars_,
    " private:\n"
    "  ::google::protobuf::rpcchannel* channel_;\n"
    "  bool owns_channel_;\n"
    "  google_disallow_evil_constructors($classname$_stub);\n"
    "};\n"
    "\n");
}

void servicegenerator::generatemethodsignatures(
    virtualornon virtual_or_non, io::printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> sub_vars;
    sub_vars["name"] = method->name();
    sub_vars["input_type"] = classname(method->input_type(), true);
    sub_vars["output_type"] = classname(method->output_type(), true);
    sub_vars["virtual"] = virtual_or_non == virtual ? "virtual " : "";

    printer->print(sub_vars,
      "$virtual$void $name$(::google::protobuf::rpccontroller* controller,\n"
      "                     const $input_type$* request,\n"
      "                     $output_type$* response,\n"
      "                     ::google::protobuf::closure* done);\n");
  }
}

// ===================================================================

void servicegenerator::generatedescriptorinitializer(
    io::printer* printer, int index) {
  map<string, string> vars;
  vars["classname"] = descriptor_->name();
  vars["index"] = simpleitoa(index);

  printer->print(vars,
    "$classname$_descriptor_ = file->service($index$);\n");
}

// ===================================================================

void servicegenerator::generateimplementation(io::printer* printer) {
  printer->print(vars_,
    "$classname$::~$classname$() {}\n"
    "\n"
    "const ::google::protobuf::servicedescriptor* $classname$::descriptor() {\n"
    "  protobuf_assigndescriptorsonce();\n"
    "  return $classname$_descriptor_;\n"
    "}\n"
    "\n"
    "const ::google::protobuf::servicedescriptor* $classname$::getdescriptor() {\n"
    "  protobuf_assigndescriptorsonce();\n"
    "  return $classname$_descriptor_;\n"
    "}\n"
    "\n");

  // generate methods of the interface.
  generatenotimplementedmethods(printer);
  generatecallmethod(printer);
  generategetprototype(request, printer);
  generategetprototype(response, printer);

  // generate stub implementation.
  printer->print(vars_,
    "$classname$_stub::$classname$_stub(::google::protobuf::rpcchannel* channel)\n"
    "  : channel_(channel), owns_channel_(false) {}\n"
    "$classname$_stub::$classname$_stub(\n"
    "    ::google::protobuf::rpcchannel* channel,\n"
    "    ::google::protobuf::service::channelownership ownership)\n"
    "  : channel_(channel),\n"
    "    owns_channel_(ownership == ::google::protobuf::service::stub_owns_channel) {}\n"
    "$classname$_stub::~$classname$_stub() {\n"
    "  if (owns_channel_) delete channel_;\n"
    "}\n"
    "\n");

  generatestubmethods(printer);
}

void servicegenerator::generatenotimplementedmethods(io::printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> sub_vars;
    sub_vars["classname"] = descriptor_->name();
    sub_vars["name"] = method->name();
    sub_vars["index"] = simpleitoa(i);
    sub_vars["input_type"] = classname(method->input_type(), true);
    sub_vars["output_type"] = classname(method->output_type(), true);

    printer->print(sub_vars,
      "void $classname$::$name$(::google::protobuf::rpccontroller* controller,\n"
      "                         const $input_type$*,\n"
      "                         $output_type$*,\n"
      "                         ::google::protobuf::closure* done) {\n"
      "  controller->setfailed(\"method $name$() not implemented.\");\n"
      "  done->run();\n"
      "}\n"
      "\n");
  }
}

void servicegenerator::generatecallmethod(io::printer* printer) {
  printer->print(vars_,
    "void $classname$::callmethod(const ::google::protobuf::methoddescriptor* method,\n"
    "                             ::google::protobuf::rpccontroller* controller,\n"
    "                             const ::google::protobuf::message* request,\n"
    "                             ::google::protobuf::message* response,\n"
    "                             ::google::protobuf::closure* done) {\n"
    "  google_dcheck_eq(method->service(), $classname$_descriptor_);\n"
    "  switch(method->index()) {\n");

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> sub_vars;
    sub_vars["name"] = method->name();
    sub_vars["index"] = simpleitoa(i);
    sub_vars["input_type"] = classname(method->input_type(), true);
    sub_vars["output_type"] = classname(method->output_type(), true);

    // note:  down_cast does not work here because it only works on pointers,
    //   not references.
    printer->print(sub_vars,
      "    case $index$:\n"
      "      $name$(controller,\n"
      "             ::google::protobuf::down_cast<const $input_type$*>(request),\n"
      "             ::google::protobuf::down_cast< $output_type$*>(response),\n"
      "             done);\n"
      "      break;\n");
  }

  printer->print(vars_,
    "    default:\n"
    "      google_log(fatal) << \"bad method index; this should never happen.\";\n"
    "      break;\n"
    "  }\n"
    "}\n"
    "\n");
}

void servicegenerator::generategetprototype(requestorresponse which,
                                            io::printer* printer) {
  if (which == request) {
    printer->print(vars_,
      "const ::google::protobuf::message& $classname$::getrequestprototype(\n");
  } else {
    printer->print(vars_,
      "const ::google::protobuf::message& $classname$::getresponseprototype(\n");
  }

  printer->print(vars_,
    "    const ::google::protobuf::methoddescriptor* method) const {\n"
    "  google_dcheck_eq(method->service(), descriptor());\n"
    "  switch(method->index()) {\n");

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    const descriptor* type =
      (which == request) ? method->input_type() : method->output_type();

    map<string, string> sub_vars;
    sub_vars["index"] = simpleitoa(i);
    sub_vars["type"] = classname(type, true);

    printer->print(sub_vars,
      "    case $index$:\n"
      "      return $type$::default_instance();\n");
  }

  printer->print(vars_,
    "    default:\n"
    "      google_log(fatal) << \"bad method index; this should never happen.\";\n"
    "      return *reinterpret_cast< ::google::protobuf::message*>(null);\n"
    "  }\n"
    "}\n"
    "\n");
}

void servicegenerator::generatestubmethods(io::printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> sub_vars;
    sub_vars["classname"] = descriptor_->name();
    sub_vars["name"] = method->name();
    sub_vars["index"] = simpleitoa(i);
    sub_vars["input_type"] = classname(method->input_type(), true);
    sub_vars["output_type"] = classname(method->output_type(), true);

    printer->print(sub_vars,
      "void $classname$_stub::$name$(::google::protobuf::rpccontroller* controller,\n"
      "                              const $input_type$* request,\n"
      "                              $output_type$* response,\n"
      "                              ::google::protobuf::closure* done) {\n"
      "  channel_->callmethod(descriptor()->method($index$),\n"
      "                       controller, request, response, done);\n"
      "}\n");
  }
}

}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
