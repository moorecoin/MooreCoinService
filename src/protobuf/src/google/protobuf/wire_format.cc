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

#include <stack>
#include <string>
#include <vector>

#include <google/protobuf/wire_format.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/unknown_field_set.h>



namespace google {
namespace protobuf {
namespace internal {

namespace {

// this function turns out to be convenient when using some macros later.
inline int getenumnumber(const enumvaluedescriptor* descriptor) {
  return descriptor->number();
}

}  // anonymous namespace

// ===================================================================

bool unknownfieldsetfieldskipper::skipfield(
    io::codedinputstream* input, uint32 tag) {
  return wireformat::skipfield(input, tag, unknown_fields_);
}

bool unknownfieldsetfieldskipper::skipmessage(io::codedinputstream* input) {
  return wireformat::skipmessage(input, unknown_fields_);
}

void unknownfieldsetfieldskipper::skipunknownenum(
    int field_number, int value) {
  unknown_fields_->addvarint(field_number, value);
}

bool wireformat::skipfield(io::codedinputstream* input, uint32 tag,
                           unknownfieldset* unknown_fields) {
  int number = wireformatlite::gettagfieldnumber(tag);

  switch (wireformatlite::gettagwiretype(tag)) {
    case wireformatlite::wiretype_varint: {
      uint64 value;
      if (!input->readvarint64(&value)) return false;
      if (unknown_fields != null) unknown_fields->addvarint(number, value);
      return true;
    }
    case wireformatlite::wiretype_fixed64: {
      uint64 value;
      if (!input->readlittleendian64(&value)) return false;
      if (unknown_fields != null) unknown_fields->addfixed64(number, value);
      return true;
    }
    case wireformatlite::wiretype_length_delimited: {
      uint32 length;
      if (!input->readvarint32(&length)) return false;
      if (unknown_fields == null) {
        if (!input->skip(length)) return false;
      } else {
        if (!input->readstring(unknown_fields->addlengthdelimited(number),
                               length)) {
          return false;
        }
      }
      return true;
    }
    case wireformatlite::wiretype_start_group: {
      if (!input->incrementrecursiondepth()) return false;
      if (!skipmessage(input, (unknown_fields == null) ?
                              null : unknown_fields->addgroup(number))) {
        return false;
      }
      input->decrementrecursiondepth();
      // check that the ending tag matched the starting tag.
      if (!input->lasttagwas(wireformatlite::maketag(
          wireformatlite::gettagfieldnumber(tag),
          wireformatlite::wiretype_end_group))) {
        return false;
      }
      return true;
    }
    case wireformatlite::wiretype_end_group: {
      return false;
    }
    case wireformatlite::wiretype_fixed32: {
      uint32 value;
      if (!input->readlittleendian32(&value)) return false;
      if (unknown_fields != null) unknown_fields->addfixed32(number, value);
      return true;
    }
    default: {
      return false;
    }
  }
}

bool wireformat::skipmessage(io::codedinputstream* input,
                             unknownfieldset* unknown_fields) {
  while(true) {
    uint32 tag = input->readtag();
    if (tag == 0) {
      // end of input.  this is a valid place to end, so return true.
      return true;
    }

    wireformatlite::wiretype wire_type = wireformatlite::gettagwiretype(tag);

    if (wire_type == wireformatlite::wiretype_end_group) {
      // must be the end of the message.
      return true;
    }

    if (!skipfield(input, tag, unknown_fields)) return false;
  }
}

void wireformat::serializeunknownfields(const unknownfieldset& unknown_fields,
                                        io::codedoutputstream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);
    switch (field.type()) {
      case unknownfield::type_varint:
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_varint));
        output->writevarint64(field.varint());
        break;
      case unknownfield::type_fixed32:
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_fixed32));
        output->writelittleendian32(field.fixed32());
        break;
      case unknownfield::type_fixed64:
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_fixed64));
        output->writelittleendian64(field.fixed64());
        break;
      case unknownfield::type_length_delimited:
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_length_delimited));
        output->writevarint32(field.length_delimited().size());
        output->writestring(field.length_delimited());
        break;
      case unknownfield::type_group:
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_start_group));
        serializeunknownfields(field.group(), output);
        output->writevarint32(wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_end_group));
        break;
    }
  }
}

