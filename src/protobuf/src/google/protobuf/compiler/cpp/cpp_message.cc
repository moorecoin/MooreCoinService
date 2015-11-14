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

#include <algorithm>
#include <google/protobuf/stubs/hash.h>
#include <map>
#include <utility>
#include <vector>
#include <google/protobuf/compiler/cpp/cpp_message.h>
#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_enum.h>
#include <google/protobuf/compiler/cpp/cpp_extension.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/descriptor.pb.h>


namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

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

const char* kwiretypenames[] = {
  "varint",
  "fixed64",
  "length_delimited",
  "start_group",
  "end_group",
  "fixed32",
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

// functor for sorting extension ranges by their "start" field number.
struct extensionrangesorter {
  bool operator()(const descriptor::extensionrange* left,
                  const descriptor::extensionrange* right) const {
    return left->start < right->start;
  }
};

// returns true if the "required" restriction check should be ignored for the
// given field.
inline static bool shouldignorerequiredfieldcheck(
    const fielddescriptor* field) {
  return false;
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
    // since the first occurrence of a required field causes the whole
    // function to return true, we can assume that if the type is already
    // in the cache it didn't have any required fields.
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
    if (field->cpp_type() == fielddescriptor::cpptype_message &&
        !shouldignorerequiredfieldcheck(field)) {
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

// this returns an estimate of the compiler's alignment for the field.  this
// can't guarantee to be correct because the generated code could be compiled on
// different systems with different alignment rules.  the estimates below assume
// 64-bit pointers.
int estimatealignmentsize(const fielddescriptor* field) {
  if (field == null) return 0;
  if (field->is_repeated()) return 8;
  switch (field->cpp_type()) {
    case fielddescriptor::cpptype_bool:
      return 1;

    case fielddescriptor::cpptype_int32:
    case fielddescriptor::cpptype_uint32:
    case fielddescriptor::cpptype_enum:
    case fielddescriptor::cpptype_float:
      return 4;

    case fielddescriptor::cpptype_int64:
    case fielddescriptor::cpptype_uint64:
    case fielddescriptor::cpptype_double:
    case fielddescriptor::cpptype_string:
    case fielddescriptor::cpptype_message:
      return 8;
  }
  google_log(fatal) << "can't get here.";
  return -1;  // make compiler happy.
}

// fieldgroup is just a helper for optimizepadding below.  it holds a vector of
// fields that are grouped together because they have compatible alignment, and
// a preferred location in the final field ordering.
class fieldgroup {
 public:
  fieldgroup()
      : preferred_location_(0) {}

  // a group with a single field.
  fieldgroup(float preferred_location, const fielddescriptor* field)
      : preferred_location_(preferred_location),
        fields_(1, field) {}

  // append the fields in 'other' to this group.
  void append(const fieldgroup& other) {
    if (other.fields_.empty()) {
      return;
    }
    // preferred location is the average among all the fields, so we weight by
    // the number of fields on each fieldgroup object.
    preferred_location_ =
        (preferred_location_ * fields_.size() +
         (other.preferred_location_ * other.fields_.size())) /
        (fields_.size() + other.fields_.size());
    fields_.insert(fields_.end(), other.fields_.begin(), other.fields_.end());
  }

  void setpreferredlocation(float location) { preferred_location_ = location; }
  const vector<const fielddescriptor*>& fields() const { return fields_; }

  // fieldgroup objects sort by their preferred location.
  bool operator<(const fieldgroup& other) const {
    return preferred_location_ < other.preferred_location_;
  }

 private:
  // "preferred_location_" is an estimate of where this group should go in the
  // final list of fields.  we compute this by taking the average index of each
  // field in this group in the original ordering of fields.  this is very
  // approximate, but should put this group close to where its member fields
  // originally went.
  float preferred_location_;
  vector<const fielddescriptor*> fields_;
  // we rely on the default copy constructor and operator= so this type can be
  // used in a vector.
};

// reorder 'fields' so that if the fields are output into a c++ class in the new
// order, the alignment padding is minimized.  we try to do this while keeping
// each field as close as possible to its original position so that we don't
// reduce cache locality much for function that access each field in order.
void optimizepadding(vector<const fielddescriptor*>* fields) {
  // first divide fields into those that align to 1 byte, 4 bytes or 8 bytes.
  vector<fieldgroup> aligned_to_1, aligned_to_4, aligned_to_8;
  for (int i = 0; i < fields->size(); ++i) {
    switch (estimatealignmentsize((*fields)[i])) {
      case 1: aligned_to_1.push_back(fieldgroup(i, (*fields)[i])); break;
      case 4: aligned_to_4.push_back(fieldgroup(i, (*fields)[i])); break;
      case 8: aligned_to_8.push_back(fieldgroup(i, (*fields)[i])); break;
      default:
        google_log(fatal) << "unknown alignment size.";
    }
  }

  // now group fields aligned to 1 byte into sets of 4, and treat those like a
  // single field aligned to 4 bytes.
  for (int i = 0; i < aligned_to_1.size(); i += 4) {
    fieldgroup field_group;
    for (int j = i; j < aligned_to_1.size() && j < i + 4; ++j) {
      field_group.append(aligned_to_1[j]);
    }
    aligned_to_4.push_back(field_group);
  }
  // sort by preferred location to keep fields as close to their original
  // location as possible.
  sort(aligned_to_4.begin(), aligned_to_4.end());

  // now group fields aligned to 4 bytes (or the 4-field groups created above)
  // into pairs, and treat those like a single field aligned to 8 bytes.
  for (int i = 0; i < aligned_to_4.size(); i += 2) {
    fieldgroup field_group;
    for (int j = i; j < aligned_to_4.size() && j < i + 2; ++j) {
      field_group.append(aligned_to_4[j]);
    }
    if (i == aligned_to_4.size() - 1) {
      // move incomplete 4-byte block to the end.
      field_group.setpreferredlocation(fields->size() + 1);
    }
    aligned_to_8.push_back(field_group);
  }
  // sort by preferred location to keep fields as close to their original
  // location as possible.
  sort(aligned_to_8.begin(), aligned_to_8.end());

  // now pull out all the fielddescriptors in order.
  fields->clear();
  for (int i = 0; i < aligned_to_8.size(); ++i) {
    fields->insert(fields->end(),
                   aligned_to_8[i].fields().begin(),
                   aligned_to_8[i].fields().end());
  }
}

}

// ===================================================================

messagegenerator::messagegenerator(const descriptor* descriptor,
                                   const options& options)
  : descriptor_(descriptor),
    classname_(classname(descriptor, false)),
    options_(options),
    field_generators_(descriptor, options),
    nested_generators_(new scoped_ptr<messagegenerator>[
      descriptor->nested_type_count()]),
    enum_generators_(new scoped_ptr<enumgenerator>[
      descriptor->enum_type_count()]),
    extension_generators_(new scoped_ptr<extensiongenerator>[
      descriptor->extension_count()]) {

  for (int i = 0; i < descriptor->nested_type_count(); i++) {
    nested_generators_[i].reset(
      new messagegenerator(descriptor->nested_type(i), options));
  }

  for (int i = 0; i < descriptor->enum_type_count(); i++) {
    enum_generators_[i].reset(
      new enumgenerator(descriptor->enum_type(i), options));
  }

  for (int i = 0; i < descriptor->extension_count(); i++) {
    extension_generators_[i].reset(
      new extensiongenerator(descriptor->extension(i), options));
  }
}

messagegenerator::~messagegenerator() {}

void messagegenerator::
generateforwarddeclaration(io::printer* printer) {
  printer->print("class $classname$;\n",
                 "classname", classname_);

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateforwarddeclaration(printer);
  }
}

void messagegenerator::
generateenumdefinitions(io::printer* printer) {
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateenumdefinitions(printer);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->generatedefinition(printer);
  }
}

