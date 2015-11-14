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

#include <google/protobuf/compiler/java/java_service.h>
#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

servicegenerator::servicegenerator(const servicedescriptor* descriptor)
  : descriptor_(descriptor) {}

servicegenerator::~servicegenerator() {}

void servicegenerator::generate(io::printer* printer) {
  bool is_own_file = descriptor_->file()->options().java_multiple_files();
  writeservicedoccomment(printer, descriptor_);
  printer->print(
    "public $static$ abstract class $classname$\n"
    "    implements com.google.protobuf.service {\n",
    "static", is_own_file ? "" : "static",
    "classname", descriptor_->name());
  printer->indent();

  printer->print(
    "protected $classname$() {}\n\n",
    "classname", descriptor_->name());

  generateinterface(printer);

  generatenewreflectiveservicemethod(printer);
  generatenewreflectiveblockingservicemethod(printer);

  generateabstractmethods(printer);

  // generate getdescriptor() and getdescriptorfortype().
  printer->print(
    "public static final\n"
    "    com.google.protobuf.descriptors.servicedescriptor\n"
    "    getdescriptor() {\n"
    "  return $file$.getdescriptor().getservices().get($index$);\n"
    "}\n",
    "file", classname(descriptor_->file()),
    "index", simpleitoa(descriptor_->index()));
  generategetdescriptorfortype(printer);

  // generate more stuff.
  generatecallmethod(printer);
  generategetprototype(request, printer);
  generategetprototype(response, printer);
  generatestub(printer);
  generateblockingstub(printer);

  // add an insertion point.
  printer->print(
    "\n"
    "// @@protoc_insertion_point(class_scope:$full_name$)\n",
    "full_name", descriptor_->full_name());

  printer->outdent();
  printer->print("}\n\n");
}

void servicegenerator::generategetdescriptorfortype(io::printer* printer) {
  printer->print(
    "public final com.google.protobuf.descriptors.servicedescriptor\n"
    "    getdescriptorfortype() {\n"
    "  return getdescriptor();\n"
    "}\n");
}

void servicegenerator::generateinterface(io::printer* printer) {
  printer->print("public interface interface {\n");
  printer->indent();
  generateabstractmethods(printer);
  printer->outdent();
  printer->print("}\n\n");
}

void servicegenerator::generatenewreflectiveservicemethod(
    io::printer* printer) {
  printer->print(
    "public static com.google.protobuf.service newreflectiveservice(\n"
    "    final interface impl) {\n"
    "  return new $classname$() {\n",
    "classname", descriptor_->name());
  printer->indent();
  printer->indent();

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    printer->print("@java.lang.override\n");
    generatemethodsignature(printer, method, is_concrete);
    printer->print(
      " {\n"
      "  impl.$method$(controller, request, done);\n"
      "}\n\n",
      "method", underscorestocamelcase(method));
  }

  printer->outdent();
  printer->print("};\n");
  printer->outdent();
  printer->print("}\n\n");
}

void servicegenerator::generatenewreflectiveblockingservicemethod(
    io::printer* printer) {
  printer->print(
    "public static com.google.protobuf.blockingservice\n"
    "    newreflectiveblockingservice(final blockinginterface impl) {\n"
    "  return new com.google.protobuf.blockingservice() {\n");
  printer->indent();
  printer->indent();

  generategetdescriptorfortype(printer);

  generatecallblockingmethod(printer);
  generategetprototype(request, printer);
  generategetprototype(response, printer);

  printer->outdent();
  printer->print("};\n");
  printer->outdent();
  printer->print("}\n\n");
}

void servicegenerator::generateabstractmethods(io::printer* printer) {
  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    writemethoddoccomment(printer, method);
    generatemethodsignature(printer, method, is_abstract);
    printer->print(";\n\n");
  }
}