uint8* wireformat::serializeunknownfieldstoarray(
    const unknownfieldset& unknown_fields,
    uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);

    switch (field.type()) {
      case unknownfield::type_varint:
        target = wireformatlite::writeint64toarray(
            field.number(), field.varint(), target);
        break;
      case unknownfield::type_fixed32:
        target = wireformatlite::writefixed32toarray(
            field.number(), field.fixed32(), target);
        break;
      case unknownfield::type_fixed64:
        target = wireformatlite::writefixed64toarray(
            field.number(), field.fixed64(), target);
        break;
      case unknownfield::type_length_delimited:
        target = wireformatlite::writebytestoarray(
            field.number(), field.length_delimited(), target);
        break;
      case unknownfield::type_group:
        target = wireformatlite::writetagtoarray(
            field.number(), wireformatlite::wiretype_start_group, target);
        target = serializeunknownfieldstoarray(field.group(), target);
        target = wireformatlite::writetagtoarray(
            field.number(), wireformatlite::wiretype_end_group, target);
        break;
    }
  }
  return target;
}

void wireformat::serializeunknownmessagesetitems(
    const unknownfieldset& unknown_fields,
    io::codedoutputstream* output) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);
    // the only unknown fields that are allowed to exist in a messageset are
    // messages, which are length-delimited.
    if (field.type() == unknownfield::type_length_delimited) {
      // start group.
      output->writevarint32(wireformatlite::kmessagesetitemstarttag);

      // write type id.
      output->writevarint32(wireformatlite::kmessagesettypeidtag);
      output->writevarint32(field.number());

      // write message.
      output->writevarint32(wireformatlite::kmessagesetmessagetag);
      field.serializelengthdelimitednotag(output);

      // end group.
      output->writevarint32(wireformatlite::kmessagesetitemendtag);
    }
  }
}

uint8* wireformat::serializeunknownmessagesetitemstoarray(
    const unknownfieldset& unknown_fields,
    uint8* target) {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);

    // the only unknown fields that are allowed to exist in a messageset are
    // messages, which are length-delimited.
    if (field.type() == unknownfield::type_length_delimited) {
      // start group.
      target = io::codedoutputstream::writetagtoarray(
          wireformatlite::kmessagesetitemstarttag, target);

      // write type id.
      target = io::codedoutputstream::writetagtoarray(
          wireformatlite::kmessagesettypeidtag, target);
      target = io::codedoutputstream::writevarint32toarray(
          field.number(), target);

      // write message.
      target = io::codedoutputstream::writetagtoarray(
          wireformatlite::kmessagesetmessagetag, target);
      target = field.serializelengthdelimitednotagtoarray(target);

      // end group.
      target = io::codedoutputstream::writetagtoarray(
          wireformatlite::kmessagesetitemendtag, target);
    }
  }

  return target;
}

int wireformat::computeunknownfieldssize(
    const unknownfieldset& unknown_fields) {
  int size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);

    switch (field.type()) {
      case unknownfield::type_varint:
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_varint));
        size += io::codedoutputstream::varintsize64(field.varint());
        break;
      case unknownfield::type_fixed32:
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_fixed32));
        size += sizeof(int32);
        break;
      case unknownfield::type_fixed64:
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_fixed64));
        size += sizeof(int64);
        break;
      case unknownfield::type_length_delimited:
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_length_delimited));
        size += io::codedoutputstream::varintsize32(
            field.length_delimited().size());
        size += field.length_delimited().size();
        break;
      case unknownfield::type_group:
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_start_group));
        size += computeunknownfieldssize(field.group());
        size += io::codedoutputstream::varintsize32(
            wireformatlite::maketag(field.number(),
            wireformatlite::wiretype_end_group));
        break;
    }
  }

  return size;
}

int wireformat::computeunknownmessagesetitemssize(
    const unknownfieldset& unknown_fields) {
  int size = 0;
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);

    // the only unknown fields that are allowed to exist in a messageset are
    // messages, which are length-delimited.
    if (field.type() == unknownfield::type_length_delimited) {
      size += wireformatlite::kmessagesetitemtagssize;
      size += io::codedoutputstream::varintsize32(field.number());

      int field_size = field.getlengthdelimitedsize();
      size += io::codedoutputstream::varintsize32(field_size);
      size += field_size;
    }
  }

  return size;
}

// ===================================================================