void messagegenerator::
generategetenumdescriptorspecializations(io::printer* printer) {
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generategetenumdescriptorspecializations(printer);
  }
  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->generategetenumdescriptorspecializations(printer);
  }
}

void messagegenerator::
generatefieldaccessordeclarations(io::printer* printer) {
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    printfieldcomment(printer, field);

    map<string, string> vars;
    setcommonfieldvariables(field, &vars, options_);
    vars["constant_name"] = fieldconstantname(field);

    if (field->is_repeated()) {
      printer->print(vars, "inline int $name$_size() const$deprecation$;\n");
    } else {
      printer->print(vars, "inline bool has_$name$() const$deprecation$;\n");
    }

    printer->print(vars, "inline void clear_$name$()$deprecation$;\n");
    printer->print(vars, "static const int $constant_name$ = $number$;\n");

    // generate type-specific accessor declarations.
    field_generators_.get(field).generateaccessordeclarations(printer);

    printer->print("\n");
  }

  if (descriptor_->extension_range_count() > 0) {
    // generate accessors for extensions.  we just call a macro located in
    // extension_set.h since the accessors about 80 lines of static code.
    printer->print(
      "google_protobuf_extension_accessors($classname$)\n",
      "classname", classname_);
  }
}

void messagegenerator::
generatefieldaccessordefinitions(io::printer* printer) {
  printer->print("// $classname$\n\n", "classname", classname_);

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    printfieldcomment(printer, field);

    map<string, string> vars;
    setcommonfieldvariables(field, &vars, options_);

    // generate has_$name$() or $name$_size().
    if (field->is_repeated()) {
      printer->print(vars,
        "inline int $classname$::$name$_size() const {\n"
        "  return $name$_.size();\n"
        "}\n");
    } else {
      // singular field.
      char buffer[kfasttobuffersize];
      vars["has_array_index"] = simpleitoa(field->index() / 32);
      vars["has_mask"] = fasthex32tobuffer(1u << (field->index() % 32), buffer);
      printer->print(vars,
        "inline bool $classname$::has_$name$() const {\n"
        "  return (_has_bits_[$has_array_index$] & 0x$has_mask$u) != 0;\n"
        "}\n"
        "inline void $classname$::set_has_$name$() {\n"
        "  _has_bits_[$has_array_index$] |= 0x$has_mask$u;\n"
        "}\n"
        "inline void $classname$::clear_has_$name$() {\n"
        "  _has_bits_[$has_array_index$] &= ~0x$has_mask$u;\n"
        "}\n"
        );
    }

    // generate clear_$name$()
    printer->print(vars,
      "inline void $classname$::clear_$name$() {\n");

    printer->indent();
    field_generators_.get(field).generateclearingcode(printer);
    printer->outdent();

    if (!field->is_repeated()) {
      printer->print(vars,
                     "  clear_has_$name$();\n");
    }

    printer->print("}\n");

    // generate type-specific accessors.
    field_generators_.get(field).generateinlineaccessordefinitions(printer);

    printer->print("\n");
  }
}

