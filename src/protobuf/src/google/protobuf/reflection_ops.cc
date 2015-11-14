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

#include <string>
#include <vector>

#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace internal {

void reflectionops::copy(const message& from, message* to) {
  if (&from == to) return;
  clear(to);
  merge(from, to);
}

void reflectionops::merge(const message& from, message* to) {
  google_check_ne(&from, to);

  const descriptor* descriptor = from.getdescriptor();
  google_check_eq(to->getdescriptor(), descriptor)
    << "tried to merge messages of different types.";

  const reflection* from_reflection = from.getreflection();
  const reflection* to_reflection = to->getreflection();

  vector<const fielddescriptor*> fields;
  from_reflection->listfields(from, &fields);
  for (int i = 0; i < fields.size(); i++) {
    const fielddescriptor* field = fields[i];

    if (field->is_repeated()) {
      int count = from_reflection->fieldsize(from, field);
      for (int j = 0; j < count; j++) {
        switch (field->cpp_type()) {
#define handle_type(cpptype, method)                                     \
          case fielddescriptor::cpptype_##cpptype:                       \
            to_reflection->add##method(to, field,                        \
              from_reflection->getrepeated##method(from, field, j));     \
            break;

          handle_type(int32 , int32 );
          handle_type(int64 , int64 );
          handle_type(uint32, uint32);
          handle_type(uint64, uint64);
          handle_type(float , float );
          handle_type(double, double);
          handle_type(bool  , bool  );
          handle_type(string, string);
          handle_type(enum  , enum  );
#undef handle_type

          case fielddescriptor::cpptype_message:
            to_reflection->addmessage(to, field)->mergefrom(
              from_reflection->getrepeatedmessage(from, field, j));
            break;
        }
      }
    } else {
      switch (field->cpp_type()) {
#define handle_type(cpptype, method)                                        \
        case fielddescriptor::cpptype_##cpptype:                            \
          to_reflection->set##method(to, field,                             \
            from_reflection->get##method(from, field));                     \
          break;

        handle_type(int32 , int32 );
        handle_type(int64 , int64 );
        handle_type(uint32, uint32);
        handle_type(uint64, uint64);
        handle_type(float , float );
        handle_type(double, double);
        handle_type(bool  , bool  );
        handle_type(string, string);
        handle_type(enum  , enum  );
#undef handle_type

        case fielddescriptor::cpptype_message:
          to_reflection->mutablemessage(to, field)->mergefrom(
            from_reflection->getmessage(from, field));
          break;
      }
    }
  }

  to_reflection->mutableunknownfields(to)->mergefrom(
    from_reflection->getunknownfields(from));
}

void reflectionops::clear(message* message) {
  const reflection* reflection = message->getreflection();

  vector<const fielddescriptor*> fields;
  reflection->listfields(*message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    reflection->clearfield(message, fields[i]);
  }

  reflection->mutableunknownfields(message)->clear();
}

bool reflectionops::isinitialized(const message& message) {
  const descriptor* descriptor = message.getdescriptor();
  const reflection* reflection = message.getreflection();

  // check required fields of this message.
  for (int i = 0; i < descriptor->field_count(); i++) {
    if (descriptor->field(i)->is_required()) {
      if (!reflection->hasfield(message, descriptor->field(i))) {
        return false;
      }
    }
  }

  // check that sub-messages are initialized.
  vector<const fielddescriptor*> fields;
  reflection->listfields(message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    const fielddescriptor* field = fields[i];
    if (field->cpp_type() == fielddescriptor::cpptype_message) {

      if (field->is_repeated()) {
        int size = reflection->fieldsize(message, field);

        for (int j = 0; j < size; j++) {
          if (!reflection->getrepeatedmessage(message, field, j)
                          .isinitialized()) {
            return false;
          }
        }
      } else {
        if (!reflection->getmessage(message, field).isinitialized()) {
          return false;
        }
      }
    }
  }

  return true;
}

void reflectionops::discardunknownfields(message* message) {
  const reflection* reflection = message->getreflection();

  reflection->mutableunknownfields(message)->clear();

  vector<const fielddescriptor*> fields;
  reflection->listfields(*message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    const fielddescriptor* field = fields[i];
    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      if (field->is_repeated()) {
        int size = reflection->fieldsize(*message, field);
        for (int j = 0; j < size; j++) {
          reflection->mutablerepeatedmessage(message, field, j)
                    ->discardunknownfields();
        }
      } else {
        reflection->mutablemessage(message, field)->discardunknownfields();
      }
    }
  }
}

static string submessageprefix(const string& prefix,
                               const fielddescriptor* field,
                               int index) {
  string result(prefix);
  if (field->is_extension()) {
    result.append("(");
    result.append(field->full_name());
    result.append(")");
  } else {
    result.append(field->name());
  }
  if (index != -1) {
    result.append("[");
    result.append(simpleitoa(index));
    result.append("]");
  }
  result.append(".");
  return result;
}

void reflectionops::findinitializationerrors(
    const message& message,
    const string& prefix,
    vector<string>* errors) {
  const descriptor* descriptor = message.getdescriptor();
  const reflection* reflection = message.getreflection();

  // check required fields of this message.
  for (int i = 0; i < descriptor->field_count(); i++) {
    if (descriptor->field(i)->is_required()) {
      if (!reflection->hasfield(message, descriptor->field(i))) {
        errors->push_back(prefix + descriptor->field(i)->name());
      }
    }
  }

  // check sub-messages.
  vector<const fielddescriptor*> fields;
  reflection->listfields(message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    const fielddescriptor* field = fields[i];
    if (field->cpp_type() == fielddescriptor::cpptype_message) {

      if (field->is_repeated()) {
        int size = reflection->fieldsize(message, field);

        for (int j = 0; j < size; j++) {
          const message& sub_message =
            reflection->getrepeatedmessage(message, field, j);
          findinitializationerrors(sub_message,
                                   submessageprefix(prefix, field, j),
                                   errors);
        }
      } else {
        const message& sub_message = reflection->getmessage(message, field);
        findinitializationerrors(sub_message,
                                 submessageprefix(prefix, field, -1),
                                 errors);
      }
    }
  }
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