bool wireformat::parseandmergepartial(io::codedinputstream* input,
                                      message* message) {
  const descriptor* descriptor = message->getdescriptor();
  const reflection* message_reflection = message->getreflection();

  while(true) {
    uint32 tag = input->readtag();
    if (tag == 0) {
      // end of input.  this is a valid place to end, so return true.
      return true;
    }

    if (wireformatlite::gettagwiretype(tag) ==
        wireformatlite::wiretype_end_group) {
      // must be the end of the message.
      return true;
    }

    const fielddescriptor* field = null;

    if (descriptor != null) {
      int field_number = wireformatlite::gettagfieldnumber(tag);
      field = descriptor->findfieldbynumber(field_number);

      // if that failed, check if the field is an extension.
      if (field == null && descriptor->isextensionnumber(field_number)) {
        if (input->getextensionpool() == null) {
          field = message_reflection->findknownextensionbynumber(field_number);
        } else {
          field = input->getextensionpool()
                       ->findextensionbynumber(descriptor, field_number);
        }
      }

      // if that failed, but we're a messageset, and this is the tag for a
      // messageset item, then parse that.
      if (field == null &&
          descriptor->options().message_set_wire_format() &&
          tag == wireformatlite::kmessagesetitemstarttag) {
        if (!parseandmergemessagesetitem(input, message)) {
          return false;
        }
        continue;  // skip parseandmergefield(); already taken care of.
      }
    }

    if (!parseandmergefield(tag, field, message, input)) {
      return false;
    }
  }
}