void messagegenerator::
generateclassdefinition(io::printer* printer) {
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateclassdefinition(printer);
    printer->print("\n");
    printer->print(kthinseparator);
    printer->print("\n");
  }

  map<string, string> vars;
  vars["classname"] = classname_;
  vars["field_count"] = simpleitoa(descriptor_->field_count());
  if (options_.dllexport_decl.empty()) {
    vars["dllexport"] = "";
  } else {
    vars["dllexport"] = options_.dllexport_decl + " ";
  }
  vars["superclass"] = superclassname(descriptor_);

  printer->print(vars,
    "class $dllexport$$classname$ : public $superclass$ {\n"
    " public:\n");
  printer->indent();

  printer->print(vars,
    "$classname$();\n"
    "virtual ~$classname$();\n"
    "\n"
    "$classname$(const $classname$& from);\n"
    "\n"
    "inline $classname$& operator=(const $classname$& from) {\n"
    "  copyfrom(from);\n"
    "  return *this;\n"
    "}\n"
    "\n");

  if (hasunknownfields(descriptor_->file())) {
    printer->print(
      "inline const ::google::protobuf::unknownfieldset& unknown_fields() const {\n"
      "  return _unknown_fields_;\n"
      "}\n"
      "\n"
      "inline ::google::protobuf::unknownfieldset* mutable_unknown_fields() {\n"
      "  return &_unknown_fields_;\n"
      "}\n"
      "\n");
  }

  // only generate this member if it's not disabled.
  if (hasdescriptormethods(descriptor_->file()) &&
      !descriptor_->options().no_standard_descriptor_accessor()) {
    printer->print(vars,
      "static const ::google::protobuf::descriptor* descriptor();\n");
  }

  printer->print(vars,
    "static const $classname$& default_instance();\n"
    "\n");

  if (!staticinitializersforced(descriptor_->file())) {
    printer->print(vars,
      "#ifdef google_protobuf_no_static_initializer\n"
      "// returns the internal default instance pointer. this function can\n"
      "// return null thus should not be used by the user. this is intended\n"
      "// for protobuf internal code. please use default_instance() declared\n"
      "// above instead.\n"
      "static inline const $classname$* internal_default_instance() {\n"
      "  return default_instance_;\n"
      "}\n"
      "#endif\n"
      "\n");
  }


  printer->print(vars,
    "void swap($classname$* other);\n"
    "\n"
    "// implements message ----------------------------------------------\n"
    "\n"
    "$classname$* new() const;\n");

  if (hasgeneratedmethods(descriptor_->file())) {
    if (hasdescriptormethods(descriptor_->file())) {
      printer->print(vars,
        "void copyfrom(const ::google::protobuf::message& from);\n"
        "void mergefrom(const ::google::protobuf::message& from);\n");
    } else {
      printer->print(vars,
        "void checktypeandmergefrom(const ::google::protobuf::messagelite& from);\n");
    }

    printer->print(vars,
      "void copyfrom(const $classname$& from);\n"
      "void mergefrom(const $classname$& from);\n"
      "void clear();\n"
      "bool isinitialized() const;\n"
      "\n"
      "int bytesize() const;\n"
      "bool mergepartialfromcodedstream(\n"
      "    ::google::protobuf::io::codedinputstream* input);\n"
      "void serializewithcachedsizes(\n"
      "    ::google::protobuf::io::codedoutputstream* output) const;\n");
    if (hasfastarrayserialization(descriptor_->file())) {
      printer->print(
        "::google::protobuf::uint8* serializewithcachedsizestoarray(::google::protobuf::uint8* output) const;\n");
    }
  }

  printer->print(vars,
    "int getcachedsize() const { return _cached_size_; }\n"
    "private:\n"
    "void sharedctor();\n"
    "void shareddtor();\n"
    "void setcachedsize(int size) const;\n"
    "public:\n"
    "\n");

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(
      "::google::protobuf::metadata getmetadata() const;\n"
      "\n");
  } else {
    printer->print(
      "::std::string gettypename() const;\n"
      "\n");
  }

  printer->print(
    "// nested types ----------------------------------------------------\n"
    "\n");

  // import all nested message classes into this class's scope with typedefs.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    const descriptor* nested_type = descriptor_->nested_type(i);
    printer->print("typedef $nested_full_name$ $nested_name$;\n",
                   "nested_name", nested_type->name(),
                   "nested_full_name", classname(nested_type, false));
  }

  if (descriptor_->nested_type_count() > 0) {
    printer->print("\n");
  }

  // import all nested enums and their values into this class's scope with
  // typedefs and constants.
  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->generatesymbolimports(printer);
    printer->print("\n");
  }

  printer->print(
    "// accessors -------------------------------------------------------\n"
    "\n");

  // generate accessor methods for all fields.
  generatefieldaccessordeclarations(printer);

  // declare extension identifiers.
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extension_generators_[i]->generatedeclaration(printer);
  }


  printer->print(
    "// @@protoc_insertion_point(class_scope:$full_name$)\n",
    "full_name", descriptor_->full_name());

  // generate private members.
  printer->outdent();
  printer->print(" private:\n");
  printer->indent();


  for (int i = 0; i < descriptor_->field_count(); i++) {
    if (!descriptor_->field(i)->is_repeated()) {
      printer->print(
        "inline void set_has_$name$();\n",
        "name", fieldname(descriptor_->field(i)));
      printer->print(
        "inline void clear_has_$name$();\n",
        "name", fieldname(descriptor_->field(i)));
    }
  }
  printer->print("\n");

  // to minimize padding, data members are divided into three sections:
  // (1) members assumed to align to 8 bytes
  // (2) members corresponding to message fields, re-ordered to optimize
  //     alignment.
  // (3) members assumed to align to 4 bytes.

  // members assumed to align to 8 bytes:

  if (descriptor_->extension_range_count() > 0) {
    printer->print(
      "::google::protobuf::internal::extensionset _extensions_;\n"
      "\n");
  }

  if (hasunknownfields(descriptor_->file())) {
    printer->print(
      "::google::protobuf::unknownfieldset _unknown_fields_;\n"
      "\n");
  }

  // field members:

  vector<const fielddescriptor*> fields;
  for (int i = 0; i < descriptor_->field_count(); i++) {
    fields.push_back(descriptor_->field(i));
  }
  optimizepadding(&fields);
  for (int i = 0; i < fields.size(); ++i) {
    field_generators_.get(fields[i]).generateprivatemembers(printer);
  }

  // members assumed to align to 4 bytes:

  // todo(kenton):  make _cached_size_ an atomic<int> when c++ supports it.
  printer->print(
      "\n"
      "mutable int _cached_size_;\n");

  // generate _has_bits_.
  if (descriptor_->field_count() > 0) {
    printer->print(vars,
      "::google::protobuf::uint32 _has_bits_[($field_count$ + 31) / 32];\n"
      "\n");
  } else {
    // zero-size arrays aren't technically allowed, and msvc in particular
    // doesn't like them.  we still need to declare these arrays to make
    // other code compile.  since this is an uncommon case, we'll just declare
    // them with size 1 and waste some space.  oh well.
    printer->print(
      "::google::protobuf::uint32 _has_bits_[1];\n"
      "\n");
  }

  // declare adddescriptors(), builddescriptors(), and shutdownfile() as
  // friends so that they can access private static variables like
  // default_instance_ and reflection_.
  printhandlingoptionalstaticinitializers(
    descriptor_->file(), printer,
    // with static initializers.
    "friend void $dllexport_decl$ $adddescriptorsname$();\n",
    // without.
    "friend void $dllexport_decl$ $adddescriptorsname$_impl();\n",
    // vars.
    "dllexport_decl", options_.dllexport_decl,
    "adddescriptorsname",
    globaladddescriptorsname(descriptor_->file()->name()));

  printer->print(
    "friend void $assigndescriptorsname$();\n"
    "friend void $shutdownfilename$();\n"
    "\n",
    "assigndescriptorsname",
      globalassigndescriptorsname(descriptor_->file()->name()),
    "shutdownfilename", globalshutdownfilename(descriptor_->file()->name()));

  printer->print(
    "void initasdefaultinstance();\n"
    "static $classname$* default_instance_;\n",
    "classname", classname_);

  printer->outdent();
  printer->print(vars, "};");
}

void messagegenerator::
generateinlinemethods(io::printer* printer) {
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateinlinemethods(printer);
    printer->print(kthinseparator);
    printer->print("\n");
  }

  generatefieldaccessordefinitions(printer);
}

void messagegenerator::
generatedescriptordeclarations(io::printer* printer) {
  printer->print(
    "const ::google::protobuf::descriptor* $name$_descriptor_ = null;\n"
    "const ::google::protobuf::internal::generatedmessagereflection*\n"
    "  $name$_reflection_ = null;\n",
    "name", classname_);

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generatedescriptordeclarations(printer);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    printer->print(
      "const ::google::protobuf::enumdescriptor* $name$_descriptor_ = null;\n",
      "name", classname(descriptor_->enum_type(i), false));
  }
}

void messagegenerator::
generatedescriptorinitializer(io::printer* printer, int index) {
  // todo(kenton):  passing the index to this method is redundant; just use
  //   descriptor_->index() instead.
  map<string, string> vars;
  vars["classname"] = classname_;
  vars["index"] = simpleitoa(index);

  // obtain the descriptor from the parent's descriptor.
  if (descriptor_->containing_type() == null) {
    printer->print(vars,
      "$classname$_descriptor_ = file->message_type($index$);\n");
  } else {
    vars["parent"] = classname(descriptor_->containing_type(), false);
    printer->print(vars,
      "$classname$_descriptor_ = "
        "$parent$_descriptor_->nested_type($index$);\n");
  }

  // generate the offsets.
  generateoffsets(printer);

  // construct the reflection object.
  printer->print(vars,
    "$classname$_reflection_ =\n"
    "  new ::google::protobuf::internal::generatedmessagereflection(\n"
    "    $classname$_descriptor_,\n"
    "    $classname$::default_instance_,\n"
    "    $classname$_offsets_,\n"
    "    google_protobuf_generated_message_field_offset($classname$, _has_bits_[0]),\n"
    "    google_protobuf_generated_message_field_offset("
      "$classname$, _unknown_fields_),\n");
  if (descriptor_->extension_range_count() > 0) {
    printer->print(vars,
      "    google_protobuf_generated_message_field_offset("
        "$classname$, _extensions_),\n");
  } else {
    // no extensions.
    printer->print(vars,
      "    -1,\n");
  }
  printer->print(
    "    ::google::protobuf::descriptorpool::generated_pool(),\n");
  printer->print(vars,
    "    ::google::protobuf::messagefactory::generated_factory(),\n");
  printer->print(vars,
    "    sizeof($classname$));\n");

  // handle nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generatedescriptorinitializer(printer, i);
  }

  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->generatedescriptorinitializer(printer, i);
  }
}

