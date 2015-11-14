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

// author: robinson@google.com (will robinson)
//
// this module outputs pure-python protocol message classes that will
// largely be constructed at runtime via the metaclass in reflection.py.
// in other words, our job is basically to output a python equivalent
// of the c++ *descriptor objects, and fix up all circular references
// within these objects.
//
// note that the runtime performance of protocol message classes created in
// this way is expected to be lousy.  the plan is to create an alternate
// generator that outputs a python/c extension module that lets
// performance-minded python code leverage the fast c++ implementation
// directly.

#include <limits>
#include <map>
#include <utility>
#include <string>
#include <vector>

#include <google/protobuf/compiler/python/python_generator.h>
#include <google/protobuf/descriptor.pb.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace python {

namespace {

// returns a copy of |filename| with any trailing ".protodevel" or ".proto
// suffix stripped.
// todo(robinson): unify with copy in compiler/cpp/internal/helpers.cc.
string stripproto(const string& filename) {
  const char* suffix = hassuffixstring(filename, ".protodevel")
      ? ".protodevel" : ".proto";
  return stripsuffixstring(filename, suffix);
}


// returns the python module name expected for a given .proto filename.
string modulename(const string& filename) {
  string basename = stripproto(filename);
  stripstring(&basename, "-", '_');
  stripstring(&basename, "/", '.');
  return basename + "_pb2";
}


// returns the name of all containing types for descriptor,
// in order from outermost to innermost, followed by descriptor's
// own name.  each name is separated by |separator|.
template <typename descriptort>
string nameprefixedwithnestedtypes(const descriptort& descriptor,
                                   const string& separator) {
  string name = descriptor.name();
  for (const descriptor* current = descriptor.containing_type();
       current != null; current = current->containing_type()) {
    name = current->name() + separator + name;
  }
  return name;
}


// name of the class attribute where we store the python
// descriptor.descriptor instance for the generated class.
// must stay consistent with the _descriptor_key constant
// in proto2/public/reflection.py.
const char kdescriptorkey[] = "descriptor";


// does the file have top-level enums?
inline bool hastoplevelenums(const filedescriptor *file) {
  return file->enum_type_count() > 0;
}


// should we generate generic services for this file?
inline bool hasgenericservices(const filedescriptor *file) {
  return file->service_count() > 0 &&
         file->options().py_generic_services();
}


// prints the common boilerplate needed at the top of every .py
// file output by this generator.
void printtopboilerplate(
    io::printer* printer, const filedescriptor* file, bool descriptor_proto) {
  // todo(robinson): allow parameterization of python version?
  printer->print(
      "# generated by the protocol buffer compiler.  do not edit!\n"
      "# source: $filename$\n"
      "\n",
      "filename", file->name());
  if (hastoplevelenums(file)) {
    printer->print(
        "from google.protobuf.internal import enum_type_wrapper\n");
  }
  printer->print(
      "from google.protobuf import descriptor as _descriptor\n"
      "from google.protobuf import message as _message\n"
      "from google.protobuf import reflection as _reflection\n"
      );
  if (hasgenericservices(file)) {
    printer->print(
        "from google.protobuf import service as _service\n"
        "from google.protobuf import service_reflection\n");
  }

  // avoid circular imports if this module is descriptor_pb2.
  if (!descriptor_proto) {
    printer->print(
        "from google.protobuf import descriptor_pb2\n");
  }
  printer->print(
    "# @@protoc_insertion_point(imports)\n");
  printer->print("\n\n");
}


// returns a python literal giving the default value for a field.
// if the field specifies no explicit default value, we'll return
// the default default value for the field type (zero for numbers,
// empty string for strings, empty list for repeated fields, and
// none for non-repeated, composite fields).
//
// todo(robinson): unify with code from
// //compiler/cpp/internal/primitive_field.cc
// //compiler/cpp/internal/enum_field.cc
// //compiler/cpp/internal/string_field.cc
string stringifydefaultvalue(const fielddescriptor& field) {
  if (field.is_repeated()) {
    return "[]";
  }

  switch (field.cpp_type()) {
    case fielddescriptor::cpptype_int32:
      return simpleitoa(field.default_value_int32());
    case fielddescriptor::cpptype_uint32:
      return simpleitoa(field.default_value_uint32());
    case fielddescriptor::cpptype_int64:
      return simpleitoa(field.default_value_int64());
    case fielddescriptor::cpptype_uint64:
      return simpleitoa(field.default_value_uint64());
    case fielddescriptor::cpptype_double: {
      double value = field.default_value_double();
      if (value == numeric_limits<double>::infinity()) {
        // python pre-2.6 on windows does not parse "inf" correctly.  however,
        // a numeric literal that is too big for a double will become infinity.
        return "1e10000";
      } else if (value == -numeric_limits<double>::infinity()) {
        // see above.
        return "-1e10000";
      } else if (value != value) {
        // infinity * 0 = nan
        return "(1e10000 * 0)";
      } else {
        return simpledtoa(value);
      }
    }
    case fielddescriptor::cpptype_float: {
      float value = field.default_value_float();
      if (value == numeric_limits<float>::infinity()) {
        // python pre-2.6 on windows does not parse "inf" correctly.  however,
        // a numeric literal that is too big for a double will become infinity.
        return "1e10000";
      } else if (value == -numeric_limits<float>::infinity()) {
        // see above.
        return "-1e10000";
      } else if (value != value) {
        // infinity - infinity = nan
        return "(1e10000 * 0)";
      } else {
        return simpleftoa(value);
      }
    }
    case fielddescriptor::cpptype_bool:
      return field.default_value_bool() ? "true" : "false";
    case fielddescriptor::cpptype_enum:
      return simpleitoa(field.default_value_enum()->number());
    case fielddescriptor::cpptype_string:
      if (field.type() == fielddescriptor::type_string) {
        return "unicode(\"" + cescape(field.default_value_string()) +
                        "\", \"utf-8\")";
      } else {
        return "\"" + cescape(field.default_value_string()) + "\"";
      }
      case fielddescriptor::cpptype_message:
          return "none";
  }
  // (we could add a default case above but then we wouldn't get the nice
  // compiler warning when a new type is added.)
  google_log(fatal) << "not reached.";
  return "";
}



}  // namespace


