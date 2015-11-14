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

#include <google/protobuf/compiler/java/java_file.h>
#include <google/protobuf/compiler/java/java_enum.h>
#include <google/protobuf/compiler/java/java_service.h>
#include <google/protobuf/compiler/java/java_extension.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_message.h>
#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/dynamic_message.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

namespace {


// recursively searches the given message to collect extensions.
// returns true if all the extensions can be recognized. the extensions will be
// appended in to the extensions parameter.
// returns false when there are unknown fields, in which case the data in the
// extensions output parameter is not reliable and should be discarded.
bool collectextensions(const message& message,
                       vector<const fielddescriptor*>* extensions) {
  const reflection* reflection = message.getreflection();

  // there are unknown fields that could be extensions, thus this call fails.
  if (reflection->getunknownfields(message).field_count() > 0) return false;

  vector<const fielddescriptor*> fields;
  reflection->listfields(message, &fields);

  for (int i = 0; i < fields.size(); i++) {
    if (fields[i]->is_extension()) extensions->push_back(fields[i]);

    if (getjavatype(fields[i]) == javatype_message) {
      if (fields[i]->is_repeated()) {
        int size = reflection->fieldsize(message, fields[i]);
        for (int j = 0; j < size; j++) {
          const message& sub_message =
            reflection->getrepeatedmessage(message, fields[i], j);
          if (!collectextensions(sub_message, extensions)) return false;
        }
      } else {
        const message& sub_message = reflection->getmessage(message, fields[i]);
        if (!collectextensions(sub_message, extensions)) return false;
      }
    }
  }

  return true;
}

// finds all extensions in the given message and its sub-messages.  if the
// message contains unknown fields (which could be extensions), then those
// extensions are defined in alternate_pool.
// the message will be converted to a dynamicmessage backed by alternate_pool
// in order to handle this case.
void collectextensions(const filedescriptorproto& file_proto,
                       const descriptorpool& alternate_pool,
                       vector<const fielddescriptor*>* extensions,
                       const string& file_data) {
  if (!collectextensions(file_proto, extensions)) {
    // there are unknown fields in the file_proto, which are probably
    // extensions. we need to parse the data into a dynamic message based on the
    // builder-pool to find out all extensions.
    const descriptor* file_proto_desc = alternate_pool.findmessagetypebyname(
        file_proto.getdescriptor()->full_name());
    google_check(file_proto_desc)
        << "find unknown fields in filedescriptorproto when building "
        << file_proto.name()
        << ". it's likely that those fields are custom options, however, "
           "descriptor.proto is not in the transitive dependencies. "
           "this normally should not happen. please report a bug.";
    dynamicmessagefactory factory;
    scoped_ptr<message> dynamic_file_proto(
        factory.getprototype(file_proto_desc)->new());
    google_check(dynamic_file_proto.get() != null);
    google_check(dynamic_file_proto->parsefromstring(file_data));

    // collect the extensions again from the dynamic message. there should be no
    // more unknown fields this time, i.e. all the custom options should be
    // parsed as extensions now.
    extensions->clear();
    google_check(collectextensions(*dynamic_file_proto, extensions))
        << "find unknown fields in filedescriptorproto when building "
        << file_proto.name()
        << ". it's likely that those fields are custom options, however, "
           "those options cannot be recognized in the builder pool. "
           "this normally should not happen. please report a bug.";
  }
}


}  // namespace

filegenerator::filegenerator(const filedescriptor* file)
  : file_(file),
    java_package_(filejavapackage(file)),
    classname_(fileclassname(file)) {
}

filegenerator::~filegenerator() {}

bool filegenerator::validate(string* error) {
  // check that no class name matches the file's class name.  this is a common
  // problem that leads to java compile errors that can be hard to understand.
  // it's especially bad when using the java_multiple_files, since we would
  // end up overwriting the outer class with one of the inner ones.

  bool found_conflict = false;
  for (int i = 0; i < file_->enum_type_count() && !found_conflict; i++) {
    if (file_->enum_type(i)->name() == classname_) {
      found_conflict = true;
    }
  }
  for (int i = 0; i < file_->message_type_count() && !found_conflict; i++) {
    if (file_->message_type(i)->name() == classname_) {
      found_conflict = true;
    }
  }
  for (int i = 0; i < file_->service_count() && !found_conflict; i++) {
    if (file_->service(i)->name() == classname_) {
      found_conflict = true;
    }
  }

  if (found_conflict) {
    error->assign(file_->name());
    error->append(
      ": cannot generate java output because the file's outer class name, \"");
    error->append(classname_);
    error->append(
      "\", matches the name of one of the types declared inside it.  "
      "please either rename the type or use the java_outer_classname "
      "option to specify a different outer class name for the .proto file.");
    return false;
  }

  return true;
}