void messagegenerator::
generatetyperegistrations(io::printer* printer) {
  // register this message type with the message factory.
  printer->print(
    "::google::protobuf::messagefactory::internalregistergeneratedmessage(\n"
    "  $classname$_descriptor_, &$classname$::default_instance());\n",
    "classname", classname_);

  // handle nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generatetyperegistrations(printer);
  }
}

void messagegenerator::
generatedefaultinstanceallocator(io::printer* printer) {
  // construct the default instances of all fields, as they will be used
  // when creating the default instance of the entire message.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generatedefaultinstanceallocator(printer);
  }

  // construct the default instance.  we can't call initasdefaultinstance() yet
  // because we need to make sure all default instances that this one might
  // depend on are constructed first.
  printer->print(
    "$classname$::default_instance_ = new $classname$();\n",
    "classname", classname_);

  // handle nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generatedefaultinstanceallocator(printer);
  }

}

void messagegenerator::
generatedefaultinstanceinitializer(io::printer* printer) {
  printer->print(
    "$classname$::default_instance_->initasdefaultinstance();\n",
    "classname", classname_);

  // register extensions.
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extension_generators_[i]->generateregistration(printer);
  }

  // handle nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generatedefaultinstanceinitializer(printer);
  }
}

void messagegenerator::
generateshutdowncode(io::printer* printer) {
  printer->print(
    "delete $classname$::default_instance_;\n",
    "classname", classname_);

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(
      "delete $classname$_reflection_;\n",
      "classname", classname_);
  }

  // handle default instances of fields.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generateshutdowncode(printer);
  }

  // handle nested types.
  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateshutdowncode(printer);
  }
}

void messagegenerator::
generateclassmethods(io::printer* printer) {
  for (int i = 0; i < descriptor_->enum_type_count(); i++) {
    enum_generators_[i]->generatemethods(printer);
  }

  for (int i = 0; i < descriptor_->nested_type_count(); i++) {
    nested_generators_[i]->generateclassmethods(printer);
    printer->print("\n");
    printer->print(kthinseparator);
    printer->print("\n");
  }

  // generate non-inline field definitions.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generatenoninlineaccessordefinitions(printer);
  }

  // generate field number constants.
  printer->print("#ifndef _msc_ver\n");
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor *field = descriptor_->field(i);
    printer->print(
      "const int $classname$::$constant_name$;\n",
      "classname", classname(fieldscope(field), false),
      "constant_name", fieldconstantname(field));
  }
  printer->print(
    "#endif  // !_msc_ver\n"
    "\n");

  // define extension identifiers.
  for (int i = 0; i < descriptor_->extension_count(); i++) {
    extension_generators_[i]->generatedefinition(printer);
  }

  generatestructors(printer);
  printer->print("\n");

  if (hasgeneratedmethods(descriptor_->file())) {
    generateclear(printer);
    printer->print("\n");

    generatemergefromcodedstream(printer);
    printer->print("\n");

    generateserializewithcachedsizes(printer);
    printer->print("\n");

    if (hasfastarrayserialization(descriptor_->file())) {
      generateserializewithcachedsizestoarray(printer);
      printer->print("\n");
    }

    generatebytesize(printer);
    printer->print("\n");

    generatemergefrom(printer);
    printer->print("\n");

    generatecopyfrom(printer);
    printer->print("\n");

    generateisinitialized(printer);
    printer->print("\n");
  }

  generateswap(printer);
  printer->print("\n");

  if (hasdescriptormethods(descriptor_->file())) {
    printer->print(
      "::google::protobuf::metadata $classname$::getmetadata() const {\n"
      "  protobuf_assigndescriptorsonce();\n"
      "  ::google::protobuf::metadata metadata;\n"
      "  metadata.descriptor = $classname$_descriptor_;\n"
      "  metadata.reflection = $classname$_reflection_;\n"
      "  return metadata;\n"
      "}\n"
      "\n",
      "classname", classname_);
  } else {
    printer->print(
      "::std::string $classname$::gettypename() const {\n"
      "  return \"$type_name$\";\n"
      "}\n"
      "\n",
      "classname", classname_,
      "type_name", descriptor_->full_name());
  }

}

void messagegenerator::
generateoffsets(io::printer* printer) {
  printer->print(
    "static const int $classname$_offsets_[$field_count$] = {\n",
    "classname", classname_,
    "field_count", simpleitoa(max(1, descriptor_->field_count())));
  printer->indent();

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    printer->print(
      "google_protobuf_generated_message_field_offset($classname$, $name$_),\n",
      "classname", classname_,
      "name", fieldname(field));
  }

  printer->outdent();
  printer->print("};\n");
}

void messagegenerator::
generatesharedconstructorcode(io::printer* printer) {
  printer->print(
    "void $classname$::sharedctor() {\n",
    "classname", classname_);
  printer->indent();

  printer->print(
    "_cached_size_ = 0;\n");

  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generateconstructorcode(printer);
  }

  printer->print(
    "::memset(_has_bits_, 0, sizeof(_has_bits_));\n");

  printer->outdent();
  printer->print("}\n\n");
}

void messagegenerator::
generateshareddestructorcode(io::printer* printer) {
  printer->print(
    "void $classname$::shareddtor() {\n",
    "classname", classname_);
  printer->indent();
  // write the destructors for each field.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    field_generators_.get(descriptor_->field(i))
                     .generatedestructorcode(printer);
  }

  printhandlingoptionalstaticinitializers(
    descriptor_->file(), printer,
    // with static initializers.
    "if (this != default_instance_) {\n",
    // without.
    "if (this != &default_instance()) {\n");

  // we need to delete all embedded messages.
  // todo(kenton):  if we make unset messages point at default instances
  //   instead of null, then it would make sense to move this code into
  //   messagefieldgenerator::generatedestructorcode().
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (!field->is_repeated() &&
        field->cpp_type() == fielddescriptor::cpptype_message) {
      printer->print("  delete $name$_;\n",
                     "name", fieldname(field));
    }
  }

  printer->outdent();
  printer->print(
    "  }\n"
    "}\n"
    "\n");
}

