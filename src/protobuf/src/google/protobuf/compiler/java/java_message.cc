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

#include <google/protobuf/compiler/java/java_message.h>

#include <algorithm>
#include <google/protobuf/stubs/hash.h>
#include <map>
#include <vector>

#include <google/protobuf/compiler/java/java_doc_comment.h>
#include <google/protobuf/compiler/java/java_enum.h>
#include <google/protobuf/compiler/java/java_extension.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

using internal::wireformat;
using internal::wireformatlite;

namespace {

void printfieldcomment(io::printer* printer, const fielddescriptor* field) {
  // print the field's proto-syntax definition as a comment.  we don't want to
  // print group bodies so we cut off after the first line.
  string def = field->debugstring();
  printer->print("// $def$\n",
    "def", def.substr(0, def.find_first_of('\n')));
}

struct fieldorderingbynumber {
  inline bool operator()(const fielddescriptor* a,
                         const fielddescriptor* b) const {
    return a->number() < b->number();
  }
};

struct extensionrangeordering {
  bool operator()(const descriptor::extensionrange* a,
                  const descriptor::extensionrange* b) const {
    return a->start < b->start;
  }
};

// sort the fields of the given descriptor by number into a new[]'d array
// and return it.
const fielddescriptor** sortfieldsbynumber(const descriptor* descriptor) {
  const fielddescriptor** fields =
    new const fielddescriptor*[descriptor->field_count()];
  for (int i = 0; i < descriptor->field_count(); i++) {
    fields[i] = descriptor->field(i);
  }
  sort(fields, fields + descriptor->field_count(),
       fieldorderingbynumber());
  return fields;
}

// get an identifier that uniquely identifies this type within the file.
// this is used to declare static variables related to this type at the
// outermost file scope.
string uniquefilescopeidentifier(const descriptor* descriptor) {
  return "static_" + stringreplace(descriptor->full_name(), ".", "_", true);
}

// returns true if the message type has any required fields.  if it doesn't,
// we can optimize out calls to its isinitialized() method.
//
// already_seen is used to avoid checking the same type multiple times
// (and also to protect against recursion).
static bool hasrequiredfields(
    const descriptor* type,
    hash_set<const descriptor*>* already_seen) {
  if (already_seen->count(type) > 0) {
    // the type is already in cache.  this means that either:
    // a. the type has no required fields.
    // b. we are in the midst of checking if the type has required fields,
    //    somewhere up the stack.  in this case, we know that if the type
    //    has any required fields, they'll be found when we return to it,
    //    and the whole call to hasrequiredfields() will return true.
    //    therefore, we don't have to check if this type has required fields
    //    here.
    return false;
  }
  already_seen->insert(type);

  // if the type has extensions, an extension with message type could contain
  // required fields, so we have to be conservative and assume such an
  // extension exists.
  if (type->extension_range_count() > 0) return true;

  for (int i = 0; i < type->field_count(); i++) {
    const fielddescriptor* field = type->field(i);
    if (field->is_required()) {
      return true;
    }
    if (getjavatype(field) == javatype_message) {
      if (hasrequiredfields(field->message_type(), already_seen)) {
        return true;
      }
    }
  }

  return false;
}

static bool hasrequiredfields(const descriptor* type) {
  hash_set<const descriptor*> already_seen;
  return hasrequiredfields(type, &already_seen);
}

}  // namespace

// ===================================================================

messagegenerator::messagegenerator(const descriptor* descriptor)
  : descriptor_(descriptor),
    field_generators_(descriptor) {
}

messagegenerator::~messagegenerator() {}

void messagegenerator::generatestaticvariables(io::printer* printer) {
  if (hasdescriptormethods(descriptor_)) {
    // because descriptor.proto (com.google.protobuf.descriptorprotos) is
    // used in the construction of descriptors, we have a tricky bootstrapping
    // problem.  to help control static initialization order, we make sure all
    // descriptors and other static data that depends on them are members of
    // the outermost class in the file.  this way, they will be initialized in
    // a deterministic order.

    map<string, string> vars;
    vars["identifier"] = uniquefilescopeidentifier(descriptor_);
    vars["index"] = simpleitoa(descriptor_->index());
    vars["classname"] = classname(descriptor_);
    if (descriptor_->containing_type() != null) {
      vars["parent"] = uniquefilescopeidentifier(
          descriptor_->containing_type());
    }
    if (descriptor_->file()->options().java_multiple_files()) {
      // we can only make these package-private since the classes that use them
      // are in separate files.
      vars["private"] = "";
    } else {
      vars["private"] = "private ";
    }

    // the descriptor for this type.
    printer->print(vars,
      "$private$static com.google.protobuf.descriptors.descriptor\n"
      "  internal_$identifier$_descriptor;\n");

    // and the fieldaccessortable.
    printer->print(vars,
      "$private$static\n"
      "  com.google.protobuf.generatedmessage.fieldaccessortable\n"
      "    internal_$identifier$_fieldaccessortable;\n");
  }

  // generate static members for all nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    // todo(kenton):  reuse messagegenerator objects?
    messagegenerator(descriptor_->nested_type(i))
      .generatestaticvariables(printer);
  }
}