generator::generator() : file_(null) {
}

generator::~generator() {
}

bool generator::generate(const filedescriptor* file,
                         const string& parameter,
                         generatorcontext* context,
                         string* error) const {

  // completely serialize all generate() calls on this instance.  the
  // thread-safety constraints of the codegenerator interface aren't clear so
  // just be as conservative as possible.  it's easier to relax this later if
  // we need to, but i doubt it will be an issue.
  // todo(kenton):  the proper thing to do would be to allocate any state on
  //   the stack and use that, so that the generator class itself does not need
  //   to have any mutable members.  then it is implicitly thread-safe.
  mutexlock lock(&mutex_);
  file_ = file;
  string module_name = modulename(file->name());
  string filename = module_name;
  stripstring(&filename, ".", '/');
  filename += ".py";

  filedescriptorproto fdp;
  file_->copyto(&fdp);
  fdp.serializetostring(&file_descriptor_serialized_);


  scoped_ptr<io::zerocopyoutputstream> output(context->open(filename));
  google_check(output.get());
  io::printer printer(output.get(), '$');
  printer_ = &printer;

  printtopboilerplate(printer_, file_, generatingdescriptorproto());
  printimports();
  printfiledescriptor();
  printtoplevelenums();
  printtoplevelextensions();
  printallnestedenumsinfile();
  printmessagedescriptors();
  fixforeignfieldsindescriptors();
  printmessages();
  // we have to fix up the extensions after the message classes themselves,
  // since they need to call static registerextension() methods on these
  // classes.
  fixforeignfieldsinextensions();
  // descriptor options may have custom extensions. these custom options
  // can only be successfully parsed after we register corresponding
  // extensions. therefore we parse all options again here to recognize
  // custom options that may be unknown when we define the descriptors.
  fixalldescriptoroptions();
  if (hasgenericservices(file)) {
    printservices();
  }

  printer.print(
    "# @@protoc_insertion_point(module_scope)\n");

  return !printer.failed();
}

// prints python imports for all modules imported by |file|.
void generator::printimports() const {
  for (int i = 0; i < file_->dependency_count(); ++i) {
    string module_name = modulename(file_->dependency(i)->name());
    printer_->print("import $module$\n", "module",
                    module_name);
  }
  printer_->print("\n");

  // print public imports.
  for (int i = 0; i < file_->public_dependency_count(); ++i) {
    string module_name = modulename(file_->public_dependency(i)->name());
    printer_->print("from $module$ import *\n", "module", module_name);
  }
  printer_->print("\n");
}

// prints the single file descriptor for this file.
void generator::printfiledescriptor() const {
  map<string, string> m;
  m["descriptor_name"] = kdescriptorkey;
  m["name"] = file_->name();
  m["package"] = file_->package();
  const char file_descriptor_template[] =
      "$descriptor_name$ = _descriptor.filedescriptor(\n"
      "  name='$name$',\n"
      "  package='$package$',\n";
  printer_->print(m, file_descriptor_template);
  printer_->indent();
  printer_->print(
      "serialized_pb='$value$'",
      "value", strings::chexescape(file_descriptor_serialized_));

  // todo(falk): also print options and fix the message_type, enum_type,
  //             service and extension later in the generation.

  printer_->outdent();
  printer_->print(")\n");
  printer_->print("\n");
}