bool wireformat::parseandmergefield(
    uint32 tag,
    const fielddescriptor* field,        // may be null for unknown
    message* message,
    io::codedinputstream* input) {
  const reflection* message_reflection = message->getreflection();

  enum { unknown, normal_format, packed_format } value_format;

  if (field == null) {
    value_format = unknown;
  } else if (wireformatlite::gettagwiretype(tag) ==
             wiretypeforfieldtype(field->type())) {
    value_format = normal_format;
  } else if (field->is_packable() &&
             wireformatlite::gettagwiretype(tag) ==
             wireformatlite::wiretype_length_delimited) {
    value_format = packed_format;
  } else {
    // we don't recognize this field. either the field number is unknown
    // or the wire type doesn't match. put it in our unknown field set.
    value_format = unknown;
  }

  if (value_format == unknown) {
    return skipfield(input, tag,
                     message_reflection->mutableunknownfields(message));
  } else if (value_format == packed_format) {
    uint32 length;
    if (!input->readvarint32(&length)) return false;
    io::codedinputstream::limit limit = input->pushlimit(length);

    switch (field->type()) {
#define handle_packed_type(type, cpptype, cpptype_method)                      \
      case fielddescriptor::type_##type: {                                     \
        while (input->bytesuntillimit() > 0) {                                 \
          cpptype value;                                                       \
          if (!wireformatlite::readprimitive<                                  \
                cpptype, wireformatlite::type_##type>(input, &value))          \
            return false;                                                      \
          message_reflection->add##cpptype_method(message, field, value);      \
        }                                                                      \
        break;                                                                 \
      }

      handle_packed_type( int32,  int32,  int32)
      handle_packed_type( int64,  int64,  int64)
      handle_packed_type(sint32,  int32,  int32)
      handle_packed_type(sint64,  int64,  int64)
      handle_packed_type(uint32, uint32, uint32)
      handle_packed_type(uint64, uint64, uint64)

      handle_packed_type( fixed32, uint32, uint32)
      handle_packed_type( fixed64, uint64, uint64)
      handle_packed_type(sfixed32,  int32,  int32)
      handle_packed_type(sfixed64,  int64,  int64)

      handle_packed_type(float , float , float )
      handle_packed_type(double, double, double)

      handle_packed_type(bool, bool, bool)
#undef handle_packed_type

      case fielddescriptor::type_enum: {
        while (input->bytesuntillimit() > 0) {
          int value;
          if (!wireformatlite::readprimitive<int, wireformatlite::type_enum>(
                  input, &value)) return false;
          const enumvaluedescriptor* enum_value =
              field->enum_type()->findvaluebynumber(value);
          if (enum_value != null) {
            message_reflection->addenum(message, field, enum_value);
          }
        }

        break;
      }

      case fielddescriptor::type_string:
      case fielddescriptor::type_group:
      case fielddescriptor::type_message:
      case fielddescriptor::type_bytes:
        // can't have packed fields of these types: these should be caught by
        // the protocol compiler.
        return false;
        break;
    }

    input->poplimit(limit);
  } else {
    // non-packed value (value_format == normal_format)
    switch (field->type()) {
#define handle_type(type, cpptype, cpptype_method)                            \
      case fielddescriptor::type_##type: {                                    \
        cpptype value;                                                        \
        if (!wireformatlite::readprimitive<                                   \
                cpptype, wireformatlite::type_##type>(input, &value))         \
          return false;                                                       \
        if (field->is_repeated()) {                                           \
          message_reflection->add##cpptype_method(message, field, value);     \
        } else {                                                              \
          message_reflection->set##cpptype_method(message, field, value);     \
        }                                                                     \
        break;                                                                \
      }

      handle_type( int32,  int32,  int32)
      handle_type( int64,  int64,  int64)
      handle_type(sint32,  int32,  int32)
      handle_type(sint64,  int64,  int64)
      handle_type(uint32, uint32, uint32)
      handle_type(uint64, uint64, uint64)

      handle_type( fixed32, uint32, uint32)
      handle_type( fixed64, uint64, uint64)
      handle_type(sfixed32,  int32,  int32)
      handle_type(sfixed64,  int64,  int64)

      handle_type(float , float , float )
      handle_type(double, double, double)

      handle_type(bool, bool, bool)
#undef handle_type

      case fielddescriptor::type_enum: {
        int value;
        if (!wireformatlite::readprimitive<int, wireformatlite::type_enum>(
                input, &value)) return false;
        const enumvaluedescriptor* enum_value =
          field->enum_type()->findvaluebynumber(value);
        if (enum_value != null) {
          if (field->is_repeated()) {
            message_reflection->addenum(message, field, enum_value);
          } else {
            message_reflection->setenum(message, field, enum_value);
          }
        } else {
          // the enum value is not one of the known values.  add it to the
          // unknownfieldset.
          int64 sign_extended_value = static_cast<int64>(value);
          message_reflection->mutableunknownfields(message)
                            ->addvarint(wireformatlite::gettagfieldnumber(tag),
                                        sign_extended_value);
        }
        break;
      }

      // handle strings separately so that we can optimize the ctype=cord case.
      case fielddescriptor::type_string: {
        string value;
        if (!wireformatlite::readstring(input, &value)) return false;
        verifyutf8string(value.data(), value.length(), parse);
        if (field->is_repeated()) {
          message_reflection->addstring(message, field, value);
        } else {
          message_reflection->setstring(message, field, value);
        }
        break;
      }

      case fielddescriptor::type_bytes: {
        string value;
        if (!wireformatlite::readbytes(input, &value)) return false;
        if (field->is_repeated()) {
          message_reflection->addstring(message, field, value);
        } else {
          message_reflection->setstring(message, field, value);
        }
        break;
      }

      case fielddescriptor::type_group: {
        message* sub_message;
        if (field->is_repeated()) {
          sub_message = message_reflection->addmessage(
              message, field, input->getextensionfactory());
        } else {
          sub_message = message_reflection->mutablemessage(
              message, field, input->getextensionfactory());
        }

        if (!wireformatlite::readgroup(wireformatlite::gettagfieldnumber(tag),
                                       input, sub_message))
          return false;
        break;
      }

      case fielddescriptor::type_message: {
        message* sub_message;
        if (field->is_repeated()) {
          sub_message = message_reflection->addmessage(
              message, field, input->getextensionfactory());
        } else {
          sub_message = message_reflection->mutablemessage(
              message, field, input->getextensionfactory());
        }

        if (!wireformatlite::readmessage(input, sub_message)) return false;
        break;
      }
    }
  }

  return true;
}