void messagegenerator::generatestaticvariableinitializers(
    io::printer* printer) {
  if (hasdescriptormethods(descriptor_)) {
    map<string, string> vars;
    vars["identifier"] = uniquefilescopeidentifier(descriptor_);
    vars["index"] = simpleitoa(descriptor_->index());
    vars["classname"] = classname(descriptor_);
    if (descriptor_->containing_type() != null) {
      vars["parent"] = uniquefilescopeidentifier(
          descriptor_->containing_type());
    }

    // the descriptor for this type.
    if (descriptor_->containing_type() == null) {
      printer->print(vars,
        "internal_$identifier$_descriptor =\n"
        "  getdescriptor().getmessagetypes().get($index$);\n");
    } else {
      printer->print(vars,
        "internal_$identifier$_descriptor =\n"
        "  internal_$parent$_descriptor.getnestedtypes().get($index$);\n");
    }

    // and the fieldaccessortable.
    printer->print(vars,
      "internal_$identifier$_fieldaccessortable = new\n"
      "  com.google.protobuf.generatedmessage.fieldaccessortable(\n"
      "    internal_$identifier$_descriptor,\n"
      "    new java.lang.string[] { ");
    for (int i = 0; i < descriptor_->field_count(); i++) {
      printer->print(
        "\"$field_name$\", ",
        "field_name",
          underscorestocapitalizedcamelcase(descriptor_->field(i)));
    }
    printer->print(
        "});\n");
  }

  // generate static member initializers for all nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    // todo(kenton):  reuse messagegenerator objects?
    messagegenerator(descriptor_->nested_type(i))
      .generatestaticvariableinitializers(printer);
  }
}

// ===================================================================

void messagegenerator::generateinterface(io::printer* printer) {
  if (descriptor_->extension_range_count() > 0) {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public interface $classname$orbuilder extends\n"
        "    com.google.protobuf.generatedmessage.\n"
        "        extendablemessageorbuilder<$classname$> {\n",
        "classname", descriptor_->name());
    } else {
      printer->print(
        "public interface $classname$orbuilder extends \n"
        "     com.google.protobuf.generatedmessagelite.\n"
        "          extendablemessageorbuilder<$classname$> {\n",
        "classname", descriptor_->name());
    }
  } else {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public interface $classname$orbuilder\n"
        "    extends com.google.protobuf.messageorbuilder {\n",
        "classname", descriptor_->name());
    } else {
      printer->print(
        "public interface $classname$orbuilder\n"
        "    extends com.google.protobuf.messageliteorbuilder {\n",
        "classname", descriptor_->name());
    }
  }

  printer->indent();
    for (int i = 0; i < descriptor_->field_count(); i++) {
      printer->print("\n");
      printfieldcomment(printer, descriptor_->field(i));
      field_generators_.get(descriptor_->field(i))
                       .generateinterfacemembers(printer);
    }
  printer->outdent();

  printer->print("}\n");
}

// ===================================================================