// prints descriptors and module-level constants for all top-level
// enums defined in |file|.
void generator::printtoplevelenums() const {
  vector<pair<string, int> > top_level_enum_values;
  for (int i = 0; i < file_->enum_type_count(); ++i) {
    const enumdescriptor& enum_descriptor = *file_->enum_type(i);
    printenum(enum_descriptor);
    printer_->print("$name$ = "
                    "enum_type_wrapper.enumtypewrapper($descriptor_name$)",
                    "name", enum_descriptor.name(),
                    "descriptor_name",
                    moduleleveldescriptorname(enum_descriptor));
    printer_->print("\n");

    for (int j = 0; j < enum_descriptor.value_count(); ++j) {
      const enumvaluedescriptor& value_descriptor = *enum_descriptor.value(j);
      top_level_enum_values.push_back(
          make_pair(value_descriptor.name(), value_descriptor.number()));
    }
  }

  for (int i = 0; i < top_level_enum_values.size(); ++i) {
    printer_->print("$name$ = $value$\n",
                    "name", top_level_enum_values[i].first,
                    "value", simpleitoa(top_level_enum_values[i].second));
  }
  printer_->print("\n");
}

// prints all enums contained in all message types in |file|.
void generator::printallnestedenumsinfile() const {
  for (int i = 0; i < file_->message_type_count(); ++i) {
    printnestedenums(*file_->message_type(i));
  }
}

// prints a python statement assigning the appropriate module-level
// enum name to a python enumdescriptor object equivalent to
// enum_descriptor.
void generator::printenum(const enumdescriptor& enum_descriptor) const {
  map<string, string> m;
  m["descriptor_name"] = moduleleveldescriptorname(enum_descriptor);
  m["name"] = enum_descriptor.name();
  m["full_name"] = enum_descriptor.full_name();
  m["file"] = kdescriptorkey;
  const char enum_descriptor_template[] =
      "$descriptor_name$ = _descriptor.enumdescriptor(\n"
      "  name='$name$',\n"
      "  full_name='$full_name$',\n"
      "  filename=none,\n"
      "  file=$file$,\n"
      "  values=[\n";
  string options_string;
  enum_descriptor.options().serializetostring(&options_string);
  printer_->print(m, enum_descriptor_template);
  printer_->indent();
  printer_->indent();
  for (int i = 0; i < enum_descriptor.value_count(); ++i) {
    printenumvaluedescriptor(*enum_descriptor.value(i));
    printer_->print(",\n");
  }
  printer_->outdent();
  printer_->print("],\n");
  printer_->print("containing_type=none,\n");
  printer_->print("options=$options_value$,\n",
                  "options_value",
                  optionsvalue("enumoptions", options_string));
  enumdescriptorproto edp;
  printserializedpbinterval(enum_descriptor, edp);
  printer_->outdent();
  printer_->print(")\n");
  printer_->print("\n");
}

// recursively prints enums in nested types within descriptor, then
// prints enums contained at the top level in descriptor.
void generator::printnestedenums(const descriptor& descriptor) const {
  for (int i = 0; i < descriptor.nested_type_count(); ++i) {
    printnestedenums(*descriptor.nested_type(i));
  }

  for (int i = 0; i < descriptor.enum_type_count(); ++i) {
    printenum(*descriptor.enum_type(i));
  }
}

void generator::printtoplevelextensions() const {
  const bool is_extension = true;
  for (int i = 0; i < file_->extension_count(); ++i) {
    const fielddescriptor& extension_field = *file_->extension(i);
    string constant_name = extension_field.name() + "_field_number";
    upperstring(&constant_name);
    printer_->print("$constant_name$ = $number$\n",
      "constant_name", constant_name,
      "number", simpleitoa(extension_field.number()));
    printer_->print("$name$ = ", "name", extension_field.name());
    printfielddescriptor(extension_field, is_extension);
    printer_->print("\n");
  }
  printer_->print("\n");
}

// prints python equivalents of all descriptors in |file|.
void generator::printmessagedescriptors() const {
  for (int i = 0; i < file_->message_type_count(); ++i) {
    printdescriptor(*file_->message_type(i));
    printer_->print("\n");
  }
}

void generator::printservices() const {
  for (int i = 0; i < file_->service_count(); ++i) {
    printservicedescriptor(*file_->service(i));
    printserviceclass(*file_->service(i));
    printservicestub(*file_->service(i));
    printer_->print("\n");
  }
}

void generator::printservicedescriptor(
    const servicedescriptor& descriptor) const {
  printer_->print("\n");
  string service_name = modulelevelservicedescriptorname(descriptor);
  string options_string;
  descriptor.options().serializetostring(&options_string);

  printer_->print(
      "$service_name$ = _descriptor.servicedescriptor(\n",
      "service_name", service_name);
  printer_->indent();
  map<string, string> m;
  m["name"] = descriptor.name();
  m["full_name"] = descriptor.full_name();
  m["file"] = kdescriptorkey;
  m["index"] = simpleitoa(descriptor.index());
  m["options_value"] = optionsvalue("serviceoptions", options_string);
  const char required_function_arguments[] =
      "name='$name$',\n"
      "full_name='$full_name$',\n"
      "file=$file$,\n"
      "index=$index$,\n"
      "options=$options_value$,\n";
  printer_->print(m, required_function_arguments);

  servicedescriptorproto sdp;
  printserializedpbinterval(descriptor, sdp);

  printer_->print("methods=[\n");
  for (int i = 0; i < descriptor.method_count(); ++i) {
    const methoddescriptor* method = descriptor.method(i);
    method->options().serializetostring(&options_string);

    m.clear();
    m["name"] = method->name();
    m["full_name"] = method->full_name();
    m["index"] = simpleitoa(method->index());
    m["serialized_options"] = cescape(options_string);
    m["input_type"] = moduleleveldescriptorname(*(method->input_type()));
    m["output_type"] = moduleleveldescriptorname(*(method->output_type()));
    m["options_value"] = optionsvalue("methodoptions", options_string);
    printer_->print("_descriptor.methoddescriptor(\n");
    printer_->indent();
    printer_->print(
        m,
        "name='$name$',\n"
        "full_name='$full_name$',\n"
        "index=$index$,\n"
        "containing_service=none,\n"
        "input_type=$input_type$,\n"
        "output_type=$output_type$,\n"
        "options=$options_value$,\n");
    printer_->outdent();
    printer_->print("),\n");
  }

  printer_->outdent();
  printer_->print("])\n\n");
}

