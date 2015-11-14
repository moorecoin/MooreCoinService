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

#include <google/protobuf/compiler/cpp/cpp_field.h>
#include <google/protobuf/compiler/cpp/cpp_helpers.h>
#include <google/protobuf/compiler/cpp/cpp_primitive_field.h>
#include <google/protobuf/compiler/cpp/cpp_string_field.h>
#include <google/protobuf/compiler/cpp/cpp_enum_field.h>
#include <google/protobuf/compiler/cpp/cpp_message_field.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace cpp {

using internal::wireformat;

void setcommonfieldvariables(const fielddescriptor* descriptor,
                             map<string, string>* variables,
                             const options& options) {
  (*variables)["name"] = fieldname(descriptor);
  (*variables)["index"] = simpleitoa(descriptor->index());
  (*variables)["number"] = simpleitoa(descriptor->number());
  (*variables)["classname"] = classname(fieldscope(descriptor), false);
  (*variables)["declared_type"] = declaredtypemethodname(descriptor->type());

  (*variables)["tag_size"] = simpleitoa(
    wireformat::tagsize(descriptor->number(), descriptor->type()));
  (*variables)["deprecation"] = descriptor->options().deprecated()
      ? " protobuf_deprecated" : "";

  (*variables)["cppget"] = "get";
}

fieldgenerator::~fieldgenerator() {}

void fieldgenerator::
generatemergefromcodedstreamwithpacking(io::printer* printer) const {
  // reaching here indicates a bug. cases are:
  //   - this fieldgenerator should support packing, but this method should be
  //     overridden.
  //   - this fieldgenerator doesn't support packing, and this method should
  //     never have been called.
  google_log(fatal) << "generatemergefromcodedstreamwithpacking() "
             << "called on field generator that does not support packing.";

}

fieldgeneratormap::fieldgeneratormap(const descriptor* descriptor,
                                     const options& options)
  : descriptor_(descriptor),
    field_generators_(new scoped_ptr<fieldgenerator>[descriptor->field_count()]) {
  // construct all the fieldgenerators.
  for (int i = 0; i < descriptor->field_count(); i++) {
    field_generators_[i].reset(makegenerator(descriptor->field(i), options));
  }
}

fieldgenerator* fieldgeneratormap::makegenerator(const fielddescriptor* field,
                                                 const options& options) {
  if (field->is_repeated()) {
    switch (field->cpp_type()) {
      case fielddescriptor::cpptype_message:
        return new repeatedmessagefieldgenerator(field, options);
      case fielddescriptor::cpptype_string:
        switch (field->options().ctype()) {
          default:  // repeatedstringfieldgenerator handles unknown ctypes.
          case fieldoptions::string:
            return new repeatedstringfieldgenerator(field, options);
        }
      case fielddescriptor::cpptype_enum:
        return new repeatedenumfieldgenerator(field, options);
      default:
        return new repeatedprimitivefieldgenerator(field, options);
    }
  } else {
    switch (field->cpp_type()) {
      case fielddescriptor::cpptype_message:
        return new messagefieldgenerator(field, options);
      case fielddescriptor::cpptype_string:
        switch (field->options().ctype()) {
          default:  // stringfieldgenerator handles unknown ctypes.
          case fieldoptions::string:
            return new stringfieldgenerator(field, options);
        }
      case fielddescriptor::cpptype_enum:
        return new enumfieldgenerator(field, options);
      default:
        return new primitivefieldgenerator(field, options);
    }
  }
}

fieldgeneratormap::~fieldgeneratormap() {}

const fieldgenerator& fieldgeneratormap::get(
    const fielddescriptor* field) const {
  google_check_eq(field->containing_type(), descriptor_);
  return *field_generators_[field->index()];
}


}  // namespace cpp
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