void messagegenerator::generate(io::printer* printer) {
  bool is_own_file =
    descriptor_->containing_type() == null &&
    descriptor_->file()->options().java_multiple_files();

  writemessagedoccomment(printer, descriptor_);

  // the builder_type stores the super type name of the nested builder class.
  string builder_type;
  if (descriptor_->extension_range_count() > 0) {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public $static$ final class $classname$ extends\n"
        "    com.google.protobuf.generatedmessage.extendablemessage<\n"
        "      $classname$> implements $classname$orbuilder {\n",
        "static", is_own_file ? "" : "static",
        "classname", descriptor_->name());
      builder_type = strings::substitute(
          "com.google.protobuf.generatedmessage.extendablebuilder<$0, ?>",
          classname(descriptor_));
    } else {
      printer->print(
        "public $static$ final class $classname$ extends\n"
        "    com.google.protobuf.generatedmessagelite.extendablemessage<\n"
        "      $classname$> implements $classname$orbuilder {\n",
        "static", is_own_file ? "" : "static",
        "classname", descriptor_->name());
      builder_type = strings::substitute(
          "com.google.protobuf.generatedmessagelite.extendablebuilder<$0, ?>",
          classname(descriptor_));
    }
  } else {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public $static$ final class $classname$ extends\n"
        "    com.google.protobuf.generatedmessage\n"
        "    implements $classname$orbuilder {\n",
        "static", is_own_file ? "" : "static",
        "classname", descriptor_->name());
      builder_type = "com.google.protobuf.generatedmessage.builder<?>";
    } else {
      printer->print(
        "public $static$ final class $classname$ extends\n"
        "    com.google.protobuf.generatedmessagelite\n"
        "    implements $classname$orbuilder {\n",
        "static", is_own_file ? "" : "static",
        "classname", descriptor_->name());
      builder_type = "com.google.protobuf.generatedmessagelite.builder";
    }
  }
  printer->indent();
  // using builder_type, instead of builder, prevents the builder class from
  // being loaded into permgen space when the default instance is created.
  // this optimizes the permgen space usage for clients that do not modify
  // messages.
  printer->print(
    "// use $classname$.newbuilder() to construct.\n"
    "private $classname$($buildertype$ builder) {\n"
    "  super(builder);\n"
    "$set_unknown_fields$\n"
    "}\n",
    "classname", descriptor_->name(),
    "buildertype", builder_type,
    "set_unknown_fields", hasunknownfields(descriptor_)
        ? "  this.unknownfields = builder.getunknownfields();" : "");
  printer->print(
    // used when constructing the default instance, which cannot be initialized
    // immediately because it may cyclically refer to other default instances.
    "private $classname$(boolean noinit) {$set_default_unknown_fields$}\n"
    "\n"
    "private static final $classname$ defaultinstance;\n"
    "public static $classname$ getdefaultinstance() {\n"
    "  return defaultinstance;\n"
    "}\n"
    "\n"
    "public $classname$ getdefaultinstancefortype() {\n"
    "  return defaultinstance;\n"
    "}\n"
    "\n",
    "classname", descriptor_->name(),
    "set_default_unknown_fields", hasunknownfields(descriptor_)
        ? " this.unknownfields ="
          " com.google.protobuf.unknownfieldset.getdefaultinstance(); " : "");

  if (hasunknownfields(descriptor_)) {
    printer->print(
        "private final com.google.protobuf.unknownfieldset unknownfields;\n"
        ""
        "@java.lang.override\n"
        "public final com.google.protobuf.unknownfieldset\n"
        "    getunknownfields() {\n"
        "  return this.unknownfields;\n"
        "}\n");
  }

  if (hasgeneratedmethods(descriptor_)) {
    generateparsingconstructor(printer);
  }

  generatedescriptormethods(printer);
  generateparser(printer);

  // nested types
  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enumgenerator(descriptor_->enum_type(i)).generate(printer);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    messagegenerator messagegenerator(descriptor_->nested_type(i));
    messagegenerator.generateinterface(printer);
    messagegenerator.generate(printer);
  }

  // integers for bit fields.
  int totalbits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    totalbits += field_generators_.get(descriptor_->field(i))
        .getnumbitsformessage();
  }
  int totalints = (totalbits + 31) / 32;
  for (int i = 0; i < totalints; i++) {
    printer->print("private int $bit_field_name$;\n",
      "bit_field_name", getbitfieldname(i));
  }

  // fields
  for (int i = 0; i < descriptor_->field_count(); i++) {
    printfieldcomment(printer, descriptor_->field(i));
    printer->print("public static final int $constant_name$ = $number$;\n",
      "constant_name", fieldconstantname(descriptor_->field(i)),
      "number", simpleitoa(descriptor_->field(i)->number()));
    field_generators_.get(descriptor_->field(i)).generatemembers(printer);
    printer->print("\n");
  }

  // called by the constructor, except in the case of the default instance,
  // in which case this is called by static init code later on.
  printer->print("private void initfields() {\n");
  printer->indent();
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generateinitializationcode(printer);
  }
  printer->outdent();
  printer->print("}\n");

  if (hasgeneratedmethods(descriptor_)) {
    generateisinitialized(printer, memoize);
    generatemessageserializationmethods(printer);
  }

  if (hasequalsandhashcode(descriptor_)) {
    generateequalsandhashcode(printer);
  }

  generateparsefrommethods(printer);
  generatebuilder(printer);

  // carefully initialize the default instance in such a way that it doesn't
  // conflict with other initialization.
  printer->print(
    "\n"
    "static {\n"
    "  defaultinstance = new $classname$(true);\n"
    "  defaultinstance.initfields();\n"
    "}\n"
    "\n"
    "// @@protoc_insertion_point(class_scope:$full_name$)\n",
    "classname", descriptor_->name(),
    "full_name", descriptor_->full_name());

  // extensions must be declared after the defaultinstance is initialized
  // because the defaultinstance is used by the extension to lazily retrieve
  // the outer class's filedescriptor.
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extensiongenerator(descriptor_->extension(i)).generate(printer);
  }

  printer->outdent();
  printer->print("}\n\n");
}


// ===================================================================

void messagegenerator::
generatemessageserializationmethods(io::printer* printer) {
  scoped_array<const fielddescriptor*> sorted_fields(
    sortfieldsbynumber(descriptor_));

  vector<const descriptor::extensionrange*> sorted_extensions;
  for (int i = 0; i < descriptor_->extension_range_count(); ++i) {
    sorted_extensions.push_back(descriptor_->extension_range(i));
  }
  sort(sorted_extensions.begin(), sorted_extensions.end(),
       extensionrangeordering());

  printer->print(
    "public void writeto(com.google.protobuf.codedoutputstream output)\n"
    "                    throws java.io.ioexception {\n");
  printer->indent();
  // writeto(codedoutputstream output) might be invoked without
  // getserializedsize() ever being called, but we need the memoized
  // sizes in case this message has packed fields. rather than emit checks for
  // each packed field, just call getserializedsize() up front for all messages.
  // in most cases, getserializedsize() will have already been called anyway by
  // one of the wrapper writeto() methods, making this call cheap.
  printer->print(
    "getserializedsize();\n");

  if (descriptor_->extension_range_count() > 0) {
    if (descriptor_->options().message_set_wire_format()) {
      printer->print(
        "com.google.protobuf.generatedmessage$lite$\n"
        "  .extendablemessage<$classname$>.extensionwriter extensionwriter =\n"
        "    newmessagesetextensionwriter();\n",
        "lite", hasdescriptormethods(descriptor_) ? "" : "lite",
        "classname", classname(descriptor_));
    } else {
      printer->print(
        "com.google.protobuf.generatedmessage$lite$\n"
        "  .extendablemessage<$classname$>.extensionwriter extensionwriter =\n"
        "    newextensionwriter();\n",
        "lite", hasdescriptormethods(descriptor_) ? "" : "lite",
        "classname", classname(descriptor_));
    }
  }

  // merge the fields and the extension ranges, both sorted by field number.
  for (int i = 0, j = 0;
       i < descriptor_->field_count() || j < sorted_extensions.size();
       ) {
    if (i == descriptor_->field_count()) {
      generateserializeoneextensionrange(printer, sorted_extensions[j++]);
    } else if (j == sorted_extensions.size()) {
      generateserializeonefield(printer, sorted_fields[i++]);
    } else if (sorted_fields[i]->number() < sorted_extensions[j]->start) {
      generateserializeonefield(printer, sorted_fields[i++]);
    } else {
      generateserializeoneextensionrange(printer, sorted_extensions[j++]);
    }
  }

  if (hasunknownfields(descriptor_)) {
    if (descriptor_->options().message_set_wire_format()) {
      printer->print(
        "getunknownfields().writeasmessagesetto(output);\n");
    } else {
      printer->print(
        "getunknownfields().writeto(output);\n");
    }
  }

  printer->outdent();
  printer->print(
    "}\n"
    "\n"
    "private int memoizedserializedsize = -1;\n"
    "public int getserializedsize() {\n"
    "  int size = memoizedserializedsize;\n"
    "  if (size != -1) return size;\n"
    "\n"
    "  size = 0;\n");
  printer->indent();

  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(sorted_fields[i]).generateserializedsizecode(printer);
  }

  if (descriptor_->extension_range_count() > 0) {
    if (descriptor_->options().message_set_wire_format()) {
      printer->print(
        "size += extensionsserializedsizeasmessageset();\n");
    } else {
      printer->print(
        "size += extensionsserializedsize();\n");
    }
  }

  if (hasunknownfields(descriptor_)) {
    if (descriptor_->options().message_set_wire_format()) {
      printer->print(
        "size += getunknownfields().getserializedsizeasmessageset();\n");
    } else {
      printer->print(
        "size += getunknownfields().getserializedsize();\n");
    }
  }

  printer->outdent();
  printer->print(
    "  memoizedserializedsize = size;\n"
    "  return size;\n"
    "}\n"
    "\n");

  printer->print(
    "private static final long serialversionuid = 0l;\n"
    "@java.lang.override\n"
    "protected java.lang.object writereplace()\n"
    "    throws java.io.objectstreamexception {\n"
    "  return super.writereplace();\n"
    "}\n"
    "\n");
}