void generator::printserviceclass(const servicedescriptor& descriptor) const {
  // print the service.
  printer_->print("class $class_name$(_service.service):\n",
                  "class_name", descriptor.name());
  printer_->indent();
  printer_->print(
      "__metaclass__ = service_reflection.generatedservicetype\n"
      "$descriptor_key$ = $descriptor_name$\n",
      "descriptor_key", kdescriptorkey,
      "descriptor_name", modulelevelservicedescriptorname(descriptor));
  printer_->outdent();
}

void generator::printservicestub(const servicedescriptor& descriptor) const {
  // print the service stub.
  printer_->print("class $class_name$_stub($class_name$):\n",
                  "class_name", descriptor.name());
  printer_->indent();
  printer_->print(
      "__metaclass__ = service_reflection.generatedservicestubtype\n"
      "$descriptor_key$ = $descriptor_name$\n",
      "descriptor_key", kdescriptorkey,
      "descriptor_name", modulelevelservicedescriptorname(descriptor));
  printer_->outdent();
}

// prints statement assigning moduleleveldescriptorname(message_descriptor)
// to a python descriptor object for message_descriptor.
//
// mutually recursive with printnesteddescriptors().
void generator::printdescriptor(const descriptor& message_descriptor) const {
  printnesteddescriptors(message_descriptor);

  printer_->print("\n");
  printer_->print("$descriptor_name$ = _descriptor.descriptor(\n",
                  "descriptor_name",
                  moduleleveldescriptorname(message_descriptor));
  printer_->indent();
  map<string, string> m;
  m["name"] = message_descriptor.name();
  m["full_name"] = message_descriptor.full_name();
  m["file"] = kdescriptorkey;
  const char required_function_arguments[] =
      "name='$name$',\n"
      "full_name='$full_name$',\n"
      "filename=none,\n"
      "file=$file$,\n"
      "containing_type=none,\n";
  printer_->print(m, required_function_arguments);
  printfieldsindescriptor(message_descriptor);
  printextensionsindescriptor(message_descriptor);

  // nested types
  printer_->print("nested_types=[");
  for (int i = 0; i < message_descriptor.nested_type_count(); ++i) {
    const string nested_name = moduleleveldescriptorname(
        *message_descriptor.nested_type(i));
    printer_->print("$name$, ", "name", nested_name);
  }
  printer_->print("],\n");

  // enum types
  printer_->print("enum_types=[\n");
  printer_->indent();
  for (int i = 0; i < message_descriptor.enum_type_count(); ++i) {
    const string descriptor_name = moduleleveldescriptorname(
        *message_descriptor.enum_type(i));
    printer_->print(descriptor_name.c_str());
    printer_->print(",\n");
  }
  printer_->outdent();
  printer_->print("],\n");
  string options_string;
  message_descriptor.options().serializetostring(&options_string);
  printer_->print(
      "options=$options_value$,\n"
      "is_extendable=$extendable$",
      "options_value", optionsvalue("messageoptions", options_string),
      "extendable", message_descriptor.extension_range_count() > 0 ?
                      "true" : "false");
  printer_->print(",\n");

  // extension ranges
  printer_->print("extension_ranges=[");
  for (int i = 0; i < message_descriptor.extension_range_count(); ++i) {
    const descriptor::extensionrange* range =
        message_descriptor.extension_range(i);
    printer_->print("($start$, $end$), ",
                    "start", simpleitoa(range->start),
                    "end", simpleitoa(range->end));
  }
  printer_->print("],\n");

  // serialization of proto
  descriptorproto edp;
  printserializedpbinterval(message_descriptor, edp);

  printer_->outdent();
  printer_->print(")\n");
}

// prints python descriptor objects for all nested types contained in
// message_descriptor.
//
// mutually recursive with printdescriptor().
void generator::printnesteddescriptors(
    const descriptor& containing_descriptor) const {
  for (int i = 0; i < containing_descriptor.nested_type_count(); ++i) {
    printdescriptor(*containing_descriptor.nested_type(i));
  }
}