bool wireformat::parseandmergemessagesetitem(
    io::codedinputstream* input,
    message* message) {
  const reflection* message_reflection = message->getreflection();

  // this method parses a group which should contain two fields:
  //   required int32 type_id = 2;
  //   required data message = 3;

  // once we see a type_id, we'll construct a fake tag for this extension
  // which is the tag it would have had under the proto2 extensions wire
  // format.
  uint32 fake_tag = 0;

  // once we see a type_id, we'll look up the fielddescriptor for the
  // extension.
  const fielddescriptor* field = null;

  // if we see message data before the type_id, we'll append it to this so
  // we can parse it later.
  string message_data;

  while (true) {
    uint32 tag = input->readtag();
    if (tag == 0) return false;

    switch (tag) {
      case wireformatlite::kmessagesettypeidtag: {
        uint32 type_id;
        if (!input->readvarint32(&type_id)) return false;
        fake_tag = wireformatlite::maketag(
            type_id, wireformatlite::wiretype_length_delimited);
        field = message_reflection->findknownextensionbynumber(type_id);

        if (!message_data.empty()) {
          // we saw some message data before the type_id.  have to parse it
          // now.
          io::arrayinputstream raw_input(message_data.data(),
                                         message_data.size());
          io::codedinputstream sub_input(&raw_input);
          if (!parseandmergefield(fake_tag, field, message,
                                  &sub_input)) {
            return false;
          }
          message_data.clear();
        }

        break;
      }

      case wireformatlite::kmessagesetmessagetag: {
        if (fake_tag == 0) {
          // we haven't seen a type_id yet.  append this data to message_data.
          string temp;
          uint32 length;
          if (!input->readvarint32(&length)) return false;
          if (!input->readstring(&temp, length)) return false;
          io::stringoutputstream output_stream(&message_data);
          io::codedoutputstream coded_output(&output_stream);
          coded_output.writevarint32(length);
          coded_output.writestring(temp);
        } else {
          // already saw type_id, so we can parse this directly.
          if (!parseandmergefield(fake_tag, field, message, input)) {
            return false;
          }
        }

        break;
      }

      case wireformatlite::kmessagesetitemendtag: {
        return true;
      }

      default: {
        if (!skipfield(input, tag, null)) return false;
      }
    }
  }
}

// ===================================================================

void wireformat::serializewithcachedsizes(
    const message& message,
    int size, io::codedoutputstream* output) {
  const descriptor* descriptor = message.getdescriptor();
  const reflection* message_reflection = message.getreflection();
  int expected_endpoint = output->bytecount() + size;

  vector<const fielddescriptor*> fields;
  message_reflection->listfields(message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    serializefieldwithcachedsizes(fields[i], message, output);
  }

  if (descriptor->options().message_set_wire_format()) {
    serializeunknownmessagesetitems(
        message_reflection->getunknownfields(message), output);
  } else {
    serializeunknownfields(
        message_reflection->getunknownfields(message), output);
  }

  google_check_eq(output->bytecount(), expected_endpoint)
    << ": protocol message serialized to a size different from what was "
       "originally expected.  perhaps it was modified by another thread "
       "during serialization?";
}

void wireformat::serializefieldwithcachedsizes(
    const fielddescriptor* field,
    const message& message,
    io::codedoutputstream* output) {
  const reflection* message_reflection = message.getreflection();

  if (field->is_extension() &&
      field->containing_type()->options().message_set_wire_format() &&
      field->cpp_type() == fielddescriptor::cpptype_message &&
      !field->is_repeated()) {
    serializemessagesetitemwithcachedsizes(field, message, output);
    return;
  }

  int count = 0;

  if (field->is_repeated()) {
    count = message_reflection->fieldsize(message, field);
  } else if (message_reflection->hasfield(message, field)) {
    count = 1;
  }

  const bool is_packed = field->options().packed();
  if (is_packed && count > 0) {
    wireformatlite::writetag(field->number(),
        wireformatlite::wiretype_length_delimited, output);
    const int data_size = fielddataonlybytesize(field, message);
    output->writevarint32(data_size);
  }

  for (int j = 0; j < count; j++) {
    switch (field->type()) {
#define handle_primitive_type(type, cpptype, type_method, cpptype_method)      \
      case fielddescriptor::type_##type: {                                     \
        const cpptype value = field->is_repeated() ?                           \
                              message_reflection->getrepeated##cpptype_method( \
                                message, field, j) :                           \
                              message_reflection->get##cpptype_method(         \
                                message, field);                               \
        if (is_packed) {                                                       \
          wireformatlite::write##type_method##notag(value, output);            \
        } else {                                                               \
          wireformatlite::write##type_method(field->number(), value, output);  \
        }                                                                      \
        break;                                                                 \
      }

      handle_primitive_type( int32,  int32,  int32,  int32)
      handle_primitive_type( int64,  int64,  int64,  int64)
      handle_primitive_type(sint32,  int32, sint32,  int32)
      handle_primitive_type(sint64,  int64, sint64,  int64)
      handle_primitive_type(uint32, uint32, uint32, uint32)
      handle_primitive_type(uint64, uint64, uint64, uint64)

      handle_primitive_type( fixed32, uint32,  fixed32, uint32)
      handle_primitive_type( fixed64, uint64,  fixed64, uint64)
      handle_primitive_type(sfixed32,  int32, sfixed32,  int32)
      handle_primitive_type(sfixed64,  int64, sfixed64,  int64)

      handle_primitive_type(float , float , float , float )
      handle_primitive_type(double, double, double, double)

      handle_primitive_type(bool, bool, bool, bool)