void messagegenerator::
generateparsefrommethods(io::printer* printer) {
  // note:  these are separate from generatemessageserializationmethods()
  //   because they need to be generated even for messages that are optimized
  //   for code size.
  printer->print(
    "public static $classname$ parsefrom(\n"
    "    com.google.protobuf.bytestring data)\n"
    "    throws com.google.protobuf.invalidprotocolbufferexception {\n"
    "  return parser.parsefrom(data);\n"
    "}\n"
    "public static $classname$ parsefrom(\n"
    "    com.google.protobuf.bytestring data,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws com.google.protobuf.invalidprotocolbufferexception {\n"
    "  return parser.parsefrom(data, extensionregistry);\n"
    "}\n"
    "public static $classname$ parsefrom(byte[] data)\n"
    "    throws com.google.protobuf.invalidprotocolbufferexception {\n"
    "  return parser.parsefrom(data);\n"
    "}\n"
    "public static $classname$ parsefrom(\n"
    "    byte[] data,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws com.google.protobuf.invalidprotocolbufferexception {\n"
    "  return parser.parsefrom(data, extensionregistry);\n"
    "}\n"
    "public static $classname$ parsefrom(java.io.inputstream input)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsefrom(input);\n"
    "}\n"
    "public static $classname$ parsefrom(\n"
    "    java.io.inputstream input,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsefrom(input, extensionregistry);\n"
    "}\n"
    "public static $classname$ parsedelimitedfrom(java.io.inputstream input)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsedelimitedfrom(input);\n"
    "}\n"
    "public static $classname$ parsedelimitedfrom(\n"
    "    java.io.inputstream input,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsedelimitedfrom(input, extensionregistry);\n"
    "}\n"
    "public static $classname$ parsefrom(\n"
    "    com.google.protobuf.codedinputstream input)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsefrom(input);\n"
    "}\n"
    "public static $classname$ parsefrom(\n"
    "    com.google.protobuf.codedinputstream input,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws java.io.ioexception {\n"
    "  return parser.parsefrom(input, extensionregistry);\n"
    "}\n"
    "\n",
    "classname", classname(descriptor_));
}

void messagegenerator::generateserializeonefield(
    io::printer* printer, const fielddescriptor* field) {
  field_generators_.get(field).generateserializationcode(printer);
}

void messagegenerator::generateserializeoneextensionrange(
    io::printer* printer, const descriptor::extensionrange* range) {
  printer->print(
    "extensionwriter.writeuntil($end$, output);\n",
    "end", simpleitoa(range->end));
}

// ===================================================================