// prints all messages in |file|.
void generator::printmessages() const {
  for (int i = 0; i < file_->message_type_count(); ++i) {
    printmessage(*file_->message_type(i));
    printer_->print("\n");
  }
}

// prints a python class for the given message descriptor.  we defer to the
// metaclass to do almost all of the work of actually creating a useful class.
// the purpose of this function and its many helper functions above is merely
// to output a python version of the descriptors, which the metaclass in
// reflection.py will use to construct the meat of the class itself.
//
// mutually recursive with printnestedmessages().
void generator::printmessage(
    const descriptor& message_descriptor) const {
  printer_->print("class $name$(_message.message):\n", "name",
                  message_descriptor.name());
  printer_->indent();
  printer_->print("__metaclass__ = _reflection.generatedprotocolmessagetype\n");
  printnestedmessages(message_descriptor);
  map<string, string> m;
  m["descriptor_key"] = kdescriptorkey;
  m["descriptor_name"] = moduleleveldescriptorname(message_descriptor);
  printer_->print(m, "$descriptor_key$ = $descriptor_name$\n");

  printer_->print(
    "\n"
    "# @@protoc_insertion_point(class_scope:$full_name$)\n",
    "full_name", message_descriptor.full_name());

  printer_->outdent();
}

// prints all nested messages within |containing_descriptor|.
// mutually recursive with printmessage().
void generator::printnestedmessages(
    const descriptor& containing_descriptor) const {
  for (int i = 0; i < containing_descriptor.nested_type_count(); ++i) {
    printer_->print("\n");
    printmessage(*containing_descriptor.nested_type(i));
  }
}

// recursively fixes foreign fields in all nested types in |descriptor|, then
// sets the message_type and enum_type of all message and enum fields to point
// to their respective descriptors.
// args:
//   descriptor: descriptor to print fields for.
//   containing_descriptor: if descriptor is a nested type, this is its
//       containing type, or null if this is a root/top-level type.
void generator::fixforeignfieldsindescriptor(
    const descriptor& descriptor,
    const descriptor* containing_descriptor) const {
  for (int i = 0; i < descriptor.nested_type_count(); ++i) {
    fixforeignfieldsindescriptor(*descriptor.nested_type(i), &descriptor);
  }

  for (int i = 0; i < descriptor.field_count(); ++i) {
    const fielddescriptor& field_descriptor = *descriptor.field(i);
    fixforeignfieldsinfield(&descriptor, field_descriptor, "fields_by_name");
  }

  fixcontainingtypeindescriptor(descriptor, containing_descriptor);
  for (int i = 0; i < descriptor.enum_type_count(); ++i) {
    const enumdescriptor& enum_descriptor = *descriptor.enum_type(i);
    fixcontainingtypeindescriptor(enum_descriptor, &descriptor);
  }
}

void generator::addmessagetofiledescriptor(const descriptor& descriptor) const {
  map<string, string> m;
  m["descriptor_name"] = kdescriptorkey;
  m["message_name"] = descriptor.name();
  m["message_descriptor_name"] = moduleleveldescriptorname(descriptor);
  const char file_descriptor_template[] =
      "$descriptor_name$.message_types_by_name['$message_name$'] = "
      "$message_descriptor_name$\n";
  printer_->print(m, file_descriptor_template);
}

// sets any necessary message_type and enum_type attributes
// for the python version of |field|.
//
// containing_type may be null, in which case this is a module-level field.
//
// python_dict_name is the name of the python dict where we should
// look the field up in the containing type.  (e.g., fields_by_name
// or extensions_by_name).  we ignore python_dict_name if containing_type
// is null.
void generator::fixforeignfieldsinfield(const descriptor* containing_type,
                                        const fielddescriptor& field,
                                        const string& python_dict_name) const {
  const string field_referencing_expression = fieldreferencingexpression(
      containing_type, field, python_dict_name);
  map<string, string> m;
  m["field_ref"] = field_referencing_expression;
  const descriptor* foreign_message_type = field.message_type();
  if (foreign_message_type) {
    m["foreign_type"] = moduleleveldescriptorname(*foreign_message_type);
    printer_->print(m, "$field_ref$.message_type = $foreign_type$\n");
  }
  const enumdescriptor* enum_type = field.enum_type();
  if (enum_type) {
    m["enum_type"] = moduleleveldescriptorname(*enum_type);
    printer_->print(m, "$field_ref$.enum_type = $enum_type$\n");
  }
}

// returns the module-level expression for the given fielddescriptor.
// only works for fields in the .proto file this generator is generating for.
//
// containing_type may be null, in which case this is a module-level field.
//
// python_dict_name is the name of the python dict where we should
// look the field up in the containing type.  (e.g., fields_by_name
// or extensions_by_name).  we ignore python_dict_name if containing_type
// is null.
string generator::fieldreferencingexpression(
    const descriptor* containing_type,
    const fielddescriptor& field,
    const string& python_dict_name) const {
  // we should only ever be looking up fields in the current file.
  // the only things we refer to from other files are message descriptors.
  google_check_eq(field.file(), file_) << field.file()->name() << " vs. "
                                << file_->name();
  if (!containing_type) {
    return field.name();
  }
  return strings::substitute(
      "$0.$1['$2']",
      moduleleveldescriptorname(*containing_type),
      python_dict_name, field.name());
}