#undef handle_primitive_type

#define handle_type(type, type_method, cpptype_method)                       \
      case fielddescriptor::type_##type:                                     \
        wireformatlite::write##type_method(                                  \
              field->number(),                                               \
              field->is_repeated() ?                                         \
                message_reflection->getrepeated##cpptype_method(             \
                  message, field, j) :                                       \
                message_reflection->get##cpptype_method(message, field),     \
              output);                                                       \
        break;

      handle_type(group  , group  , message)
      handle_type(message, message, message)
#undef handle_type

      case fielddescriptor::type_enum: {
        const enumvaluedescriptor* value = field->is_repeated() ?
          message_reflection->getrepeatedenum(message, field, j) :
          message_reflection->getenum(message, field);
        if (is_packed) {
          wireformatlite::writeenumnotag(value->number(), output);
        } else {
          wireformatlite::writeenum(field->number(), value->number(), output);
        }
        break;
      }

      // handle strings separately so that we can get string references
      // instead of copying.
      case fielddescriptor::type_string: {
        string scratch;
        const string& value = field->is_repeated() ?
          message_reflection->getrepeatedstringreference(
            message, field, j, &scratch) :
          message_reflection->getstringreference(message, field, &scratch);
        verifyutf8string(value.data(), value.length(), serialize);
        wireformatlite::writestring(field->number(), value, output);
        break;
      }

      case fielddescriptor::type_bytes: {
        string scratch;
        const string& value = field->is_repeated() ?
          message_reflection->getrepeatedstringreference(
            message, field, j, &scratch) :
          message_reflection->getstringreference(message, field, &scratch);
        wireformatlite::writebytes(field->number(), value, output);
        break;
      }
    }
  }
}

void wireformat::serializemessagesetitemwithcachedsizes(
    const fielddescriptor* field,
    const message& message,
    io::codedoutputstream* output) {
  const reflection* message_reflection = message.getreflection();

  // start group.
  output->writevarint32(wireformatlite::kmessagesetitemstarttag);

  // write type id.
  output->writevarint32(wireformatlite::kmessagesettypeidtag);
  output->writevarint32(field->number());

  // write message.
  output->writevarint32(wireformatlite::kmessagesetmessagetag);

  const message& sub_message = message_reflection->getmessage(message, field);
  output->writevarint32(sub_message.getcachedsize());
  sub_message.serializewithcachedsizes(output);

  // end group.
  output->writevarint32(wireformatlite::kmessagesetitemendtag);
}

// ===================================================================

int wireformat::bytesize(const message& message) {
  const descriptor* descriptor = message.getdescriptor();
  const reflection* message_reflection = message.getreflection();

  int our_size = 0;

  vector<const fielddescriptor*> fields;
  message_reflection->listfields(message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    our_size += fieldbytesize(fields[i], message);
  }

  if (descriptor->options().message_set_wire_format()) {
    our_size += computeunknownmessagesetitemssize(
      message_reflection->getunknownfields(message));
  } else {
    our_size += computeunknownfieldssize(
      message_reflection->getunknownfields(message));
  }

  return our_size;
}

int wireformat::fieldbytesize(
    const fielddescriptor* field,
    const message& message) {
  const reflection* message_reflection = message.getreflection();

  if (field->is_extension() &&
      field->containing_type()->options().message_set_wire_format() &&
      field->cpp_type() == fielddescriptor::cpptype_message &&
      !field->is_repeated()) {
    return messagesetitembytesize(field, message);
  }

  int count = 0;
  if (field->is_repeated()) {
    count = message_reflection->fieldsize(message, field);
  } else if (message_reflection->hasfield(message, field)) {
    count = 1;
  }

  const int data_size = fielddataonlybytesize(field, message);
  int our_size = data_size;
  if (field->options().packed()) {
    if (data_size > 0) {
      // packed fields get serialized like a string, not their native type.
      // technically this doesn't really matter; the size only changes if it's
      // a group
      our_size += tagsize(field->number(), fielddescriptor::type_string);
      our_size += io::codedoutputstream::varintsize32(data_size);
    }
  } else {
    our_size += count * tagsize(field->number(), field->type());
  }
  return our_size;
}