void filegenerator::generate(io::printer* printer) {
  // we don't import anything because we refer to all classes by their
  // fully-qualified names in the generated source.
  printer->print(
    "// generated by the protocol buffer compiler.  do not edit!\n"
    "// source: $filename$\n"
    "\n",
    "filename", file_->name());
  if (!java_package_.empty()) {
    printer->print(
      "package $package$;\n"
      "\n",
      "package", java_package_);
  }
  printer->print(
    "public final class $classname$ {\n"
    "  private $classname$() {}\n",
    "classname", classname_);
  printer->indent();

  // -----------------------------------------------------------------

  printer->print(
    "public static void registerallextensions(\n"
    "    com.google.protobuf.extensionregistry$lite$ registry) {\n",
    "lite", hasdescriptormethods(file_) ? "" : "lite");

  printer->indent();

  for (int i = 0; i < file_->extension_count(); i++) {
    extensiongenerator(file_->extension(i)).generateregistrationcode(printer);
  }

  for (int i = 0; i < file_->message_type_count(); i++) {
    messagegenerator(file_->message_type(i))
      .generateextensionregistrationcode(printer);
  }

  printer->outdent();
  printer->print(
    "}\n");

  // -----------------------------------------------------------------

  if (!file_->options().java_multiple_files()) {
    for (int i = 0; i < file_->enum_type_count(); i++) {
      enumgenerator(file_->enum_type(i)).generate(printer);
    }
    for (int i = 0; i < file_->message_type_count(); i++) {
      messagegenerator messagegenerator(file_->message_type(i));
      messagegenerator.generateinterface(printer);
      messagegenerator.generate(printer);
    }
    if (hasgenericservices(file_)) {
      for (int i = 0; i < file_->service_count(); i++) {
        servicegenerator(file_->service(i)).generate(printer);
      }
    }
  }

  // extensions must be generated in the outer class since they are values,
  // not classes.
  for (int i = 0; i < file_->extension_count(); i++) {
    extensiongenerator(file_->extension(i)).generate(printer);
  }

  // static variables.
  for (int i = 0; i < file_->message_type_count(); i++) {
    // todo(kenton):  reuse messagegenerator objects?
    messagegenerator(file_->message_type(i)).generatestaticvariables(printer);
  }

  printer->print("\n");

  if (hasdescriptormethods(file_)) {
    generateembeddeddescriptor(printer);
  } else {
    printer->print(
      "static {\n");
    printer->indent();

    for (int i = 0; i < file_->message_type_count(); i++) {
      // todo(kenton):  reuse messagegenerator objects?
      messagegenerator(file_->message_type(i))
        .generatestaticvariableinitializers(printer);
    }

    printer->outdent();
    printer->print(
      "}\n");
  }

  printer->print(
    "\n"
    "// @@protoc_insertion_point(outer_class_scope)\n");

  printer->outdent();
  printer->print("}\n");
}