// prints containing_type for nested descriptors or enum descriptors.
template <typename descriptort>
void generator::fixcontainingtypeindescriptor(
    const descriptort& descriptor,
    const descriptor* containing_descriptor) const {
  if (containing_descriptor != null) {
    const string nested_name = moduleleveldescriptorname(descriptor);
    const string parent_name = moduleleveldescriptorname(
        *containing_descriptor);
    printer_->print(
        "$nested_name$.containing_type = $parent_name$;\n",
        "nested_name", nested_name,
        "parent_name", parent_name);
  }
}

// prints statements setting the message_type and enum_type fields in the
// python descriptor objects we've already output in ths file.  we must
// do this in a separate step due to circular references (otherwise, we'd
// just set everything in the initial assignment statements).
void generator::fixforeignfieldsindescriptors() const {
  for (int i = 0; i < file_->message_type_count(); ++i) {
    fixforeignfieldsindescriptor(*file_->message_type(i), null);
  }
  for (int i = 0; i < file_->message_type_count(); ++i) {
    addmessagetofiledescriptor(*file_->message_type(i));
  }
  printer_->print("\n");
}

// we need to not only set any necessary message_type fields, but
// also need to call registerextension() on each message we're
// extending.
void generator::fixforeignfieldsinextensions() const {
  // top-level extensions.
  for (int i = 0; i < file_->extension_count(); ++i) {
    fixforeignfieldsinextension(*file_->extension(i));
  }
  // nested extensions.
  for (int i = 0; i < file_->message_type_count(); ++i) {
    fixforeignfieldsinnestedextensions(*file_->message_type(i));
  }
  printer_->print("\n");
}

void generator::fixforeignfieldsinextension(
    const fielddescriptor& extension_field) const {
  google_check(extension_field.is_extension());
  // extension_scope() will be null for top-level extensions, which is
  // exactly what fixforeignfieldsinfield() wants.
  fixforeignfieldsinfield(extension_field.extension_scope(), extension_field,
                          "extensions_by_name");

  map<string, string> m;
  // confusingly, for fielddescriptors that happen to be extensions,
  // containing_type() means "extended type."
  // on the other hand, extension_scope() will give us what we normally
  // mean by containing_type().
  m["extended_message_class"] = modulelevelmessagename(
      *extension_field.containing_type());
  m["field"] = fieldreferencingexpression(extension_field.extension_scope(),
                                          extension_field,
                                          "extensions_by_name");
  printer_->print(m, "$extended_message_class$.registerextension($field$)\n");
}

void generator::fixforeignfieldsinnestedextensions(
    const descriptor& descriptor) const {
  // recursively fix up extensions in all nested types.
  for (int i = 0; i < descriptor.nested_type_count(); ++i) {
    fixforeignfieldsinnestedextensions(*descriptor.nested_type(i));
  }
  // fix up extensions directly contained within this type.
  for (int i = 0; i < descriptor.extension_count(); ++i) {
    fixforeignfieldsinextension(*descriptor.extension(i));
  }
}

// returns a python expression that instantiates a python enumvaluedescriptor
// object for the given c++ descriptor.
void generator::printenumvaluedescriptor(
    const enumvaluedescriptor& descriptor) const {
  // todo(robinson): fix up enumvaluedescriptor "type" fields.
  // more circular references.  ::sigh::
  string options_string;
  descriptor.options().serializetostring(&options_string);
  map<string, string> m;
  m["name"] = descriptor.name();
  m["index"] = simpleitoa(descriptor.index());
  m["number"] = simpleitoa(descriptor.number());
  m["options"] = optionsvalue("enumvalueoptions", options_string);
  printer_->print(
      m,
      "_descriptor.enumvaluedescriptor(\n"
      "  name='$name$', index=$index$, number=$number$,\n"
      "  options=$options$,\n"
      "  type=none)");
}

// returns a python expression that calls descriptor._parseoptions using
// the given descriptor class name and serialized options protobuf string.
string generator::optionsvalue(
    const string& class_name, const string& serialized_options) const {
  if (serialized_options.length() == 0 || generatingdescriptorproto()) {
    return "none";
  } else {
    string full_class_name = "descriptor_pb2." + class_name;
    return "_descriptor._parseoptions(" + full_class_name + "(), '"
        + cescape(serialized_options)+ "')";
  }
}