int wireformat::fielddataonlybytesize(
    const fielddescriptor* field,
    const message& message) {
  const reflection* message_reflection = message.getreflection();

  int count = 0;
  if (field->is_repeated()) {
    count = message_reflection->fieldsize(message, field);
  } else if (message_reflection->hasfield(message, field)) {
    count = 1;
  }

  int data_size = 0;
  switch (field->type()) {
#define handle_type(type, type_method, cpptype_method)                     \
    case fielddescriptor::type_##type:                                     \
      if (field->is_repeated()) {                                          \
        for (int j = 0; j < count; j++) {                                  \
          data_size += wireformatlite::type_method##size(                  \
            message_reflection->getrepeated##cpptype_method(               \
              message, field, j));                                         \
        }                                                                  \
      } else {                                                             \
        data_size += wireformatlite::type_method##size(                    \
          message_reflection->get##cpptype_method(message, field));        \
      }                                                                    \
      break;

#define handle_fixed_type(type, type_method)                               \
    case fielddescriptor::type_##type:                                     \
      data_size += count * wireformatlite::k##type_method##size;           \
      break;

    handle_type( int32,  int32,  int32)
    handle_type( int64,  int64,  int64)
    handle_type(sint32, sint32,  int32)
    handle_type(sint64, sint64,  int64)
    handle_type(uint32, uint32, uint32)
    handle_type(uint64, uint64, uint64)

    handle_fixed_type( fixed32,  fixed32)
    handle_fixed_type( fixed64,  fixed64)
    handle_fixed_type(sfixed32, sfixed32)
    handle_fixed_type(sfixed64, sfixed64)

    handle_fixed_type(float , float )
    handle_fixed_type(double, double)

    handle_fixed_type(bool, bool)

    handle_type(group  , group  , message)
    handle_type(message, message, message)
#undef handle_type
#undef handle_fixed_type

    case fielddescriptor::type_enum: {
      if (field->is_repeated()) {
        for (int j = 0; j < count; j++) {
          data_size += wireformatlite::enumsize(
            message_reflection->getrepeatedenum(message, field, j)->number());
        }
      } else {
        data_size += wireformatlite::enumsize(
          message_reflection->getenum(message, field)->number());
      }
      break;
    }

    // handle strings separately so that we can get string references
    // instead of copying.
    case fielddescriptor::type_string:
    case fielddescriptor::type_bytes: {
      for (int j = 0; j < count; j++) {
        string scratch;
        const string& value = field->is_repeated() ?
          message_reflection->getrepeatedstringreference(
            message, field, j, &scratch) :
          message_reflection->getstringreference(message, field, &scratch);
        data_size += wireformatlite::stringsize(value);
      }
      break;
    }
  }
  return data_size;
}

int wireformat::messagesetitembytesize(
    const fielddescriptor* field,
    const message& message) {
  const reflection* message_reflection = message.getreflection();

  int our_size = wireformatlite::kmessagesetitemtagssize;

  // type_id
  our_size += io::codedoutputstream::varintsize32(field->number());

  // message
  const message& sub_message = message_reflection->getmessage(message, field);
  int message_size = sub_message.bytesize();

  our_size += io::codedoutputstream::varintsize32(message_size);
  our_size += message_size;

  return our_size;
}

void wireformat::verifyutf8stringfallback(const char* data,
                                          int size,
                                          operation op) {
  if (!isstructurallyvalidutf8(data, size)) {
    const char* operation_str = null;
    switch (op) {
      case parse:
        operation_str = "parsing";
        break;
      case serialize:
        operation_str = "serializing";
        break;
      // no default case: have the compiler warn if a case is not covered.
    }
    google_log(error) << "string field contains invalid utf-8 data when "
               << operation_str
               << " a protocol buffer. use the 'bytes' type if you intend to "
                  "send raw bytes.";
  }
}


}  // namespace internal
}  // namespace protobuf
}  // namespace google