void messagegenerator::
generatestructors(io::printer* printer) {
  string superclass = superclassname(descriptor_);

  // generate the default constructor.
  printer->print(
    "$classname$::$classname$()\n"
    "  : $superclass$() {\n"
    "  sharedctor();\n"
    "}\n",
    "classname", classname_,
    "superclass", superclass);

  printer->print(
    "\n"
    "void $classname$::initasdefaultinstance() {\n",
    "classname", classname_);

  // the default instance needs all of its embedded message pointers
  // cross-linked to other default instances.  we can't do this initialization
  // in the constructor because some other default instances may not have been
  // constructed yet at that time.
  // todo(kenton):  maybe all message fields (even for non-default messages)
  //   should be initialized to point at default instances rather than null?
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (!field->is_repeated() &&
        field->cpp_type() == fielddescriptor::cpptype_message) {
      printhandlingoptionalstaticinitializers(
        descriptor_->file(), printer,
        // with static initializers.
        "  $name$_ = const_cast< $type$*>(&$type$::default_instance());\n",
        // without.
        "  $name$_ = const_cast< $type$*>(\n"
        "      $type$::internal_default_instance());\n",
        // vars.
        "name", fieldname(field),
        "type", fieldmessagetypename(field));
    }
  }
  printer->print(
    "}\n"
    "\n");

  // generate the copy constructor.
  printer->print(
    "$classname$::$classname$(const $classname$& from)\n"
    "  : $superclass$() {\n"
    "  sharedctor();\n"
    "  mergefrom(from);\n"
    "}\n"
    "\n",
    "classname", classname_,
    "superclass", superclass);

  // generate the shared constructor code.
  generatesharedconstructorcode(printer);

  // generate the destructor.
  printer->print(
    "$classname$::~$classname$() {\n"
    "  shareddtor();\n"
    "}\n"
    "\n",
    "classname", classname_);

  // generate the shared destructor code.
  generateshareddestructorcode(printer);

  // generate setcachedsize.
  printer->print(
    "void $classname$::setcachedsize(int size) const {\n"
    "  google_safe_concurrent_writes_begin();\n"
    "  _cached_size_ = size;\n"
    "  google_safe_concurrent_writes_end();\n"
    "}\n",
    "classname", classname_);

  // only generate this member if it's not disabled.
  if (hasdescriptormethods(descriptor_->file()) &&
      !descriptor_->options().no_standard_descriptor_accessor()) {
    printer->print(
      "const ::google::protobuf::descriptor* $classname$::descriptor() {\n"
      "  protobuf_assigndescriptorsonce();\n"
      "  return $classname$_descriptor_;\n"
      "}\n"
      "\n",
      "classname", classname_,
      "adddescriptorsname",
      globaladddescriptorsname(descriptor_->file()->name()));
  }

  printer->print(
    "const $classname$& $classname$::default_instance() {\n",
    "classname", classname_);

  printhandlingoptionalstaticinitializers(
    descriptor_->file(), printer,
    // with static initializers.
    "  if (default_instance_ == null) $adddescriptorsname$();\n",
    // without.
    "  $adddescriptorsname$();\n",
    // vars.
    "adddescriptorsname",
    globaladddescriptorsname(descriptor_->file()->name()));

  printer->print(
    "  return *default_instance_;\n"
    "}\n"
    "\n"
    "$classname$* $classname$::default_instance_ = null;\n"
    "\n"
    "$classname$* $classname$::new() const {\n"
    "  return new $classname$;\n"
    "}\n",
    "classname", classname_,
    "adddescriptorsname",
    globaladddescriptorsname(descriptor_->file()->name()));
}

void messagegenerator::
generateclear(io::printer* printer) {
  printer->print("void $classname$::clear() {\n",
                 "classname", classname_);
  printer->indent();

  int last_index = -1;

  if (descriptor_->extension_range_count() > 0) {
    printer->print("_extensions_.clear();\n");
  }

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (!field->is_repeated()) {
      // we can use the fact that _has_bits_ is a giant bitfield to our
      // advantage:  we can check up to 32 bits at a time for equality to
      // zero, and skip the whole range if so.  this can improve the speed
      // of clear() for messages which contain a very large number of
      // optional fields of which only a few are used at a time.  here,
      // we've chosen to check 8 bits at a time rather than 32.
      if (i / 8 != last_index / 8 || last_index < 0) {
        if (last_index >= 0) {
          printer->outdent();
          printer->print("}\n");
        }
        printer->print(
          "if (_has_bits_[$index$ / 32] & (0xffu << ($index$ % 32))) {\n",
          "index", simpleitoa(field->index()));
        printer->indent();
      }
      last_index = i;

      // it's faster to just overwrite primitive types, but we should
      // only clear strings and messages if they were set.
      // todo(kenton):  let the cppfieldgenerator decide this somehow.
      bool should_check_bit =
        field->cpp_type() == fielddescriptor::cpptype_message ||
        field->cpp_type() == fielddescriptor::cpptype_string;

      if (should_check_bit) {
        printer->print(
          "if (has_$name$()) {\n",
          "name", fieldname(field));
        printer->indent();
      }

      field_generators_.get(field).generateclearingcode(printer);

      if (should_check_bit) {
        printer->outdent();
        printer->print("}\n");
      }
    }
  }

  if (last_index >= 0) {
    printer->outdent();
    printer->print("}\n");
  }

  // repeated fields don't use _has_bits_ so we clear them in a separate
  // pass.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (field->is_repeated()) {
      field_generators_.get(field).generateclearingcode(printer);
    }
  }

  printer->print(
    "::memset(_has_bits_, 0, sizeof(_has_bits_));\n");

  if (hasunknownfields(descriptor_->file())) {
    printer->print(
      "mutable_unknown_fields()->clear();\n");
  }

  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::