// prints an expression for a python fielddescriptor for |field|.
void generator::printfielddescriptor(
    const fielddescriptor& field, bool is_extension) const {
  string options_string;
  field.options().serializetostring(&options_string);
  map<string, string> m;
  m["name"] = field.name();
  m["full_name"] = field.full_name();
  m["index"] = simpleitoa(field.index());
  m["number"] = simpleitoa(field.number());
  m["type"] = simpleitoa(field.type());
  m["cpp_type"] = simpleitoa(field.cpp_type());
  m["label"] = simpleitoa(field.label());
  m["has_default_value"] = field.has_default_value() ? "true" : "false";
  m["default_value"] = stringifydefaultvalue(field);
  m["is_extension"] = is_extension ? "true" : "false";
  m["options"] = optionsvalue("fieldoptions", options_string);
  // we always set message_type and enum_type to none at this point, and then
  // these fields in correctly after all referenced descriptors have been
  // defined and/or imported (see fixforeignfieldsindescriptors()).
  const char field_descriptor_decl[] =
    "_descriptor.fielddescriptor(\n"
    "  name='$name$', full_name='$full_name$', index=$index$,\n"
    "  number=$number$, type=$type$, cpp_type=$cpp_type$, label=$label$,\n"
    "  has_default_value=$has_default_value$, default_value=$default_value$,\n"
    "  message_type=none, enum_type=none, containing_type=none,\n"
    "  is_extension=$is_extension$, extension_scope=none,\n"
    "  options=$options$)";
  printer_->print(m, field_descriptor_decl);
}

// helper for print{fields,extensions}indescriptor().
void generator::printfielddescriptorsindescriptor(
    const descriptor& message_descriptor,
    bool is_extension,
    const string& list_variable_name,
    int (descriptor::*countfn)() const,
    const fielddescriptor* (descriptor::*getterfn)(int) const) const {
  printer_->print("$list$=[\n", "list", list_variable_name);
  printer_->indent();
  for (int i = 0; i < (message_descriptor.*countfn)(); ++i) {
    printfielddescriptor(*(message_descriptor.*getterfn)(i),
                         is_extension);
    printer_->print(",\n");
  }
  printer_->outdent();
  printer_->print("],\n");
}

// prints a statement assigning "fields" to a list of python fielddescriptors,
// one for each field present in message_descriptor.
void generator::printfieldsindescriptor(
    const descriptor& message_descriptor) const {
  const bool is_extension = false;
  printfielddescriptorsindescriptor(
      message_descriptor, is_extension, "fields",
      &descriptor::field_count, &descriptor::field);
}

// prints a statement assigning "extensions" to a list of python
// fielddescriptors, one for each extension present in message_descriptor.
void generator::printextensionsindescriptor(
    const descriptor& message_descriptor) const {
  const bool is_extension = true;
  printfielddescriptorsindescriptor(
      message_descriptor, is_extension, "extensions",
      &descriptor::extension_count, &descriptor::extension);
}

bool generator::generatingdescriptorproto() const {
  return file_->name() == "google/protobuf/descriptor.proto";
}

// returns the unique python module-level identifier given to a descriptor.
// this name is module-qualified iff the given descriptor describes an
// entity that doesn't come from the current file.
template <typename descriptort>
string generator::moduleleveldescriptorname(
    const descriptort& descriptor) const {
  // fixme(robinson):
  // we currently don't worry about collisions with underscores in the type
  // names, so these would collide in nasty ways if found in the same file:
  //   outerproto.protoa.protob
  //   outerproto_protoa.protob  # underscore instead of period.
  // as would these:
  //   outerproto.protoa_.protob
  //   outerproto.protoa._protob  # leading vs. trailing underscore.
  // (contrived, but certainly possible).
  //
  // the c++ implementation doesn't guard against this either.  leaving
  // it for now...
  string name = nameprefixedwithnestedtypes(descriptor, "_");
  upperstring(&name);
  // module-private for now.  easy to make public later; almost impossible
  // to make private later.
  name = "_" + name;
  // we now have the name relative to its own module.  also qualify with
  // the module name iff this descriptor is from a different .proto file.
  if (descriptor.file() != file_) {
    name = modulename(descriptor.file()->name()) + "." + name;
  }
  return name;
}

// returns the name of the message class itself, not the descriptor.
// like moduleleveldescriptorname(), module-qualifies the name iff
// the given descriptor describes an entity that doesn't come from
// the current file.
string generator::modulelevelmessagename(const descriptor& descriptor) const {
  string name = nameprefixedwithnestedtypes(descriptor, ".");
  if (descriptor.file() != file_) {
    name = modulename(descriptor.file()->name()) + "." + name;
  }
  return name;
}

// returns the unique python module-level identifier given to a service
// descriptor.
string generator::modulelevelservicedescriptorname(
    const servicedescriptor& descriptor) const {
  string name = descriptor.name();
  upperstring(&name);
  name = "_" + name;
  if (descriptor.file() != file_) {
    name = modulename(descriptor.file()->name()) + "." + name;
  }
  return name;
}