void messagegenerator::generatebuilder(io::printer* printer) {
  printer->print(
    "public static builder newbuilder() { return builder.create(); }\n"
    "public builder newbuilderfortype() { return newbuilder(); }\n"
    "public static builder newbuilder($classname$ prototype) {\n"
    "  return newbuilder().mergefrom(prototype);\n"
    "}\n"
    "public builder tobuilder() { return newbuilder(this); }\n"
    "\n",
    "classname", classname(descriptor_));

  if (hasnestedbuilders(descriptor_)) {
     printer->print(
      "@java.lang.override\n"
      "protected builder newbuilderfortype(\n"
      "    com.google.protobuf.generatedmessage.builderparent parent) {\n"
      "  builder builder = new builder(parent);\n"
      "  return builder;\n"
      "}\n");
  }

  writemessagedoccomment(printer, descriptor_);

  if (descriptor_->extension_range_count() > 0) {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public static final class builder extends\n"
        "    com.google.protobuf.generatedmessage.extendablebuilder<\n"
        "      $classname$, builder> implements $classname$orbuilder {\n",
        "classname", classname(descriptor_));
    } else {
      printer->print(
        "public static final class builder extends\n"
        "    com.google.protobuf.generatedmessagelite.extendablebuilder<\n"
        "      $classname$, builder> implements $classname$orbuilder {\n",
        "classname", classname(descriptor_));
    }
  } else {
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public static final class builder extends\n"
        "    com.google.protobuf.generatedmessage.builder<builder>\n"
         "   implements $classname$orbuilder {\n",
        "classname", classname(descriptor_));
    } else {
      printer->print(
        "public static final class builder extends\n"
        "    com.google.protobuf.generatedmessagelite.builder<\n"
        "      $classname$, builder>\n"
        "    implements $classname$orbuilder {\n",
        "classname", classname(descriptor_));
    }
  }
  printer->indent();

  generatedescriptormethods(printer);
  generatecommonbuildermethods(printer);

  if (hasgeneratedmethods(descriptor_)) {
    generateisinitialized(printer, dont_memoize);
    generatebuilderparsingmethods(printer);
  }

  // integers for bit fields.
  int totalbits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    totalbits += field_generators_.get(descriptor_->field(i))
        .getnumbitsforbuilder();
  }
  int totalints = (totalbits + 31) / 32;
  for (int i = 0; i < totalints; i++) {
    printer->print("private int $bit_field_name$;\n",
      "bit_field_name", getbitfieldname(i));
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    printer->print("\n");
    printfieldcomment(printer, descriptor_->field(i));
    field_generators_.get(descriptor_->field(i))
                     .generatebuildermembers(printer);
  }

  printer->print(
    "\n"
    "// @@protoc_insertion_point(builder_scope:$full_name$)\n",
    "full_name", descriptor_->full_name());

  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::generatedescriptormethods(io::printer* printer) {
  if (hasdescriptormethods(descriptor_)) {
    if (!descriptor_->options().no_standard_descriptor_accessor()) {
      printer->print(
        "public static final com.google.protobuf.descriptors.descriptor\n"
        "    getdescriptor() {\n"
        "  return $fileclass$.internal_$identifier$_descriptor;\n"
        "}\n"
        "\n",
        "fileclass", classname(descriptor_->file()),
        "identifier", uniquefilescopeidentifier(descriptor_));
    }
    printer->print(
      "protected com.google.protobuf.generatedmessage.fieldaccessortable\n"
      "    internalgetfieldaccessortable() {\n"
      "  return $fileclass$.internal_$identifier$_fieldaccessortable\n"
      "      .ensurefieldaccessorsinitialized(\n"
      "          $classname$.class, $classname$.builder.class);\n"
      "}\n"
      "\n",
      "classname", classname(descriptor_),
      "fileclass", classname(descriptor_->file()),
      "identifier", uniquefilescopeidentifier(descriptor_));
  }
}

// ===================================================================