generateswap(io::printer* printer) {
  // generate the swap member function.
  printer->print("void $classname$::swap($classname$* other) {\n",
                 "classname", classname_);
  printer->indent();
  printer->print("if (other != this) {\n");
  printer->indent();

  if (hasgeneratedmethods(descriptor_->file())) {
    for (int i = 0; i < descriptor_->field_count(); i++) {
      const fielddescriptor* field = descriptor_->field(i);
      field_generators_.get(field).generateswappingcode(printer);
    }

    for (int i = 0; i < (descriptor_->field_count() + 31) / 32; ++i) {
      printer->print("std::swap(_has_bits_[$i$], other->_has_bits_[$i$]);\n",
                     "i", simpleitoa(i));
    }

    if (hasunknownfields(descriptor_->file())) {
      printer->print("_unknown_fields_.swap(&other->_unknown_fields_);\n");
    }
    printer->print("std::swap(_cached_size_, other->_cached_size_);\n");
    if (descriptor_->extension_range_count() > 0) {
      printer->print("_extensions_.swap(&other->_extensions_);\n");
    }
  } else {
    printer->print("getreflection()->swap(this, other);");
  }

  printer->outdent();
  printer->print("}\n");
  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::
generatemergefrom(io::printer* printer) {
  if (hasdescriptormethods(descriptor_->file())) {
    // generate the generalized mergefrom (aka that which takes in the message
    // base class as a parameter).
    printer->print(
      "void $classname$::mergefrom(const ::google::protobuf::message& from) {\n"
      "  google_check_ne(&from, this);\n",
      "classname", classname_);
    printer->indent();

    // cast the message to the proper type. if we find that the message is
    // *not* of the proper type, we can still call merge via the reflection
    // system, as the google_check above ensured that we have the same descriptor
    // for each message.
    printer->print(
      "const $classname$* source =\n"
      "  ::google::protobuf::internal::dynamic_cast_if_available<const $classname$*>(\n"
      "    &from);\n"
      "if (source == null) {\n"
      "  ::google::protobuf::internal::reflectionops::merge(from, this);\n"
      "} else {\n"
      "  mergefrom(*source);\n"
      "}\n",
      "classname", classname_);

    printer->outdent();
    printer->print("}\n\n");
  } else {
    // generate checktypeandmergefrom().
    printer->print(
      "void $classname$::checktypeandmergefrom(\n"
      "    const ::google::protobuf::messagelite& from) {\n"
      "  mergefrom(*::google::protobuf::down_cast<const $classname$*>(&from));\n"
      "}\n"
      "\n",
      "classname", classname_);
  }

  // generate the class-specific mergefrom, which avoids the google_check and cast.
  printer->print(
    "void $classname$::mergefrom(const $classname$& from) {\n"
    "  google_check_ne(&from, this);\n",
    "classname", classname_);
  printer->indent();

  // merge repeated fields. these fields do not require a
  // check as we can simply iterate over them.
  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const fielddescriptor* field = descriptor_->field(i);

    if (field->is_repeated()) {
      field_generators_.get(field).generatemergingcode(printer);
    }
  }

  // merge optional and required fields (after a _has_bit check).
  int last_index = -1;

  for (int i = 0; i < descriptor_->field_count(); ++i) {
    const fielddescriptor* field = descriptor_->field(i);

    if (!field->is_repeated()) {
      // see above in generateclear for an explanation of this.
      if (i / 8 != last_index / 8 || last_index < 0) {
        if (last_index >= 0) {
          printer->outdent();
          printer->print("}\n");
        }
        printer->print(
          "if (from._has_bits_[$index$ / 32] & (0xffu << ($index$ % 32))) {\n",
          "index", simpleitoa(field->index()));
        printer->indent();
      }

      last_index = i;

      printer->print(
        "if (from.has_$name$()) {\n",
        "name", fieldname(field));
      printer->indent();

      field_generators_.get(field).generatemergingcode(printer);

      printer->outdent();
      printer->print("}\n");
    }
  }

  if (last_index >= 0) {
    printer->outdent();
    printer->print("}\n");
  }

  if (descriptor_->extension_range_count() > 0) {
    printer->print("_extensions_.mergefrom(from._extensions_);\n");
  }

  if (hasunknownfields(descriptor_->file())) {
    printer->print(
      "mutable_unknown_fields()->mergefrom(from.unknown_fields());\n");
  }

  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::
generatecopyfrom(io::printer* printer) {
  if (hasdescriptormethods(descriptor_->file())) {
    // generate the generalized copyfrom (aka that which takes in the message
    // base class as a parameter).
    printer->print(
      "void $classname$::copyfrom(const ::google::protobuf::message& from) {\n",
      "classname", classname_);
    printer->indent();

    printer->print(
      "if (&from == this) return;\n"
      "clear();\n"
      "mergefrom(from);\n");

    printer->outdent();
    printer->print("}\n\n");
  }

  // generate the class-specific copyfrom.
  printer->print(
    "void $classname$::copyfrom(const $classname$& from) {\n",
    "classname", classname_);
  printer->indent();

  printer->print(
    "if (&from == this) return;\n"
    "clear();\n"
    "mergefrom(from);\n");

  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::
generatemergefromcodedstream(io::printer* printer) {
  if (descriptor_->options().message_set_wire_format()) {
    // special-case messageset.
    printer->print(
      "bool $classname$::mergepartialfromcodedstream(\n"
      "    ::google::protobuf::io::codedinputstream* input) {\n",
      "classname", classname_);

    printhandlingoptionalstaticinitializers(
      descriptor_->file(), printer,
      // with static initializers.
      "  return _extensions_.parsemessageset(input, default_instance_,\n"
      "                                      mutable_unknown_fields());\n",
      // without.
      "  return _extensions_.parsemessageset(input, &default_instance(),\n"
      "                                      mutable_unknown_fields());\n",
      // vars.
      "classname", classname_);

    printer->print(
      "}\n");
    return;
  }

  printer->print(
    "bool $classname$::mergepartialfromcodedstream(\n"
    "    ::google::protobuf::io::codedinputstream* input) {\n"
    "#define do_(expression) if (!(expression)) return false\n"
    "  ::google::protobuf::uint32 tag;\n"
    "  while ((tag = input->readtag()) != 0) {\n",
    "classname", classname_);

  printer->indent();
  printer->indent();

  if (descriptor_->field_count() > 0) {
    // we don't even want to print the switch() if we have no fields because
    // msvc dislikes switch() statements that contain only a default value.

    // note:  if we just switched on the tag rather than the field number, we
    // could avoid the need for the if() to check the wire type at the beginning
    // of each case.  however, this is actually a bit slower in practice as it
    // creates a jump table that is 8x larger and sparser, and meanwhile the
    // if()s are highly predictable.
    printer->print(
      "switch (::google::protobuf::internal::wireformatlite::gettagfieldnumber(tag)) {\n");

    printer->indent();

    scoped_array<const fielddescriptor*> ordered_fields(
      sortfieldsbynumber(descriptor_));

    for (int i = 0; i < descriptor_->field_count(); i++) {
      const fielddescriptor* field = ordered_fields[i];

      printfieldcomment(printer, field);

      printer->print(
        "case $number$: {\n",
        "number", simpleitoa(field->number()));
      printer->indent();
      const fieldgenerator& field_generator = field_generators_.get(field);

      // emit code to parse the common, expected case.
      printer->print(
        "if (::google::protobuf::internal::wireformatlite::gettagwiretype(tag) ==\n"
        "    ::google::protobuf::internal::wireformatlite::wiretype_$wiretype$) {\n",
        "wiretype", kwiretypenames[wireformat::wiretypeforfield(field)]);

      if (i > 0 || (field->is_repeated() && !field->options().packed())) {
        printer->print(
          " parse_$name$:\n",
          "name", field->name());
      }

      printer->indent();
      if (field->options().packed()) {
        field_generator.generatemergefromcodedstreamwithpacking(printer);
      } else {
        field_generator.generatemergefromcodedstream(printer);
      }
      printer->outdent();

      // emit code to parse unexpectedly packed or unpacked values.
      if (field->is_packable() && field->options().packed()) {
        printer->print(
          "} else if (::google::protobuf::internal::wireformatlite::gettagwiretype(tag)\n"
          "           == ::google::protobuf::internal::wireformatlite::\n"
          "              wiretype_$wiretype$) {\n",
          "wiretype",
          kwiretypenames[wireformat::wiretypeforfieldtype(field->type())]);
        printer->indent();
        field_generator.generatemergefromcodedstream(printer);
        printer->outdent();
      } else if (field->is_packable() && !field->options().packed()) {
        printer->print(
          "} else if (::google::protobuf::internal::wireformatlite::gettagwiretype(tag)\n"
          "           == ::google::protobuf::internal::wireformatlite::\n"
          "              wiretype_length_delimited) {\n");
        printer->indent();
        field_generator.generatemergefromcodedstreamwithpacking(printer);
        printer->outdent();
      }

      printer->print(
        "} else {\n"
        "  goto handle_uninterpreted;\n"
        "}\n");

      // switch() is slow since it can't be predicted well.  insert some if()s
      // here that attempt to predict the next tag.
      if (field->is_repeated() && !field->options().packed()) {
        // expect repeats of this field.
        printer->print(
          "if (input->expecttag($tag$)) goto parse_$name$;\n",
          "tag", simpleitoa(wireformat::maketag(field)),
          "name", field->name());
      }

      if (i + 1 < descriptor_->field_count()) {
        // expect the next field in order.
        const fielddescriptor* next_field = ordered_fields[i + 1];
        printer->print(
          "if (input->expecttag($next_tag$)) goto parse_$next_name$;\n",
          "next_tag", simpleitoa(wireformat::maketag(next_field)),
          "next_name", next_field->name());
      } else {
        // expect eof.
        // todo(kenton):  expect group end-tag?
        printer->print(
          "if (input->expectatend()) return true;\n");
      }

      printer->print(
        "break;\n");

      printer->outdent();
      printer->print("}\n\n");
    }

    printer->print(
      "default: {\n"
      "handle_uninterpreted:\n");
    printer->indent();
  }

  // is this an end-group tag?  if so, this must be the end of the message.
  printer->print(
    "if (::google::protobuf::internal::wireformatlite::gettagwiretype(tag) ==\n"
    "    ::google::protobuf::internal::wireformatlite::wiretype_end_group) {\n"
    "  return true;\n"
    "}\n");

  // handle extension ranges.
  if (descriptor_->extension_range_count() > 0) {
    printer->print(
      "if (");
    for (int i = 0; i < descriptor_->extension_range_count(); i++) {
      const descriptor::extensionrange* range =
        descriptor_->extension_range(i);
      if (i > 0) printer->print(" ||\n    ");

      uint32 start_tag = wireformatlite::maketag(
        range->start, static_cast<wireformatlite::wiretype>(0));
      uint32 end_tag = wireformatlite::maketag(
        range->end, static_cast<wireformatlite::wiretype>(0));

      if (range->end > fielddescriptor::kmaxnumber) {
        printer->print(
          "($start$u <= tag)",
          "start", simpleitoa(start_tag));
      } else {
        printer->print(
          "($start$u <= tag && tag < $end$u)",
          "start", simpleitoa(start_tag),
          "end", simpleitoa(end_tag));
      }
    }
    printer->print(") {\n");
    if (hasunknownfields(descriptor_->file())) {
      printhandlingoptionalstaticinitializers(
        descriptor_->file(), printer,
        // with static initializers.
        "  do_(_extensions_.parsefield(tag, input, default_instance_,\n"
        "                              mutable_unknown_fields()));\n",
        // without.
        "  do_(_extensions_.parsefield(tag, input, &default_instance(),\n"
        "                              mutable_unknown_fields()));\n");
    } else {
      printhandlingoptionalstaticinitializers(
        descriptor_->file(), printer,
        // with static initializers.
        "  do_(_extensions_.parsefield(tag, input, default_instance_));\n",
        // without.
        "  do_(_extensions_.parsefield(tag, input, &default_instance()));\n");
    }
    printer->print(
      "  continue;\n"
      "}\n");
  }

  // we really don't recognize this tag.  skip it.
  if (hasunknownfields(descriptor_->file())) {
    printer->print(
      "do_(::google::protobuf::internal::wireformat::skipfield(\n"
      "      input, tag, mutable_unknown_fields()));\n");
  } else {
    printer->print(
      "do_(::google::protobuf::internal::wireformatlite::skipfield(input, tag));\n");
  }

  if (descriptor_->field_count() > 0) {
    printer->print("break;\n");
    printer->outdent();
    printer->print("}\n");    // default:
    printer->outdent();
    printer->print("}\n");    // switch
  }

  printer->outdent();
  printer->outdent();
  printer->print(
    "  }\n"                   // while
    "  return true;\n"
    "#undef do_\n"
    "}\n");
}

void messagegenerator::generateserializeonefield(
    io::printer* printer, const fielddescriptor* field, bool to_array) {
  printfieldcomment(printer, field);

  if (!field->is_repeated()) {
    printer->print(
      "if (has_$name$()) {\n",
      "name", fieldname(field));
    printer->indent();
  }

  if (to_array) {
    field_generators_.get(field).generateserializewithcachedsizestoarray(
        printer);
  } else {
    field_generators_.get(field).generateserializewithcachedsizes(printer);
  }

  if (!field->is_repeated()) {
    printer->outdent();
    printer->print("}\n");
  }
  printer->print("\n");
}

void messagegenerator::generateserializeoneextensionrange(
    io::printer* printer, const descriptor::extensionrange* range,
    bool to_array) {
  map<string, string> vars;
  vars["start"] = simpleitoa(range->start);
  vars["end"] = simpleitoa(range->end);
  printer->print(vars,
    "// extension range [$start$, $end$)\n");
  if (to_array) {
    printer->print(vars,
      "target = _extensions_.serializewithcachedsizestoarray(\n"
      "    $start$, $end$, target);\n\n");
  } else {
    printer->print(vars,
      "_extensions_.serializewithcachedsizes(\n"
      "    $start$, $end$, output);\n\n");
  }
}

void messagegenerator::
generateserializewithcachedsizes(io::printer* printer) {
  if (descriptor_->options().message_set_wire_format()) {
    // special-case messageset.
    printer->print(
      "void $classname$::serializewithcachedsizes(\n"
      "    ::google::protobuf::io::codedoutputstream* output) const {\n"
      "  _extensions_.serializemessagesetwithcachedsizes(output);\n",
      "classname", classname_);
    if (hasunknownfields(descriptor_->file())) {
      printer->print(
        "  ::google::protobuf::internal::wireformat::serializeunknownmessagesetitems(\n"
        "      unknown_fields(), output);\n");
    }
    printer->print(
      "}\n");
    return;
  }

  printer->print(
    "void $classname$::serializewithcachedsizes(\n"
    "    ::google::protobuf::io::codedoutputstream* output) const {\n",
    "classname", classname_);
  printer->indent();

  generateserializewithcachedsizesbody(printer, false);

  printer->outdent();
  printer->print(
    "}\n");
}

void messagegenerator::
generateserializewithcachedsizestoarray(io::printer* printer) {
  if (descriptor_->options().message_set_wire_format()) {
    // special-case messageset.
    printer->print(
      "::google::protobuf::uint8* $classname$::serializewithcachedsizestoarray(\n"
      "    ::google::protobuf::uint8* target) const {\n"
      "  target =\n"
      "      _extensions_.serializemessagesetwithcachedsizestoarray(target);\n",
      "classname", classname_);
    if (hasunknownfields(descriptor_->file())) {
      printer->print(
        "  target = ::google::protobuf::internal::wireformat::\n"
        "             serializeunknownmessagesetitemstoarray(\n"
        "               unknown_fields(), target);\n");
    }
    printer->print(
      "  return target;\n"
      "}\n");
    return;
  }

  printer->print(
    "::google::protobuf::uint8* $classname$::serializewithcachedsizestoarray(\n"
    "    ::google::protobuf::uint8* target) const {\n",
    "classname", classname_);
  printer->indent();

  generateserializewithcachedsizesbody(printer, true);

  printer->outdent();
  printer->print(
    "  return target;\n"
    "}\n");
}

void messagegenerator::
generateserializewithcachedsizesbody(io::printer* printer, bool to_array) {
  scoped_array<const fielddescriptor*> ordered_fields(
    sortfieldsbynumber(descriptor_));

  vector<const descriptor::extensionrange*> sorted_extensions;
  for (int i = 0; i < descriptor_->extension_range_count(); ++i) {
    sorted_extensions.push_back(descriptor_->extension_range(i));
  }
  sort(sorted_extensions.begin(), sorted_extensions.end(),
       extensionrangesorter());

  // merge the fields and the extension ranges, both sorted by field number.
  int i, j;
  for (i = 0, j = 0;
       i < descriptor_->field_count() || j < sorted_extensions.size();
       ) {
    if (i == descriptor_->field_count()) {
      generateserializeoneextensionrange(printer,
                                         sorted_extensions[j++],
                                         to_array);
    } else if (j == sorted_extensions.size()) {
      generateserializeonefield(printer, ordered_fields[i++], to_array);
    } else if (ordered_fields[i]->number() < sorted_extensions[j]->start) {
      generateserializeonefield(printer, ordered_fields[i++], to_array);
    } else {
      generateserializeoneextensionrange(printer,
                                         sorted_extensions[j++],
                                         to_array);
    }
  }

  if (hasunknownfields(descriptor_->file())) {
    printer->print("if (!unknown_fields().empty()) {\n");
    printer->indent();
    if (to_array) {
      printer->print(
        "target = "
            "::google::protobuf::internal::wireformat::serializeunknownfieldstoarray(\n"
        "    unknown_fields(), target);\n");
    } else {
      printer->print(
        "::google::protobuf::internal::wireformat::serializeunknownfields(\n"
        "    unknown_fields(), output);\n");
    }
    printer->outdent();

    printer->print(
      "}\n");
  }
}

void messagegenerator::
generatebytesize(io::printer* printer) {
  if (descriptor_->options().message_set_wire_format()) {
    // special-case messageset.
    printer->print(
      "int $classname$::bytesize() const {\n"
      "  int total_size = _extensions_.messagesetbytesize();\n",
      "classname", classname_);
    if (hasunknownfields(descriptor_->file())) {
      printer->print(
        "  total_size += ::google::protobuf::internal::wireformat::\n"
        "      computeunknownmessagesetitemssize(unknown_fields());\n");
    }
    printer->print(
      "  google_safe_concurrent_writes_begin();\n"
      "  _cached_size_ = total_size;\n"
      "  google_safe_concurrent_writes_end();\n"
      "  return total_size;\n"
      "}\n");
    return;
  }

  printer->print(
    "int $classname$::bytesize() const {\n",
    "classname", classname_);
  printer->indent();
  printer->print(
    "int total_size = 0;\n"
    "\n");

  int last_index = -1;

  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (!field->is_repeated()) {
      // see above in generateclear for an explanation of this.
      // todo(kenton):  share code?  unclear how to do so without
      //   over-engineering.
      if ((i / 8) != (last_index / 8) ||
          last_index < 0) {
        if (last_index >= 0) {
          printer->outdent();
          printer->print("}\n");
        }
        printer->print(
          "if (_has_bits_[$index$ / 32] & (0xffu << ($index$ % 32))) {\n",
          "index", simpleitoa(field->index()));
        printer->indent();
      }
      last_index = i;

      printfieldcomment(printer, field);

      printer->print(
        "if (has_$name$()) {\n",
        "name", fieldname(field));
      printer->indent();

      field_generators_.get(field).generatebytesize(printer);

      printer->outdent();
      printer->print(
        "}\n"
        "\n");
    }
  }

  if (last_index >= 0) {
    printer->outdent();
    printer->print("}\n");
  }

  // repeated fields don't use _has_bits_ so we count them in a separate
  // pass.
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);

    if (field->is_repeated()) {
      printfieldcomment(printer, field);
      field_generators_.get(field).generatebytesize(printer);
      printer->print("\n");
    }
  }

  if (descriptor_->extension_range_count() > 0) {
    printer->print(
      "total_size += _extensions_.bytesize();\n"
      "\n");
  }

  if (hasunknownfields(descriptor_->file())) {
    printer->print("if (!unknown_fields().empty()) {\n");
    printer->indent();
    printer->print(
      "total_size +=\n"
      "  ::google::protobuf::internal::wireformat::computeunknownfieldssize(\n"
      "    unknown_fields());\n");
    printer->outdent();
    printer->print("}\n");
  }

  // we update _cached_size_ even though this is a const method.  in theory,
  // this is not thread-compatible, because concurrent writes have undefined
  // results.  in practice, since any concurrent writes will be writing the
  // exact same value, it works on all common processors.  in a future version
  // of c++, _cached_size_ should be made into an atomic<int>.
  printer->print(
    "google_safe_concurrent_writes_begin();\n"
    "_cached_size_ = total_size;\n"
    "google_safe_concurrent_writes_end();\n"
    "return total_size;\n");

  printer->outdent();
  printer->print("}\n");
}