void servicegenerator::generatecallmethod(io::printer* printer) {
  printer->print(
    "\n"
    "public final void callmethod(\n"
    "    com.google.protobuf.descriptors.methoddescriptor method,\n"
    "    com.google.protobuf.rpccontroller controller,\n"
    "    com.google.protobuf.message request,\n"
    "    com.google.protobuf.rpccallback<\n"
    "      com.google.protobuf.message> done) {\n"
    "  if (method.getservice() != getdescriptor()) {\n"
    "    throw new java.lang.illegalargumentexception(\n"
    "      \"service.callmethod() given method descriptor for wrong \" +\n"
    "      \"service type.\");\n"
    "  }\n"
    "  switch(method.getindex()) {\n");
  printer->indent();
  printer->indent();

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> vars;
    vars["index"] = simpleitoa(i);
    vars["method"] = underscorestocamelcase(method);
    vars["input"] = classname(method->input_type());
    vars["output"] = classname(method->output_type());
    printer->print(vars,
      "case $index$:\n"
      "  this.$method$(controller, ($input$)request,\n"
      "    com.google.protobuf.rpcutil.<$output$>specializecallback(\n"
      "      done));\n"
      "  return;\n");
  }

  printer->print(
    "default:\n"
    "  throw new java.lang.assertionerror(\"can't get here.\");\n");

  printer->outdent();
  printer->outdent();

  printer->print(
    "  }\n"
    "}\n"
    "\n");
}

void servicegenerator::generatecallblockingmethod(io::printer* printer) {
  printer->print(
    "\n"
    "public final com.google.protobuf.message callblockingmethod(\n"
    "    com.google.protobuf.descriptors.methoddescriptor method,\n"
    "    com.google.protobuf.rpccontroller controller,\n"
    "    com.google.protobuf.message request)\n"
    "    throws com.google.protobuf.serviceexception {\n"
    "  if (method.getservice() != getdescriptor()) {\n"
    "    throw new java.lang.illegalargumentexception(\n"
    "      \"service.callblockingmethod() given method descriptor for \" +\n"
    "      \"wrong service type.\");\n"
    "  }\n"
    "  switch(method.getindex()) {\n");
  printer->indent();
  printer->indent();

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> vars;
    vars["index"] = simpleitoa(i);
    vars["method"] = underscorestocamelcase(method);
    vars["input"] = classname(method->input_type());
    vars["output"] = classname(method->output_type());
    printer->print(vars,
      "case $index$:\n"
      "  return impl.$method$(controller, ($input$)request);\n");
  }

  printer->print(
    "default:\n"
    "  throw new java.lang.assertionerror(\"can't get here.\");\n");

  printer->outdent();
  printer->outdent();

  printer->print(
    "  }\n"
    "}\n"
    "\n");
}

void servicegenerator::generategetprototype(requestorresponse which,
                                            io::printer* printer) {
  /*
   * todo(cpovirk): the exception message says "service.foo" when it may be
   * "blockingservice.foo."  consider fixing.
   */
  printer->print(
    "public final com.google.protobuf.message\n"
    "    get$request_or_response$prototype(\n"
    "    com.google.protobuf.descriptors.methoddescriptor method) {\n"
    "  if (method.getservice() != getdescriptor()) {\n"
    "    throw new java.lang.illegalargumentexception(\n"
    "      \"service.get$request_or_response$prototype() given method \" +\n"
    "      \"descriptor for wrong service type.\");\n"
    "  }\n"
    "  switch(method.getindex()) {\n",
    "request_or_response", (which == request) ? "request" : "response");
  printer->indent();
  printer->indent();

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    map<string, string> vars;
    vars["index"] = simpleitoa(i);
    vars["type"] = classname(
      (which == request) ? method->input_type() : method->output_type());
    printer->print(vars,
      "case $index$:\n"
      "  return $type$.getdefaultinstance();\n");
  }

  printer->print(
    "default:\n"
    "  throw new java.lang.assertionerror(\"can't get here.\");\n");

  printer->outdent();
  printer->outdent();

  printer->print(
    "  }\n"
    "}\n"
    "\n");
}