// prints standard constructor arguments serialized_start and serialized_end.
// args:
//   descriptor: the cpp descriptor to have a serialized reference.
//   proto: a proto
// example printer output:
// serialized_start=41,
// serialized_end=43,
//
template <typename descriptort, typename descriptorprotot>
void generator::printserializedpbinterval(
    const descriptort& descriptor, descriptorprotot& proto) const {
  descriptor.copyto(&proto);
  string sp;
  proto.serializetostring(&sp);
  int offset = file_descriptor_serialized_.find(sp);
  google_check_ge(offset, 0);

  printer_->print("serialized_start=$serialized_start$,\n"
                  "serialized_end=$serialized_end$,\n",
                  "serialized_start", simpleitoa(offset),
                  "serialized_end", simpleitoa(offset + sp.size()));
}

namespace {
void printdescriptoroptionsfixingcode(const string& descriptor,
                                      const string& options,
                                      io::printer* printer) {
  // todo(xiaofeng): i have added a method _setoptions() to descriptorbase
  // in proto2 python runtime but it couldn't be used here because appengine
  // uses a snapshot version of the library in which the new method is not
  // yet present. after appengine has synced their runtime library, the code
  // below should be cleaned up to use _setoptions().
  printer->print(
      "$descriptor$.has_options = true\n"
      "$descriptor$._options = $options$\n",
      "descriptor", descriptor, "options", options);
}
}  // namespace

// prints expressions that set the options field of all descriptors.
void generator::fixalldescriptoroptions() const {
  // prints an expression that sets the file descriptor's options.
  string file_options = optionsvalue(
      "fileoptions", file_->options().serializeasstring());
  if (file_options != "none") {
    printdescriptoroptionsfixingcode(kdescriptorkey, file_options, printer_);
  }
  // prints expressions that set the options for all top level enums.
  for (int i = 0; i < file_->enum_type_count(); ++i) {
    const enumdescriptor& enum_descriptor = *file_->enum_type(i);
    fixoptionsforenum(enum_descriptor);
  }
  // prints expressions that set the options for all top level extensions.
  for (int i = 0; i < file_->extension_count(); ++i) {
    const fielddescriptor& field = *file_->extension(i);
    fixoptionsforfield(field);
  }
  // prints expressions that set the options for all messages, nested enums,
  // nested extensions and message fields.
  for (int i = 0; i < file_->message_type_count(); ++i) {
    fixoptionsformessage(*file_->message_type(i));
  }
}

// prints expressions that set the options for an enum descriptor and its
// value descriptors.
void generator::fixoptionsforenum(const enumdescriptor& enum_descriptor) const {
  string descriptor_name = moduleleveldescriptorname(enum_descriptor);
  string enum_options = optionsvalue(
      "enumoptions", enum_descriptor.options().serializeasstring());
  if (enum_options != "none") {
    printdescriptoroptionsfixingcode(descriptor_name, enum_options, printer_);
  }
  for (int i = 0; i < enum_descriptor.value_count(); ++i) {
    const enumvaluedescriptor& value_descriptor = *enum_descriptor.value(i);
    string value_options = optionsvalue(
        "enumvalueoptions", value_descriptor.options().serializeasstring());
    if (value_options != "none") {
      printdescriptoroptionsfixingcode(
          stringprintf("%s.values_by_name[\"%s\"]", descriptor_name.c_str(),
                       value_descriptor.name().c_str()),
          value_options, printer_);
    }
  }
}

// prints expressions that set the options for field descriptors (including
// extensions).
void generator::fixoptionsforfield(
    const fielddescriptor& field) const {
  string field_options = optionsvalue(
      "fieldoptions", field.options().serializeasstring());
  if (field_options != "none") {
    string field_name;
    if (field.is_extension()) {
      if (field.extension_scope() == null) {
        // top level extensions.
        field_name = field.name();
      } else {
        field_name = fieldreferencingexpression(
            field.extension_scope(), field, "extensions_by_name");
      }
    } else {
      field_name = fieldreferencingexpression(
          field.containing_type(), field, "fields_by_name");
    }
    printdescriptoroptionsfixingcode(field_name, field_options, printer_);
  }
}

// prints expressions that set the options for a message and all its inner
// types (nested messages, nested enums, extensions, fields).
void generator::fixoptionsformessage(const descriptor& descriptor) const {
  // nested messages.
  for (int i = 0; i < descriptor.nested_type_count(); ++i) {
    fixoptionsformessage(*descriptor.nested_type(i));
  }
  // enums.
  for (int i = 0; i < descriptor.enum_type_count(); ++i) {
    fixoptionsforenum(*descriptor.enum_type(i));
  }
  // fields.
  for (int i = 0; i < descriptor.field_count(); ++i) {
    const fielddescriptor& field = *descriptor.field(i);
    fixoptionsforfield(field);
  }
  // extensions.
  for (int i = 0; i < descriptor.extension_count(); ++i) {
    const fielddescriptor& field = *descriptor.extension(i);
    fixoptionsforfield(field);
  }
  // message option for this message.
  string message_options = optionsvalue(
      "messageoptions", descriptor.options().serializeasstring());
  if (message_options != "none") {
    string descriptor_name = moduleleveldescriptorname(descriptor);
    printdescriptoroptionsfixingcode(descriptor_name,
                                     message_options,
                                     printer_);
  }
}

}  // namespace python
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