void messagegenerator::generatecommonbuildermethods(io::printer* printer) {
  printer->print(
    "// construct using $classname$.newbuilder()\n"
    "private builder() {\n"
    "  maybeforcebuilderinitialization();\n"
    "}\n"
    "\n",
    "classname", classname(descriptor_));

  if (hasdescriptormethods(descriptor_)) {
    printer->print(
      "private builder(\n"
      "    com.google.protobuf.generatedmessage.builderparent parent) {\n"
      "  super(parent);\n"
      "  maybeforcebuilderinitialization();\n"
      "}\n",
      "classname", classname(descriptor_));
  }


  if (hasnestedbuilders(descriptor_)) {
    printer->print(
      "private void maybeforcebuilderinitialization() {\n"
      "  if (com.google.protobuf.generatedmessage.alwaysusefieldbuilders) {\n");

    printer->indent();
    printer->indent();
    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(descriptor_->field(i))
          .generatefieldbuilderinitializationcode(printer);
    }
    printer->outdent();
    printer->outdent();

    printer->print(
      "  }\n"
      "}\n");
  } else {
    printer->print(
      "private void maybeforcebuilderinitialization() {\n"
      "}\n");
  }

  printer->print(
    "private static builder create() {\n"
    "  return new builder();\n"
    "}\n"
    "\n"
    "public builder clear() {\n"
    "  super.clear();\n",
    "classname", classname(descriptor_));

  printer->indent();

  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
        .generatebuilderclearcode(printer);
  }

  printer->outdent();

  printer->print(
    "  return this;\n"
    "}\n"
    "\n"
    "public builder clone() {\n"
    "  return create().mergefrom(buildpartial());\n"
    "}\n"
    "\n",
    "classname", classname(descriptor_));
  if (hasdescriptormethods(descriptor_)) {
    printer->print(
      "public com.google.protobuf.descriptors.descriptor\n"
      "    getdescriptorfortype() {\n"
      "  return $fileclass$.internal_$identifier$_descriptor;\n"
      "}\n"
      "\n",
      "fileclass", classname(descriptor_->file()),
      "identifier", uniquefilescopeidentifier(descriptor_));
  }
  printer->print(
    "public $classname$ getdefaultinstancefortype() {\n"
    "  return $classname$.getdefaultinstance();\n"
    "}\n"
    "\n",
    "classname", classname(descriptor_));

  // -----------------------------------------------------------------

  printer->print(
    "public $classname$ build() {\n"
    "  $classname$ result = buildpartial();\n"
    "  if (!result.isinitialized()) {\n"
    "    throw newuninitializedmessageexception(result);\n"
    "  }\n"
    "  return result;\n"
    "}\n"
    "\n"
    "public $classname$ buildpartial() {\n"
    "  $classname$ result = new $classname$(this);\n",
    "classname", classname(descriptor_));

  printer->indent();

  // local vars for from and to bit fields to avoid accessing the builder and
  // message over and over for these fields. seems to provide a slight
  // perforamance improvement in micro benchmark and this is also what proto1
  // code does.
  int totalbuilderbits = 0;
  int totalmessagebits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fieldgenerator& field = field_generators_.get(descriptor_->field(i));
    totalbuilderbits += field.getnumbitsforbuilder();
    totalmessagebits += field.getnumbitsformessage();
  }
  int totalbuilderints = (totalbuilderbits + 31) / 32;
  int totalmessageints = (totalmessagebits + 31) / 32;
  for (int i = 0; i < totalbuilderints; i++) {
    printer->print("int from_$bit_field_name$ = $bit_field_name$;\n",
      "bit_field_name", getbitfieldname(i));
  }
  for (int i = 0; i < totalmessageints; i++) {
    printer->print("int to_$bit_field_name$ = 0;\n",
      "bit_field_name", getbitfieldname(i));
  }

  // output generation code for each field.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i)).generatebuildingcode(printer);
  }

  // copy the bit field results to the generated message
  for (int i = 0; i < totalmessageints; i++) {
    printer->print("result.$bit_field_name$ = to_$bit_field_name$;\n",
      "bit_field_name", getbitfieldname(i));
  }

  printer->outdent();

  if (hasdescriptormethods(descriptor_)) {
    printer->print(
    "  onbuilt();\n");
  }

  printer->print(
    "  return result;\n"
    "}\n"
    "\n",
    "classname", classname(descriptor_));

  // -----------------------------------------------------------------

  if (hasgeneratedmethods(descriptor_)) {
    // mergefrom(message other) requires the ability to distinguish the other
    // messages type by its descriptor.
    if (hasdescriptormethods(descriptor_)) {
      printer->print(
        "public builder mergefrom(com.google.protobuf.message other) {\n"
        "  if (other instanceof $classname$) {\n"
        "    return mergefrom(($classname$)other);\n"
        "  } else {\n"
        "    super.mergefrom(other);\n"
        "    return this;\n"
        "  }\n"
        "}\n"
        "\n",
        "classname", classname(descriptor_));
    }

    printer->print(
      "public builder mergefrom($classname$ other) {\n"
      // optimization:  if other is the default instance, we know none of its
      //   fields are set so we can skip the merge.
      "  if (other == $classname$.getdefaultinstance()) return this;\n",
      "classname", classname(descriptor_));
    printer->indent();

    for (int i = 0; i < descriptor_->field_count(); i++) {
      field_generators_.get(descriptor_->field(i)).generatemergingcode(printer);
    }

    printer->outdent();

    // if message type has extensions
    if (descriptor_->extension_range_count() > 0) {
      printer->print(
        "  this.mergeextensionfields(other);\n");
    }

    if (hasunknownfields(descriptor_)) {
      printer->print(
        "  this.mergeunknownfields(other.getunknownfields());\n");
    }

    printer->print(
      "  return this;\n"
      "}\n"
      "\n");
  }
}

// ===================================================================

void messagegenerator::generatebuilderparsingmethods(io::printer* printer) {
  printer->print(
    "public builder mergefrom(\n"
    "    com.google.protobuf.codedinputstream input,\n"
    "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
    "    throws java.io.ioexception {\n"
    "  $classname$ parsedmessage = null;\n"
    "  try {\n"
    "    parsedmessage = parser.parsepartialfrom(input, extensionregistry);\n"
    "  } catch (com.google.protobuf.invalidprotocolbufferexception e) {\n"
    "    parsedmessage = ($classname$) e.getunfinishedmessage();\n"
    "    throw e;\n"
    "  } finally {\n"
    "    if (parsedmessage != null) {\n"
    "      mergefrom(parsedmessage);\n"
    "    }\n"
    "  }\n"
    "  return this;\n"
    "}\n",
    "classname", classname(descriptor_));
}

// ===================================================================

