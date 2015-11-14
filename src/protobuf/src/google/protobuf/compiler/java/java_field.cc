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

#include <google/protobuf/compiler/java/java_field.h>
#include <google/protobuf/compiler/java/java_helpers.h>
#include <google/protobuf/compiler/java/java_primitive_field.h>
#include <google/protobuf/compiler/java/java_enum_field.h>
#include <google/protobuf/compiler/java/java_message_field.h>
#include <google/protobuf/compiler/java/java_string_field.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

fieldgenerator::~fieldgenerator() {}

void fieldgenerator::generateparsingcodefrompacked(io::printer* printer) const {
  // reaching here indicates a bug. cases are:
  //   - this fieldgenerator should support packing, but this method should be
  //     overridden.
  //   - this fieldgenerator doesn't support packing, and this method should
  //     never have been called.
  google_log(fatal) << "generateparsingcodefrompacked() "
             << "called on field generator that does not support packing.";
}

fieldgeneratormap::fieldgeneratormap(const descriptor* descriptor)
  : descriptor_(descriptor),
    field_generators_(
      new scoped_ptr<fieldgenerator>[descriptor->field_count()]),
    extension_generators_(
      new scoped_ptr<fieldgenerator>[descriptor->extension_count()]) {

  // construct all the fieldgenerators and assign them bit indices for their
  // bit fields.
  int messagebitindex = 0;
  int builderbitindex = 0;
  for (int i = 0; i < descriptor->field_count(); i++) {
    fieldgenerator* generator = makegenerator(descriptor->field(i),
        messagebitindex, builderbitindex);
    field_generators_[i].reset(generator);
    messagebitindex += generator->getnumbitsformessage();
    builderbitindex += generator->getnumbitsforbuilder();
  }
  for (int i = 0; i < descriptor->extension_count(); i++) {
    fieldgenerator* generator = makegenerator(descriptor->extension(i),
        messagebitindex, builderbitindex);
    extension_generators_[i].reset(generator);
    messagebitindex += generator->getnumbitsformessage();
    builderbitindex += generator->getnumbitsforbuilder();
  }
}

fieldgenerator* fieldgeneratormap::makegenerator(
    const fielddescriptor* field, int messagebitindex, int builderbitindex) {
  if (field->is_repeated()) {
    switch (getjavatype(field)) {
      case javatype_message:
        return new repeatedmessagefieldgenerator(
            field, messagebitindex, builderbitindex);
      case javatype_enum:
        return new repeatedenumfieldgenerator(
            field, messagebitindex, builderbitindex);
      case javatype_string:
        return new repeatedstringfieldgenerator(
            field, messagebitindex, builderbitindex);
      default:
        return new repeatedprimitivefieldgenerator(
            field, messagebitindex, builderbitindex);
    }
  } else {
    switch (getjavatype(field)) {
      case javatype_message:
        return new messagefieldgenerator(
            field, messagebitindex, builderbitindex);
      case javatype_enum:
        return new enumfieldgenerator(
            field, messagebitindex, builderbitindex);
      case javatype_string:
        return new stringfieldgenerator(
            field, messagebitindex, builderbitindex);
      default:
        return new primitivefieldgenerator(
            field, messagebitindex, builderbitindex);
    }
  }
}

fieldgeneratormap::~fieldgeneratormap() {}

const fieldgenerator& fieldgeneratormap::get(
    const fielddescriptor* field) const {
  google_check_eq(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}

const fieldgenerator& fieldgeneratormap::get_extension(int index) const {
  return *extension_generators_[index];
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
