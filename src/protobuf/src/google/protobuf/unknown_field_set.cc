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

#include <google/protobuf/unknown_field_set.h>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {

unknownfieldset::unknownfieldset()
  : fields_(null) {}

unknownfieldset::~unknownfieldset() {
  clear();
  delete fields_;
}

void unknownfieldset::clearfallback() {
  google_dcheck(fields_ != null);
  for (int i = 0; i < fields_->size(); i++) {
    (*fields_)[i].delete();
  }
  fields_->clear();
}

void unknownfieldset::clearandfreememory() {
  if (fields_ != null) {
    clear();
    delete fields_;
    fields_ = null;
  }
}

void unknownfieldset::mergefrom(const unknownfieldset& other) {
  for (int i = 0; i < other.field_count(); i++) {
    addfield(other.field(i));
  }
}

int unknownfieldset::spaceusedexcludingself() const {
  if (fields_ == null) return 0;

  int total_size = sizeof(*fields_) + sizeof(unknownfield) * fields_->size();
  for (int i = 0; i < fields_->size(); i++) {
    const unknownfield& field = (*fields_)[i];
    switch (field.type()) {
      case unknownfield::type_length_delimited:
        total_size += sizeof(*field.length_delimited_.string_value_) +
                      internal::stringspaceusedexcludingself(
                          *field.length_delimited_.string_value_);
        break;
      case unknownfield::type_group:
        total_size += field.group_->spaceused();
        break;
      default:
        break;
    }
  }
  return total_size;
}

int unknownfieldset::spaceused() const {
  return sizeof(*this) + spaceusedexcludingself();
}

void unknownfieldset::addvarint(int number, uint64 value) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  unknownfield field;
  field.number_ = number;
  field.type_ = unknownfield::type_varint;
  field.varint_ = value;
  fields_->push_back(field);
}

void unknownfieldset::addfixed32(int number, uint32 value) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  unknownfield field;
  field.number_ = number;
  field.type_ = unknownfield::type_fixed32;
  field.fixed32_ = value;
  fields_->push_back(field);
}

void unknownfieldset::addfixed64(int number, uint64 value) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  unknownfield field;
  field.number_ = number;
  field.type_ = unknownfield::type_fixed64;
  field.fixed64_ = value;
  fields_->push_back(field);
}

string* unknownfieldset::addlengthdelimited(int number) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  unknownfield field;
  field.number_ = number;
  field.type_ = unknownfield::type_length_delimited;
  field.length_delimited_.string_value_ = new string;
  fields_->push_back(field);
  return field.length_delimited_.string_value_;
}


unknownfieldset* unknownfieldset::addgroup(int number) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  unknownfield field;
  field.number_ = number;
  field.type_ = unknownfield::type_group;
  field.group_ = new unknownfieldset;
  fields_->push_back(field);
  return field.group_;
}

void unknownfieldset::addfield(const unknownfield& field) {
  if (fields_ == null) fields_ = new vector<unknownfield>;
  fields_->push_back(field);
  fields_->back().deepcopy();
}

void unknownfieldset::deletesubrange(int start, int num) {
  google_dcheck(fields_ != null);
  // delete the specified fields.
  for (int i = 0; i < num; ++i) {
    (*fields_)[i + start].delete();
  }
  // slide down the remaining fields.
  for (int i = start + num; i < fields_->size(); ++i) {
    (*fields_)[i - num] = (*fields_)[i];
  }
  // pop off the # of deleted fields.
  for (int i = 0; i < num; ++i) {
    fields_->pop_back();
  }
}

void unknownfieldset::deletebynumber(int number) {
  if (fields_ == null) return;
  int left = 0;  // the number of fields left after deletion.
  for (int i = 0; i < fields_->size(); ++i) {
    unknownfield* field = &(*fields_)[i];
    if (field->number() == number) {
      field->delete();
    } else {
      if (i != left) {
        (*fields_)[left] = (*fields_)[i];
      }
      ++left;
    }
  }
  fields_->resize(left);
}

bool unknownfieldset::mergefromcodedstream(io::codedinputstream* input) {

  unknownfieldset other;
  if (internal::wireformat::skipmessage(input, &other) &&
                                  input->consumedentiremessage()) {
    mergefrom(other);
    return true;
  } else {
    return false;
  }
}

bool unknownfieldset::parsefromcodedstream(io::codedinputstream* input) {
  clear();
  return mergefromcodedstream(input);
}

bool unknownfieldset::parsefromzerocopystream(io::zerocopyinputstream* input) {
  io::codedinputstream coded_input(input);
  return parsefromcodedstream(&coded_input) &&
    coded_input.consumedentiremessage();
}

bool unknownfieldset::parsefromarray(const void* data, int size) {
  io::arrayinputstream input(data, size);
  return parsefromzerocopystream(&input);
}

void unknownfield::delete() {
  switch (type()) {
    case unknownfield::type_length_delimited:
      delete length_delimited_.string_value_;
      break;
    case unknownfield::type_group:
      delete group_;
      break;
    default:
      break;
  }
}

void unknownfield::deepcopy() {
  switch (type()) {
    case unknownfield::type_length_delimited:
      length_delimited_.string_value_ = new string(
          *length_delimited_.string_value_);
      break;
    case unknownfield::type_group: {
      unknownfieldset* group = new unknownfieldset;
      group->mergefrom(*group_);
      group_ = group;
      break;
    }
    default:
      break;
  }
}


void unknownfield::serializelengthdelimitednotag(
    io::codedoutputstream* output) const {
  google_dcheck_eq(type_length_delimited, type_);
  const string& data = *length_delimited_.string_value_;
  output->writevarint32(data.size());
  output->writestring(data);
}

uint8* unknownfield::serializelengthdelimitednotagtoarray(uint8* target) const {
  google_dcheck_eq(type_length_delimited, type_);
  const string& data = *length_delimited_.string_value_;
  target = io::codedoutputstream::writevarint32toarray(data.size(), target);
  target = io::codedoutputstream::writestringtoarray(data, target);
  return target;
}

}  // namespace protobuf
}  // namespace google