void messagegenerator::generateisinitialized(
    io::printer* printer, usememoization usememoization) {
  bool memoization = usememoization == memoize;
  if (memoization) {
    // memoizes whether the protocol buffer is fully initialized (has all
    // required fields). -1 means not yet computed. 0 means false and 1 means
    // true.
    printer->print(
      "private byte memoizedisinitialized = -1;\n");
  }
  printer->print(
    "public final boolean isinitialized() {\n");
  printer->indent();

  if (memoization) {
    printer->print(
      "byte isinitialized = memoizedisinitialized;\n"
      "if (isinitialized != -1) return isinitialized == 1;\n"
      "\n");
  }

  // check that all required fields in this message are set.
  // todo(kenton):  we can optimize this when we switch to putting all the
  //   "has" fields into a single bitfield.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (field->is_required()) {
      printer->print(
        "if (!has$name$()) {\n"
        "  $memoize$\n"
        "  return false;\n"
        "}\n",
        "name", underscorestocapitalizedcamelcase(field),
        "memoize", memoization ? "memoizedisinitialized = 0;" : "");
    }
  }

  // now check that all embedded messages are initialized.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (getjavatype(field) == javatype_message &&
        hasrequiredfields(field->message_type())) {
      switch (field->label()) {
        case fielddescriptor::label_required:
          printer->print(
            "if (!get$name$().isinitialized()) {\n"
             "  $memoize$\n"
             "  return false;\n"
             "}\n",
            "type", classname(field->message_type()),
            "name", underscorestocapitalizedcamelcase(field),
            "memoize", memoization ? "memoizedisinitialized = 0;" : "");
          break;
        case fielddescriptor::label_optional:
          printer->print(
            "if (has$name$()) {\n"
            "  if (!get$name$().isinitialized()) {\n"
            "    $memoize$\n"
            "    return false;\n"
            "  }\n"
            "}\n",
            "type", classname(field->message_type()),
            "name", underscorestocapitalizedcamelcase(field),
            "memoize", memoization ? "memoizedisinitialized = 0;" : "");
          break;
        case fielddescriptor::label_repeated:
          printer->print(
            "for (int i = 0; i < get$name$count(); i++) {\n"
            "  if (!get$name$(i).isinitialized()) {\n"
            "    $memoize$\n"
            "    return false;\n"
            "  }\n"
            "}\n",
            "type", classname(field->message_type()),
            "name", underscorestocapitalizedcamelcase(field),
            "memoize", memoization ? "memoizedisinitialized = 0;" : "");
          break;
      }
    }
  }

  if (descriptor_->extension_range_count() > 0) {
    printer->print(
      "if (!extensionsareinitialized()) {\n"
      "  $memoize$\n"
      "  return false;\n"
      "}\n",
      "memoize", memoization ? "memoizedisinitialized = 0;" : "");
  }

  printer->outdent();

  if (memoization) {
    printer->print(
      "  memoizedisinitialized = 1;\n");
  }

  printer->print(
    "  return true;\n"
    "}\n"
    "\n");
}

// ===================================================================

void messagegenerator::generateequalsandhashcode(io::printer* printer) {
  printer->print(
    "@java.lang.override\n"
    "public boolean equals(final java.lang.object obj) {\n");
  printer->indent();
  printer->print(
    "if (obj == this) {\n"
    " return true;\n"
    "}\n"
    "if (!(obj instanceof $classname$)) {\n"
    "  return super.equals(obj);\n"
    "}\n"
    "$classname$ other = ($classname$) obj;\n"
    "\n",
    "classname", classname(descriptor_));

  printer->print("boolean result = true;\n");
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (!field->is_repeated()) {
      printer->print(
        "result = result && (has$name$() == other.has$name$());\n"
        "if (has$name$()) {\n",
        "name", underscorestocapitalizedcamelcase(field));
      printer->indent();
    }
    field_generators_.get(field).generateequalscode(printer);
    if (!field->is_repeated()) {
      printer->outdent();
      printer->print(
        "}\n");
    }
  }
  if (hasdescriptormethods(descriptor_)) {
    printer->print(
      "result = result &&\n"
      "    getunknownfields().equals(other.getunknownfields());\n");
    if (descriptor_->extension_range_count() > 0) {
      printer->print(
        "result = result &&\n"
        "    getextensionfields().equals(other.getextensionfields());\n");
    }
  }
  printer->print(
    "return result;\n");
  printer->outdent();
  printer->print(
    "}\n"
    "\n");

  printer->print(
    "private int memoizedhashcode = 0;\n");
  printer->print(
    "@java.lang.override\n"
    "public int hashcode() {\n");
  printer->indent();
  printer->print(
    "if (memoizedhashcode != 0) {\n");
  printer->indent();
  printer->print(
    "return memoizedhashcode;\n");
  printer->outdent();
  printer->print(
    "}\n"
    "int hash = 41;\n"
    "hash = (19 * hash) + getdescriptorfortype().hashcode();\n");
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (!field->is_repeated()) {
      printer->print(
        "if (has$name$()) {\n",
        "name", underscorestocapitalizedcamelcase(field));
      printer->indent();
    }
    field_generators_.get(field).generatehashcode(printer);
    if (!field->is_repeated()) {
      printer->outdent();
      printer->print("}\n");
    }
  }
  if (hasdescriptormethods(descriptor_)) {
    if (descriptor_->extension_range_count() > 0) {
      printer->print(
        "hash = hashfields(hash, getextensionfields());\n");
    }
  }
  printer->print(
    "hash = (29 * hash) + getunknownfields().hashcode();\n"
    "memoizedhashcode = hash;\n"
    "return hash;\n");
  printer->outdent();
  printer->print(
    "}\n"
    "\n");
}

// ===================================================================

void messagegenerator::generateextensionregistrationcode(io::printer* printer) {
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extensiongenerator(descriptor_->extension(i))
      .generateregistrationcode(printer);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    messagegenerator(descriptor_->nested_type(i))
      .generateextensionregistrationcode(printer);
  }
}