void servicegenerator::generatestub(io::printer* printer) {
  printer->print(
    "public static stub newstub(\n"
    "    com.google.protobuf.rpcchannel channel) {\n"
    "  return new stub(channel);\n"
    "}\n"
    "\n"
    "public static final class stub extends $classname$ implements interface {"
    "\n",
    "classname", classname(descriptor_));
  printer->indent();

  printer->print(
    "private stub(com.google.protobuf.rpcchannel channel) {\n"
    "  this.channel = channel;\n"
    "}\n"
    "\n"
    "private final com.google.protobuf.rpcchannel channel;\n"
    "\n"
    "public com.google.protobuf.rpcchannel getchannel() {\n"
    "  return channel;\n"
    "}\n");

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    printer->print("\n");
    generatemethodsignature(printer, method, is_concrete);
    printer->print(" {\n");
    printer->indent();

    map<string, string> vars;
    vars["index"] = simpleitoa(i);
    vars["output"] = classname(method->output_type());
    printer->print(vars,
      "channel.callmethod(\n"
      "  getdescriptor().getmethods().get($index$),\n"
      "  controller,\n"
      "  request,\n"
      "  $output$.getdefaultinstance(),\n"
      "  com.google.protobuf.rpcutil.generalizecallback(\n"
      "    done,\n"
      "    $output$.class,\n"
      "    $output$.getdefaultinstance()));\n");

    printer->outdent();
    printer->print("}\n");
  }

  printer->outdent();
  printer->print(
    "}\n"
    "\n");
}

void servicegenerator::generateblockingstub(io::printer* printer) {
  printer->print(
    "public static blockinginterface newblockingstub(\n"
    "    com.google.protobuf.blockingrpcchannel channel) {\n"
    "  return new blockingstub(channel);\n"
    "}\n"
    "\n");

  printer->print(
    "public interface blockinginterface {");
  printer->indent();

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    generateblockingmethodsignature(printer, method);
    printer->print(";\n");
  }

  printer->outdent();
  printer->print(
    "}\n"
    "\n");

  printer->print(
    "private static final class blockingstub implements blockinginterface {\n");
  printer->indent();

  printer->print(
    "private blockingstub(com.google.protobuf.blockingrpcchannel channel) {\n"
    "  this.channel = channel;\n"
    "}\n"
    "\n"
    "private final com.google.protobuf.blockingrpcchannel channel;\n");

  for (int i = 0; i < descriptor_->method_count(); i++) {
    const methoddescriptor* method = descriptor_->method(i);
    generateblockingmethodsignature(printer, method);
    printer->print(" {\n");
    printer->indent();

    map<string, string> vars;
    vars["index"] = simpleitoa(i);
    vars["output"] = classname(method->output_type());
    printer->print(vars,
      "return ($output$) channel.callblockingmethod(\n"
      "  getdescriptor().getmethods().get($index$),\n"
      "  controller,\n"
      "  request,\n"
      "  $output$.getdefaultinstance());\n");

    printer->outdent();
    printer->print(
      "}\n"
      "\n");
  }

  printer->outdent();
  printer->print("}\n");
}

void servicegenerator::generatemethodsignature(io::printer* printer,
                                               const methoddescriptor* method,
                                               isabstract is_abstract) {
  map<string, string> vars;
  vars["name"] = underscorestocamelcase(method);
  vars["input"] = classname(method->input_type());
  vars["output"] = classname(method->output_type());
  vars["abstract"] = (is_abstract == is_abstract) ? "abstract" : "";
  printer->print(vars,
    "public $abstract$ void $name$(\n"
    "    com.google.protobuf.rpccontroller controller,\n"
    "    $input$ request,\n"
    "    com.google.protobuf.rpccallback<$output$> done)");
}

void servicegenerator::generateblockingmethodsignature(
    io::printer* printer,
    const methoddescriptor* method) {
  map<string, string> vars;
  vars["method"] = underscorestocamelcase(method);
  vars["input"] = classname(method->input_type());
  vars["output"] = classname(method->output_type());
  printer->print(vars,
    "\n"
    "public $output$ $method$(\n"
    "    com.google.protobuf.rpccontroller controller,\n"
    "    $input$ request)\n"
    "    throws com.google.protobuf.serviceexception");
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