void filegenerator::generateembeddeddescriptor(io::printer* printer) {
  // embed the descriptor.  we simply serialize the entire filedescriptorproto
  // and embed it as a string literal, which is parsed and built into real
  // descriptors at initialization time.  we unfortunately have to put it in
  // a string literal, not a byte array, because apparently using a literal
  // byte array causes the java compiler to generate *instructions* to
  // initialize each and every byte of the array, e.g. as if you typed:
  //   b[0] = 123; b[1] = 456; b[2] = 789;
  // this makes huge bytecode files and can easily hit the compiler's internal
  // code size limits (error "code to large").  string literals are apparently
  // embedded raw, which is what we want.
  filedescriptorproto file_proto;
  file_->copyto(&file_proto);

  string file_data;
  file_proto.serializetostring(&file_data);

  printer->print(
    "public static com.google.protobuf.descriptors.filedescriptor\n"
    "    getdescriptor() {\n"
    "  return descriptor;\n"
    "}\n"
    "private static com.google.protobuf.descriptors.filedescriptor\n"
    "    descriptor;\n"
    "static {\n"
    "  java.lang.string[] descriptordata = {\n");
  printer->indent();
  printer->indent();

  // only write 40 bytes per line.
  static const int kbytesperline = 40;
  for (int i = 0; i < file_data.size(); i += kbytesperline) {
    if (i > 0) {
      // every 400 lines, start a new string literal, in order to avoid the
      // 64k length limit.
      if (i % 400 == 0) {
        printer->print(",\n");
      } else {
        printer->print(" +\n");
      }
    }
    printer->print("\"$data$\"",
      "data", cescape(file_data.substr(i, kbytesperline)));
  }

  printer->outdent();
  printer->print("\n};\n");

  // -----------------------------------------------------------------
  // create the internaldescriptorassigner.

  printer->print(
    "com.google.protobuf.descriptors.filedescriptor."
      "internaldescriptorassigner assigner =\n"
    "  new com.google.protobuf.descriptors.filedescriptor."
      "internaldescriptorassigner() {\n"
    "    public com.google.protobuf.extensionregistry assigndescriptors(\n"
    "        com.google.protobuf.descriptors.filedescriptor root) {\n"
    "      descriptor = root;\n");

  printer->indent();
  printer->indent();
  printer->indent();

  for (int i = 0; i < file_->message_type_count(); i++) {
    // todo(kenton):  reuse messagegenerator objects?
    messagegenerator(file_->message_type(i))
      .generatestaticvariableinitializers(printer);
  }
  for (int i = 0; i < file_->extension_count(); i++) {
    // todo(kenton):  reuse extensiongenerator objects?
    extensiongenerator(file_->extension(i))
        .generatenonnestedinitializationcode(printer);
  }

  // proto compiler builds a descriptorpool, which holds all the descriptors to
  // generate, when processing the ".proto" files. we call this descriptorpool
  // the parsed pool (a.k.a. file_->pool()).
  //
  // note that when users try to extend the (.*)descriptorproto in their
  // ".proto" files, it does not affect the pre-built filedescriptorproto class
  // in proto compiler. when we put the descriptor data in the file_proto, those
  // extensions become unknown fields.
  //
  // now we need to find out all the extension value to the (.*)descriptorproto
  // in the file_proto message, and prepare an extensionregistry to return.
  //
  // to find those extensions, we need to parse the data into a dynamic message
  // of the filedescriptor based on the builder-pool, then we can use
  // reflections to find all extension fields
  vector<const fielddescriptor*> extensions;
  collectextensions(file_proto, *file_->pool(), &extensions, file_data);

  if (extensions.size() > 0) {
    // must construct an extensionregistry containing all existing extensions
    // and return it.
    printer->print(
      "com.google.protobuf.extensionregistry registry =\n"
      "  com.google.protobuf.extensionregistry.newinstance();\n");
    for (int i = 0; i < extensions.size(); i++) {
      extensiongenerator(extensions[i]).generateregistrationcode(printer);
    }
    printer->print(
      "return registry;\n");
  } else {
    printer->print(
      "return null;\n");
  }

  printer->outdent();
  printer->outdent();
  printer->outdent();

  printer->print(
    "    }\n"
    "  };\n");

  // -----------------------------------------------------------------
  // invoke internalbuildgeneratedfilefrom() to build the file.

  printer->print(
    "com.google.protobuf.descriptors.filedescriptor\n"
    "  .internalbuildgeneratedfilefrom(descriptordata,\n"
    "    new com.google.protobuf.descriptors.filedescriptor[] {\n");

  for (int i = 0; i < file_->dependency_count(); i++) {
    if (shouldincludedependency(file_->dependency(i))) {
      printer->print(
        "      $dependency$.getdescriptor(),\n",
        "dependency", classname(file_->dependency(i)));
    }
  }

  printer->print(
    "    }, assigner);\n");

  printer->outdent();
  printer->print(
    "}\n");
}

template<typename generatorclass, typename descriptorclass>
static void generatesibling(const string& package_dir,
                            const string& java_package,
                            const descriptorclass* descriptor,
                            generatorcontext* context,
                            vector<string>* file_list,
                            const string& name_suffix,
                            void (generatorclass::*pfn)(io::printer* printer)) {
  string filename = package_dir + descriptor->name() + name_suffix + ".java";
  file_list->push_back(filename);

  scoped_ptr<io::zerocopyoutputstream> output(context->open(filename));
  io::printer printer(output.get(), '$');

  printer.print(
    "// generated by the protocol buffer compiler.  do not edit!\n"
    "// source: $filename$\n"
    "\n",
    "filename", descriptor->file()->name());
  if (!java_package.empty()) {
    printer.print(
      "package $package$;\n"
      "\n",
      "package", java_package);
  }

  generatorclass generator(descriptor);
  (generator.*pfn)(&printer);
}

void filegenerator::generatesiblings(const string& package_dir,
                                     generatorcontext* context,
                                     vector<string>* file_list) {
  if (file_->options().java_multiple_files()) {
    for (int i = 0; i < file_->enum_type_count(); i++) {
      generatesibling<enumgenerator>(package_dir, java_package_,
                                     file_->enum_type(i),
                                     context, file_list, "",
                                     &enumgenerator::generate);
    }
    for (int i = 0; i < file_->message_type_count(); i++) {
      generatesibling<messagegenerator>(package_dir, java_package_,
                                        file_->message_type(i),
                                        context, file_list, "orbuilder",
                                        &messagegenerator::generateinterface);
      generatesibling<messagegenerator>(package_dir, java_package_,
                                        file_->message_type(i),
                                        context, file_list, "",
                                        &messagegenerator::generate);
    }
    if (hasgenericservices(file_)) {
      for (int i = 0; i < file_->service_count(); i++) {
        generatesibling<servicegenerator>(package_dir, java_package_,
                                          file_->service(i),
                                          context, file_list, "",
                                          &servicegenerator::generate);
      }
    }
  }
}

bool filegenerator::shouldincludedependency(const filedescriptor* descriptor) {
  return true;
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