// ===================================================================
void messagegenerator::generateparsingconstructor(io::printer* printer) {
  scoped_array<const fielddescriptor*> sorted_fields(
      sortfieldsbynumber(descriptor_));

  printer->print(
      "private $classname$(\n"
      "    com.google.protobuf.codedinputstream input,\n"
      "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
      "    throws com.google.protobuf.invalidprotocolbufferexception {\n",
      "classname", descriptor_->name());
  printer->indent();

  // initialize all fields to default.
  printer->print(
      "initfields();\n");

  // use builder bits to track mutable repeated fields.
  int totalbuilderbits = 0;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fieldgenerator& field = field_generators_.get(descriptor_->field(i));
    totalbuilderbits += field.getnumbitsforbuilder();
  }
  int totalbuilderints = (totalbuilderbits + 31) / 32;
  for (int i = 0; i < totalbuilderints; i++) {
    printer->print("int mutable_$bit_field_name$ = 0;\n",
      "bit_field_name", getbitfieldname(i));
  }

  if (hasunknownfields(descriptor_)) {
    printer->print(
      "com.google.protobuf.unknownfieldset.builder unknownfields =\n"
      "    com.google.protobuf.unknownfieldset.newbuilder();\n");
  }

  printer->print(
      "try {\n");
  printer->indent();

  printer->print(
    "boolean done = false;\n"
    "while (!done) {\n");
  printer->indent();

  printer->print(
    "int tag = input.readtag();\n"
    "switch (tag) {\n");
  printer->indent();

  printer->print(
    "case 0:\n"          // zero signals eof / limit reached
    "  done = true;\n"
    "  break;\n"
    "default: {\n"
    "  if (!parseunknownfield(input,$unknown_fields$\n"
    "                         extensionregistry, tag)) {\n"
    "    done = true;\n"  // it's an endgroup tag
    "  }\n"
    "  break;\n"
    "}\n",
    "unknown_fields", hasunknownfields(descriptor_)
        ? " unknownfields," : "");

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = sorted_fields[i];
    uint32 tag = wireformatlite::maketag(field->number(),
      wireformat::wiretypeforfieldtype(field->type()));

    printer->print(
      "case $tag$: {\n",
      "tag", simpleitoa(tag));
    printer->indent();

    field_generators_.get(field).generateparsingcode(printer);

    printer->outdent();
    printer->print(
      "  break;\n"
      "}\n");

    if (field->is_packable()) {
      // to make packed = true wire compatible, we generate parsing code from a
      // packed version of this field regardless of field->options().packed().
      uint32 packed_tag = wireformatlite::maketag(field->number(),
        wireformatlite::wiretype_length_delimited);
      printer->print(
        "case $tag$: {\n",
        "tag", simpleitoa(packed_tag));
      printer->indent();

      field_generators_.get(field).generateparsingcodefrompacked(printer);

      printer->outdent();
      printer->print(
        "  break;\n"
        "}\n");
    }
  }

  printer->outdent();
  printer->outdent();
  printer->print(
      "  }\n"     // switch (tag)
      "}\n");     // while (!done)

  printer->outdent();
  printer->print(
      "} catch (com.google.protobuf.invalidprotocolbufferexception e) {\n"
      "  throw e.setunfinishedmessage(this);\n"
      "} catch (java.io.ioexception e) {\n"
      "  throw new com.google.protobuf.invalidprotocolbufferexception(\n"
      "      e.getmessage()).setunfinishedmessage(this);\n"
      "} finally {\n");
  printer->indent();

  // make repeated field list immutable.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = sorted_fields[i];
    field_generators_.get(field).generateparsingdonecode(printer);
  }

  // make unknown fields immutable.
  if (hasunknownfields(descriptor_)) {
    printer->print(
        "this.unknownfields = unknownfields.build();\n");
  }

  // make extensions immutable.
  printer->print(
      "makeextensionsimmutable();\n");

  printer->outdent();
  printer->outdent();
  printer->print(
      "  }\n"     // finally
      "}\n");
}

// ===================================================================
void messagegenerator::generateparser(io::printer* printer) {
  printer->print(
      "public static com.google.protobuf.parser<$classname$> parser =\n"
      "    new com.google.protobuf.abstractparser<$classname$>() {\n",
      "classname", descriptor_->name());
  printer->indent();
  printer->print(
      "public $classname$ parsepartialfrom(\n"
      "    com.google.protobuf.codedinputstream input,\n"
      "    com.google.protobuf.extensionregistrylite extensionregistry)\n"
      "    throws com.google.protobuf.invalidprotocolbufferexception {\n",
      "classname", descriptor_->name());
  if (hasgeneratedmethods(descriptor_)) {
    printer->print(
        "  return new $classname$(input, extensionregistry);\n",
        "classname", descriptor_->name());
  } else {
    // when parsing constructor isn't generated, use builder to parse messages.
    // note, will fallback to use reflection based mergefieldfrom() in
    // abstractmessage.builder.
    printer->indent();
    printer->print(
        "builder builder = newbuilder();\n"
        "try {\n"
        "  builder.mergefrom(input, extensionregistry);\n"
        "} catch (com.google.protobuf.invalidprotocolbufferexception e) {\n"
        "  throw e.setunfinishedmessage(builder.buildpartial());\n"
        "} catch (java.io.ioexception e) {\n"
        "  throw new com.google.protobuf.invalidprotocolbufferexception(\n"
        "      e.getmessage()).setunfinishedmessage(builder.buildpartial());\n"
        "}\n"
        "return builder.buildpartial();\n");
    printer->outdent();
  }
  printer->print(
        "}\n");
  printer->outdent();
  printer->print(
      "};\n"
      "\n");

  printer->print(
      "@java.lang.override\n"
      "public com.google.protobuf.parser<$classname$> getparserfortype() {\n"
      "  return parser;\n"
      "}\n"
      "\n",
      "classname", descriptor_->name());
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