void messagegenerator::
generateisinitialized(io::printer* printer) {
  printer->print(
    "bool $classname$::isinitialized() const {\n",
    "classname", classname_);
  printer->indent();

  // check that all required fields in this message are set.  we can do this
  // most efficiently by checking 32 "has bits" at a time.
  int has_bits_array_size = (descriptor_->field_count() + 31) / 32;
  for (int i = 0; i < has_bits_array_size; i++) {
    uint32 mask = 0;
    for (int bit = 0; bit < 32; bit++) {
      int index = i * 32 + bit;
      if (index >= descriptor_->field_count()) break;
      const fielddescriptor* field = descriptor_->field(index);

      if (field->is_required()) {
        mask |= 1 << bit;
      }
    }

    if (mask != 0) {
      char buffer[kfasttobuffersize];
      printer->print(
        "if ((_has_bits_[$i$] & 0x$mask$) != 0x$mask$) return false;\n",
        "i", simpleitoa(i),
        "mask", fasthex32tobuffer(mask, buffer));
    }
  }

  // now check that all embedded messages are initialized.
  printer->print("\n");
  for (int i = 0; i < descriptor_->field_count(); i++) {
    const fielddescriptor* field = descriptor_->field(i);
    if (field->cpp_type() == fielddescriptor::cpptype_message &&
        !shouldignorerequiredfieldcheck(field) &&
        hasrequiredfields(field->message_type())) {
      if (field->is_repeated()) {
        printer->print(
          "for (int i = 0; i < $name$_size(); i++) {\n"
          "  if (!this->$name$(i).isinitialized()) return false;\n"
          "}\n",
          "name", fieldname(field));
      } else {
        printer->print(
          "if (has_$name$()) {\n"
          "  if (!this->$name$().isinitialized()) return false;\n"
          "}\n",
          "name", fieldname(field));
      }
    }
  }

  if (descriptor_->extension_range_count() > 0) {
    printer->print(
      "\n"
      "if (!_extensions_.isinitialized()) return false;");
  }

  printer->outdent();
  printer->print(
    "  return true;\n"
    "}\n");
}


}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
