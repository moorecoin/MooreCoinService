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
//
// recursive descent ftw.

#include <float.h>
#include <google/protobuf/stubs/hash.h>
#include <limits>


#include <google/protobuf/compiler/parser.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/wire_format.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/map-util.h>

namespace google {
namespace protobuf {
namespace compiler {

using internal::wireformat;

namespace {

typedef hash_map<string, fielddescriptorproto::type> typenamemap;

typenamemap maketypenametable() {
  typenamemap result;

  result["double"  ] = fielddescriptorproto::type_double;
  result["float"   ] = fielddescriptorproto::type_float;
  result["uint64"  ] = fielddescriptorproto::type_uint64;
  result["fixed64" ] = fielddescriptorproto::type_fixed64;
  result["fixed32" ] = fielddescriptorproto::type_fixed32;
  result["bool"    ] = fielddescriptorproto::type_bool;
  result["string"  ] = fielddescriptorproto::type_string;
  result["group"   ] = fielddescriptorproto::type_group;

  result["bytes"   ] = fielddescriptorproto::type_bytes;
  result["uint32"  ] = fielddescriptorproto::type_uint32;
  result["sfixed32"] = fielddescriptorproto::type_sfixed32;
  result["sfixed64"] = fielddescriptorproto::type_sfixed64;
  result["int32"   ] = fielddescriptorproto::type_int32;
  result["int64"   ] = fielddescriptorproto::type_int64;
  result["sint32"  ] = fielddescriptorproto::type_sint32;
  result["sint64"  ] = fielddescriptorproto::type_sint64;

  return result;
}

const typenamemap ktypenames = maketypenametable();

}  // anonymous namespace

// makes code slightly more readable.  the meaning of "do(foo)" is
// "execute foo and fail if it fails.", where failure is indicated by
// returning false.
#define do(statement) if (statement) {} else return false

// ===================================================================

parser::parser()
  : input_(null),
    error_collector_(null),
    source_location_table_(null),
    had_errors_(false),
    require_syntax_identifier_(false),
    stop_after_syntax_identifier_(false) {
}

parser::~parser() {
}

// ===================================================================

inline bool parser::lookingat(const char* text) {
  return input_->current().text == text;
}

inline bool parser::lookingattype(io::tokenizer::tokentype token_type) {
  return input_->current().type == token_type;
}

inline bool parser::atend() {
  return lookingattype(io::tokenizer::type_end);
}

bool parser::tryconsume(const char* text) {
  if (lookingat(text)) {
    input_->next();
    return true;
  } else {
    return false;
  }
}

bool parser::consume(const char* text, const char* error) {
  if (tryconsume(text)) {
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::consume(const char* text) {
  if (tryconsume(text)) {
    return true;
  } else {
    adderror("expected \"" + string(text) + "\".");
    return false;
  }
}

bool parser::consumeidentifier(string* output, const char* error) {
  if (lookingattype(io::tokenizer::type_identifier)) {
    *output = input_->current().text;
    input_->next();
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::consumeinteger(int* output, const char* error) {
  if (lookingattype(io::tokenizer::type_integer)) {
    uint64 value = 0;
    if (!io::tokenizer::parseinteger(input_->current().text,
                                     kint32max, &value)) {
      adderror("integer out of range.");
      // we still return true because we did, in fact, parse an integer.
    }
    *output = value;
    input_->next();
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::consumesignedinteger(int* output, const char* error) {
  bool is_negative = false;
  uint64 max_value = kint32max;
  if (tryconsume("-")) {
    is_negative = true;
    max_value += 1;
  }
  uint64 value = 0;
  do(consumeinteger64(max_value, &value, error));
  if (is_negative) value *= -1;
  *output = value;
  return true;
}

bool parser::consumeinteger64(uint64 max_value, uint64* output,
                              const char* error) {
  if (lookingattype(io::tokenizer::type_integer)) {
    if (!io::tokenizer::parseinteger(input_->current().text, max_value,
                                     output)) {
      adderror("integer out of range.");
      // we still return true because we did, in fact, parse an integer.
      *output = 0;
    }
    input_->next();
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::consumenumber(double* output, const char* error) {
  if (lookingattype(io::tokenizer::type_float)) {
    *output = io::tokenizer::parsefloat(input_->current().text);
    input_->next();
    return true;
  } else if (lookingattype(io::tokenizer::type_integer)) {
    // also accept integers.
    uint64 value = 0;
    if (!io::tokenizer::parseinteger(input_->current().text,
                                     kuint64max, &value)) {
      adderror("integer out of range.");
      // we still return true because we did, in fact, parse a number.
    }
    *output = value;
    input_->next();
    return true;
  } else if (lookingat("inf")) {
    *output = numeric_limits<double>::infinity();
    input_->next();
    return true;
  } else if (lookingat("nan")) {
    *output = numeric_limits<double>::quiet_nan();
    input_->next();
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::consumestring(string* output, const char* error) {
  if (lookingattype(io::tokenizer::type_string)) {
    io::tokenizer::parsestring(input_->current().text, output);
    input_->next();
    // allow c++ like concatenation of adjacent string tokens.
    while (lookingattype(io::tokenizer::type_string)) {
      io::tokenizer::parsestringappend(input_->current().text, output);
      input_->next();
    }
    return true;
  } else {
    adderror(error);
    return false;
  }
}

bool parser::tryconsumeendofdeclaration(const char* text,
                                        const locationrecorder* location) {
  if (lookingat(text)) {
    string leading, trailing;
    input_->nextwithcomments(&trailing, null, &leading);

    // save the leading comments for next time, and recall the leading comments
    // from last time.
    leading.swap(upcoming_doc_comments_);

    if (location != null) {
      location->attachcomments(&leading, &trailing);
    }
    return true;
  } else {
    return false;
  }
}

bool parser::consumeendofdeclaration(const char* text,
                                     const locationrecorder* location) {
  if (tryconsumeendofdeclaration(text, location)) {
    return true;
  } else {
    adderror("expected \"" + string(text) + "\".");
    return false;
  }
}

// -------------------------------------------------------------------

void parser::adderror(int line, int column, const string& error) {
  if (error_collector_ != null) {
    error_collector_->adderror(line, column, error);
  }
  had_errors_ = true;
}

void parser::adderror(const string& error) {
  adderror(input_->current().line, input_->current().column, error);
}

// -------------------------------------------------------------------

parser::locationrecorder::locationrecorder(parser* parser)
  : parser_(parser),
    location_(parser_->source_code_info_->add_location()) {
  location_->add_span(parser_->input_->current().line);
  location_->add_span(parser_->input_->current().column);
}

parser::locationrecorder::locationrecorder(const locationrecorder& parent) {
  init(parent);
}

parser::locationrecorder::locationrecorder(const locationrecorder& parent,
                                           int path1) {
  init(parent);
  addpath(path1);
}

parser::locationrecorder::locationrecorder(const locationrecorder& parent,
                                           int path1, int path2) {
  init(parent);
  addpath(path1);
  addpath(path2);
}

void parser::locationrecorder::init(const locationrecorder& parent) {
  parser_ = parent.parser_;
  location_ = parser_->source_code_info_->add_location();
  location_->mutable_path()->copyfrom(parent.location_->path());

  location_->add_span(parser_->input_->current().line);
  location_->add_span(parser_->input_->current().column);
}

parser::locationrecorder::~locationrecorder() {
  if (location_->span_size() <= 2) {
    endat(parser_->input_->previous());
  }
}

void parser::locationrecorder::addpath(int path_component) {
  location_->add_path(path_component);
}

void parser::locationrecorder::startat(const io::tokenizer::token& token) {
  location_->set_span(0, token.line);
  location_->set_span(1, token.column);
}

void parser::locationrecorder::endat(const io::tokenizer::token& token) {
  if (token.line != location_->span(0)) {
    location_->add_span(token.line);
  }
  location_->add_span(token.end_column);
}

void parser::locationrecorder::recordlegacylocation(const message* descriptor,
    descriptorpool::errorcollector::errorlocation location) {
  if (parser_->source_location_table_ != null) {
    parser_->source_location_table_->add(
        descriptor, location, location_->span(0), location_->span(1));
  }
}

void parser::locationrecorder::attachcomments(
    string* leading, string* trailing) const {
  google_check(!location_->has_leading_comments());
  google_check(!location_->has_trailing_comments());

  if (!leading->empty()) {
    location_->mutable_leading_comments()->swap(*leading);
  }
  if (!trailing->empty()) {
    location_->mutable_trailing_comments()->swap(*trailing);
  }
}

// -------------------------------------------------------------------

void parser::skipstatement() {
  while (true) {
    if (atend()) {
      return;
    } else if (lookingattype(io::tokenizer::type_symbol)) {
      if (tryconsumeendofdeclaration(";", null)) {
        return;
      } else if (tryconsume("{")) {
        skiprestofblock();
        return;
      } else if (lookingat("}")) {
        return;
      }
    }
    input_->next();
  }
}

void parser::skiprestofblock() {
  while (true) {
    if (atend()) {
      return;
    } else if (lookingattype(io::tokenizer::type_symbol)) {
      if (tryconsumeendofdeclaration("}", null)) {
        return;
      } else if (tryconsume("{")) {
        skiprestofblock();
      }
    }
    input_->next();
  }
}

// ===================================================================

bool parser::parse(io::tokenizer* input, filedescriptorproto* file) {
  input_ = input;
  had_errors_ = false;
  syntax_identifier_.clear();

  // note that |file| could be null at this point if
  // stop_after_syntax_identifier_ is true.  so, we conservatively allocate
  // sourcecodeinfo on the stack, then swap it into the filedescriptorproto
  // later on.
  sourcecodeinfo source_code_info;
  source_code_info_ = &source_code_info;

  if (lookingattype(io::tokenizer::type_start)) {
    // advance to first token.
    input_->nextwithcomments(null, null, &upcoming_doc_comments_);
  }

  {
    locationrecorder root_location(this);

    if (require_syntax_identifier_ || lookingat("syntax")) {
      if (!parsesyntaxidentifier()) {
        // don't attempt to parse the file if we didn't recognize the syntax
        // identifier.
        return false;
      }
    } else if (!stop_after_syntax_identifier_) {
      syntax_identifier_ = "proto2";
    }

    if (stop_after_syntax_identifier_) return !had_errors_;

    // repeatedly parse statements until we reach the end of the file.
    while (!atend()) {
      if (!parsetoplevelstatement(file, root_location)) {
        // this statement failed to parse.  skip it, but keep looping to parse
        // other statements.
        skipstatement();

        if (lookingat("}")) {
          adderror("unmatched \"}\".");
          input_->nextwithcomments(null, null, &upcoming_doc_comments_);
        }
      }
    }
  }

  input_ = null;
  source_code_info_ = null;
  source_code_info.swap(file->mutable_source_code_info());
  return !had_errors_;
}

bool parser::parsesyntaxidentifier() {
  do(consume("syntax", "file must begin with 'syntax = \"proto2\";'."));
  do(consume("="));
  io::tokenizer::token syntax_token = input_->current();
  string syntax;
  do(consumestring(&syntax, "expected syntax identifier."));
  do(consumeendofdeclaration(";", null));

  syntax_identifier_ = syntax;

  if (syntax != "proto2" && !stop_after_syntax_identifier_) {
    adderror(syntax_token.line, syntax_token.column,
      "unrecognized syntax identifier \"" + syntax + "\".  this parser "
      "only recognizes \"proto2\".");
    return false;
  }

  return true;
}

bool parser::parsetoplevelstatement(filedescriptorproto* file,
                                    const locationrecorder& root_location) {
  if (tryconsumeendofdeclaration(";", null)) {
    // empty statement; ignore
    return true;
  } else if (lookingat("message")) {
    locationrecorder location(root_location,
      filedescriptorproto::kmessagetypefieldnumber, file->message_type_size());
    return parsemessagedefinition(file->add_message_type(), location);
  } else if (lookingat("enum")) {
    locationrecorder location(root_location,
      filedescriptorproto::kenumtypefieldnumber, file->enum_type_size());
    return parseenumdefinition(file->add_enum_type(), location);
  } else if (lookingat("service")) {
    locationrecorder location(root_location,
      filedescriptorproto::kservicefieldnumber, file->service_size());
    return parseservicedefinition(file->add_service(), location);
  } else if (lookingat("extend")) {
    locationrecorder location(root_location,
        filedescriptorproto::kextensionfieldnumber);
    return parseextend(file->mutable_extension(),
                       file->mutable_message_type(),
                       root_location,
                       filedescriptorproto::kmessagetypefieldnumber,
                       location);
  } else if (lookingat("import")) {
    return parseimport(file->mutable_dependency(),
                       file->mutable_public_dependency(),
                       file->mutable_weak_dependency(),
                       root_location);
  } else if (lookingat("package")) {
    return parsepackage(file, root_location);
  } else if (lookingat("option")) {
    locationrecorder location(root_location,
        filedescriptorproto::koptionsfieldnumber);
    return parseoption(file->mutable_options(), location, option_statement);
  } else {
    adderror("expected top-level statement (e.g. \"message\").");
    return false;
  }
}

// -------------------------------------------------------------------
// messages

bool parser::parsemessagedefinition(descriptorproto* message,
                                    const locationrecorder& message_location) {
  do(consume("message"));
  {
    locationrecorder location(message_location,
                              descriptorproto::knamefieldnumber);
    location.recordlegacylocation(
        message, descriptorpool::errorcollector::name);
    do(consumeidentifier(message->mutable_name(), "expected message name."));
  }
  do(parsemessageblock(message, message_location));
  return true;
}

namespace {

const int kmaxextensionrangesentinel = -1;

bool ismessagesetwireformatmessage(const descriptorproto& message) {
  const messageoptions& options = message.options();
  for (int i = 0; i < options.uninterpreted_option_size(); ++i) {
    const uninterpretedoption& uninterpreted = options.uninterpreted_option(i);
    if (uninterpreted.name_size() == 1 &&
        uninterpreted.name(0).name_part() == "message_set_wire_format" &&
        uninterpreted.identifier_value() == "true") {
      return true;
    }
  }
  return false;
}

// modifies any extension ranges that specified 'max' as the end of the
// extension range, and sets them to the type-specific maximum. the actual max
// tag number can only be determined after all options have been parsed.
void adjustextensionrangeswithmaxendnumber(descriptorproto* message) {
  const bool is_message_set = ismessagesetwireformatmessage(*message);
  const int max_extension_number = is_message_set ?
      kint32max :
      fielddescriptor::kmaxnumber + 1;
  for (int i = 0; i < message->extension_range_size(); ++i) {
    if (message->extension_range(i).end() == kmaxextensionrangesentinel) {
      message->mutable_extension_range(i)->set_end(max_extension_number);
    }
  }
}

}  // namespace

bool parser::parsemessageblock(descriptorproto* message,
                               const locationrecorder& message_location) {
  do(consumeendofdeclaration("{", &message_location));

  while (!tryconsumeendofdeclaration("}", null)) {
    if (atend()) {
      adderror("reached end of input in message definition (missing '}').");
      return false;
    }

    if (!parsemessagestatement(message, message_location)) {
      // this statement failed to parse.  skip it, but keep looping to parse
      // other statements.
      skipstatement();
    }
  }

  if (message->extension_range_size() > 0) {
    adjustextensionrangeswithmaxendnumber(message);
  }
  return true;
}

bool parser::parsemessagestatement(descriptorproto* message,
                                   const locationrecorder& message_location) {
  if (tryconsumeendofdeclaration(";", null)) {
    // empty statement; ignore
    return true;
  } else if (lookingat("message")) {
    locationrecorder location(message_location,
                              descriptorproto::knestedtypefieldnumber,
                              message->nested_type_size());
    return parsemessagedefinition(message->add_nested_type(), location);
  } else if (lookingat("enum")) {
    locationrecorder location(message_location,
                              descriptorproto::kenumtypefieldnumber,
                              message->enum_type_size());
    return parseenumdefinition(message->add_enum_type(), location);
  } else if (lookingat("extensions")) {
    locationrecorder location(message_location,
                              descriptorproto::kextensionrangefieldnumber);
    return parseextensions(message, location);
  } else if (lookingat("extend")) {
    locationrecorder location(message_location,
                              descriptorproto::kextensionfieldnumber);
    return parseextend(message->mutable_extension(),
                       message->mutable_nested_type(),
                       message_location,
                       descriptorproto::knestedtypefieldnumber,
                       location);
  } else if (lookingat("option")) {
    locationrecorder location(message_location,
                              descriptorproto::koptionsfieldnumber);
    return parseoption(message->mutable_options(), location, option_statement);
  } else {
    locationrecorder location(message_location,
                              descriptorproto::kfieldfieldnumber,
                              message->field_size());
    return parsemessagefield(message->add_field(),
                             message->mutable_nested_type(),
                             message_location,
                             descriptorproto::knestedtypefieldnumber,
                             location);
  }
}

bool parser::parsemessagefield(fielddescriptorproto* field,
                               repeatedptrfield<descriptorproto>* messages,
                               const locationrecorder& parent_location,
                               int location_field_number_for_nested_type,
                               const locationrecorder& field_location) {
  // parse label and type.
  io::tokenizer::token label_token = input_->current();
  {
    locationrecorder location(field_location,
                              fielddescriptorproto::klabelfieldnumber);
    fielddescriptorproto::label label;
    do(parselabel(&label));
    field->set_label(label);
  }

  {
    locationrecorder location(field_location);  // add path later
    location.recordlegacylocation(field, descriptorpool::errorcollector::type);

    fielddescriptorproto::type type = fielddescriptorproto::type_int32;
    string type_name;
    do(parsetype(&type, &type_name));
    if (type_name.empty()) {
      location.addpath(fielddescriptorproto::ktypefieldnumber);
      field->set_type(type);
    } else {
      location.addpath(fielddescriptorproto::ktypenamefieldnumber);
      field->set_type_name(type_name);
    }
  }

  // parse name and '='.
  io::tokenizer::token name_token = input_->current();
  {
    locationrecorder location(field_location,
                              fielddescriptorproto::knamefieldnumber);
    location.recordlegacylocation(field, descriptorpool::errorcollector::name);
    do(consumeidentifier(field->mutable_name(), "expected field name."));
  }
  do(consume("=", "missing field number."));

  // parse field number.
  {
    locationrecorder location(field_location,
                              fielddescriptorproto::knumberfieldnumber);
    location.recordlegacylocation(
        field, descriptorpool::errorcollector::number);
    int number;
    do(consumeinteger(&number, "expected field number."));
    field->set_number(number);
  }

  // parse options.
  do(parsefieldoptions(field, field_location));

  // deal with groups.
  if (field->has_type() && field->type() == fielddescriptorproto::type_group) {
    // awkward:  since a group declares both a message type and a field, we
    //   have to create overlapping locations.
    locationrecorder group_location(parent_location);
    group_location.startat(label_token);
    group_location.addpath(location_field_number_for_nested_type);
    group_location.addpath(messages->size());

    descriptorproto* group = messages->add();
    group->set_name(field->name());

    // record name location to match the field name's location.
    {
      locationrecorder location(group_location,
                                descriptorproto::knamefieldnumber);
      location.startat(name_token);
      location.endat(name_token);
      location.recordlegacylocation(
          group, descriptorpool::errorcollector::name);
    }

    // the field's type_name also comes from the name.  confusing!
    {
      locationrecorder location(field_location,
                                fielddescriptorproto::ktypenamefieldnumber);
      location.startat(name_token);
      location.endat(name_token);
    }

    // as a hack for backwards-compatibility, we force the group name to start
    // with a capital letter and lower-case the field name.  new code should
    // not use groups; it should use nested messages.
    if (group->name()[0] < 'a' || 'z' < group->name()[0]) {
      adderror(name_token.line, name_token.column,
        "group names must start with a capital letter.");
    }
    lowerstring(field->mutable_name());

    field->set_type_name(group->name());
    if (lookingat("{")) {
      do(parsemessageblock(group, group_location));
    } else {
      adderror("missing group body.");
      return false;
    }
  } else {
    do(consumeendofdeclaration(";", &field_location));
  }

  return true;
}

bool parser::parsefieldoptions(fielddescriptorproto* field,
                               const locationrecorder& field_location) {
  if (!lookingat("[")) return true;

  locationrecorder location(field_location,
                            fielddescriptorproto::koptionsfieldnumber);

  do(consume("["));

  // parse field options.
  do {
    if (lookingat("default")) {
      // we intentionally pass field_location rather than location here, since
      // the default value is not actually an option.
      do(parsedefaultassignment(field, field_location));
    } else {
      do(parseoption(field->mutable_options(), location, option_assignment));
    }
  } while (tryconsume(","));

  do(consume("]"));
  return true;
}

bool parser::parsedefaultassignment(fielddescriptorproto* field,
                                    const locationrecorder& field_location) {
  if (field->has_default_value()) {
    adderror("already set option \"default\".");
    field->clear_default_value();
  }

  do(consume("default"));
  do(consume("="));

  locationrecorder location(field_location,
                            fielddescriptorproto::kdefaultvaluefieldnumber);
  location.recordlegacylocation(
      field, descriptorpool::errorcollector::default_value);
  string* default_value = field->mutable_default_value();

  if (!field->has_type()) {
    // the field has a type name, but we don't know if it is a message or an
    // enum yet.  assume an enum for now.
    do(consumeidentifier(default_value, "expected identifier."));
    return true;
  }

  switch (field->type()) {
    case fielddescriptorproto::type_int32:
    case fielddescriptorproto::type_int64:
    case fielddescriptorproto::type_sint32:
    case fielddescriptorproto::type_sint64:
    case fielddescriptorproto::type_sfixed32:
    case fielddescriptorproto::type_sfixed64: {
      uint64 max_value = kint64max;
      if (field->type() == fielddescriptorproto::type_int32 ||
          field->type() == fielddescriptorproto::type_sint32 ||
          field->type() == fielddescriptorproto::type_sfixed32) {
        max_value = kint32max;
      }

      // these types can be negative.
      if (tryconsume("-")) {
        default_value->append("-");
        // two's complement always has one more negative value than positive.
        ++max_value;
      }
      // parse the integer to verify that it is not out-of-range.
      uint64 value;
      do(consumeinteger64(max_value, &value, "expected integer."));
      // and stringify it again.
      default_value->append(simpleitoa(value));
      break;
    }

    case fielddescriptorproto::type_uint32:
    case fielddescriptorproto::type_uint64:
    case fielddescriptorproto::type_fixed32:
    case fielddescriptorproto::type_fixed64: {
      uint64 max_value = kuint64max;
      if (field->type() == fielddescriptorproto::type_uint32 ||
          field->type() == fielddescriptorproto::type_fixed32) {
        max_value = kuint32max;
      }

      // numeric, not negative.
      if (tryconsume("-")) {
        adderror("unsigned field can't have negative default value.");
      }
      // parse the integer to verify that it is not out-of-range.
      uint64 value;
      do(consumeinteger64(max_value, &value, "expected integer."));
      // and stringify it again.
      default_value->append(simpleitoa(value));
      break;
    }

    case fielddescriptorproto::type_float:
    case fielddescriptorproto::type_double:
      // these types can be negative.
      if (tryconsume("-")) {
        default_value->append("-");
      }
      // parse the integer because we have to convert hex integers to decimal
      // floats.
      double value;
      do(consumenumber(&value, "expected number."));
      // and stringify it again.
      default_value->append(simpledtoa(value));
      break;

    case fielddescriptorproto::type_bool:
      if (tryconsume("true")) {
        default_value->assign("true");
      } else if (tryconsume("false")) {
        default_value->assign("false");
      } else {
        adderror("expected \"true\" or \"false\".");
        return false;
      }
      break;

    case fielddescriptorproto::type_string:
      do(consumestring(default_value, "expected string."));
      break;

    case fielddescriptorproto::type_bytes:
      do(consumestring(default_value, "expected string."));
      *default_value = cescape(*default_value);
      break;

    case fielddescriptorproto::type_enum:
      do(consumeidentifier(default_value, "expected identifier."));
      break;

    case fielddescriptorproto::type_message:
    case fielddescriptorproto::type_group:
      adderror("messages can't have default values.");
      return false;
  }

  return true;
}

bool parser::parseoptionnamepart(uninterpretedoption* uninterpreted_option,
                                 const locationrecorder& part_location) {
  uninterpretedoption::namepart* name = uninterpreted_option->add_name();
  string identifier;  // we parse identifiers into this string.
  if (lookingat("(")) {  // this is an extension.
    do(consume("("));

    {
      locationrecorder location(
          part_location, uninterpretedoption::namepart::knamepartfieldnumber);
      // an extension name consists of dot-separated identifiers, and may begin
      // with a dot.
      if (lookingattype(io::tokenizer::type_identifier)) {
        do(consumeidentifier(&identifier, "expected identifier."));
        name->mutable_name_part()->append(identifier);
      }
      while (lookingat(".")) {
        do(consume("."));
        name->mutable_name_part()->append(".");
        do(consumeidentifier(&identifier, "expected identifier."));
        name->mutable_name_part()->append(identifier);
      }
    }

    do(consume(")"));
    name->set_is_extension(true);
  } else {  // this is a regular field.
    locationrecorder location(
        part_location, uninterpretedoption::namepart::knamepartfieldnumber);
    do(consumeidentifier(&identifier, "expected identifier."));
    name->mutable_name_part()->append(identifier);
    name->set_is_extension(false);
  }
  return true;
}

bool parser::parseuninterpretedblock(string* value) {
  // note that enclosing braces are not added to *value.
  // we do not use consumeendofstatement for this brace because it's delimiting
  // an expression, not a block of statements.
  do(consume("{"));
  int brace_depth = 1;
  while (!atend()) {
    if (lookingat("{")) {
      brace_depth++;
    } else if (lookingat("}")) {
      brace_depth--;
      if (brace_depth == 0) {
        input_->next();
        return true;
      }
    }
    // todo(sanjay): interpret line/column numbers to preserve formatting
    if (!value->empty()) value->push_back(' ');
    value->append(input_->current().text);
    input_->next();
  }
  adderror("unexpected end of stream while parsing aggregate value.");
  return false;
}

// we don't interpret the option here. instead we store it in an
// uninterpretedoption, to be interpreted later.
bool parser::parseoption(message* options,
                         const locationrecorder& options_location,
                         optionstyle style) {
  // create an entry in the uninterpreted_option field.
  const fielddescriptor* uninterpreted_option_field = options->getdescriptor()->
      findfieldbyname("uninterpreted_option");
  google_check(uninterpreted_option_field != null)
      << "no field named \"uninterpreted_option\" in the options proto.";

  const reflection* reflection = options->getreflection();

  locationrecorder location(
      options_location, uninterpreted_option_field->number(),
      reflection->fieldsize(*options, uninterpreted_option_field));

  if (style == option_statement) {
    do(consume("option"));
  }

  uninterpretedoption* uninterpreted_option = down_cast<uninterpretedoption*>(
      options->getreflection()->addmessage(options,
                                           uninterpreted_option_field));

  // parse dot-separated name.
  {
    locationrecorder name_location(location,
                                   uninterpretedoption::knamefieldnumber);
    name_location.recordlegacylocation(
        uninterpreted_option, descriptorpool::errorcollector::option_name);

    {
      locationrecorder part_location(name_location,
                                     uninterpreted_option->name_size());
      do(parseoptionnamepart(uninterpreted_option, part_location));
    }

    while (lookingat(".")) {
      do(consume("."));
      locationrecorder part_location(name_location,
                                     uninterpreted_option->name_size());
      do(parseoptionnamepart(uninterpreted_option, part_location));
    }
  }

  do(consume("="));

  {
    locationrecorder value_location(location);
    value_location.recordlegacylocation(
        uninterpreted_option, descriptorpool::errorcollector::option_value);

    // all values are a single token, except for negative numbers, which consist
    // of a single '-' symbol, followed by a positive number.
    bool is_negative = tryconsume("-");

    switch (input_->current().type) {
      case io::tokenizer::type_start:
        google_log(fatal) << "trying to read value before any tokens have been read.";
        return false;

      case io::tokenizer::type_end:
        adderror("unexpected end of stream while parsing option value.");
        return false;

      case io::tokenizer::type_identifier: {
        value_location.addpath(
            uninterpretedoption::kidentifiervaluefieldnumber);
        if (is_negative) {
          adderror("invalid '-' symbol before identifier.");
          return false;
        }
        string value;
        do(consumeidentifier(&value, "expected identifier."));
        uninterpreted_option->set_identifier_value(value);
        break;
      }

      case io::tokenizer::type_integer: {
        uint64 value;
        uint64 max_value =
            is_negative ? static_cast<uint64>(kint64max) + 1 : kuint64max;
        do(consumeinteger64(max_value, &value, "expected integer."));
        if (is_negative) {
          value_location.addpath(
              uninterpretedoption::knegativeintvaluefieldnumber);
          uninterpreted_option->set_negative_int_value(
              -static_cast<int64>(value));
        } else {
          value_location.addpath(
              uninterpretedoption::kpositiveintvaluefieldnumber);
          uninterpreted_option->set_positive_int_value(value);
        }
        break;
      }

      case io::tokenizer::type_float: {
        value_location.addpath(uninterpretedoption::kdoublevaluefieldnumber);
        double value;
        do(consumenumber(&value, "expected number."));
        uninterpreted_option->set_double_value(is_negative ? -value : value);
        break;
      }

      case io::tokenizer::type_string: {
        value_location.addpath(uninterpretedoption::kstringvaluefieldnumber);
        if (is_negative) {
          adderror("invalid '-' symbol before string.");
          return false;
        }
        string value;
        do(consumestring(&value, "expected string."));
        uninterpreted_option->set_string_value(value);
        break;
      }

      case io::tokenizer::type_symbol:
        if (lookingat("{")) {
          value_location.addpath(
              uninterpretedoption::kaggregatevaluefieldnumber);
          do(parseuninterpretedblock(
              uninterpreted_option->mutable_aggregate_value()));
        } else {
          adderror("expected option value.");
          return false;
        }
        break;
    }
  }

  if (style == option_statement) {
    do(consumeendofdeclaration(";", &location));
  }

  return true;
}

bool parser::parseextensions(descriptorproto* message,
                             const locationrecorder& extensions_location) {
  // parse the declaration.
  do(consume("extensions"));

  do {
    // note that kextensionrangefieldnumber was already pushed by the parent.
    locationrecorder location(extensions_location,
                              message->extension_range_size());

    descriptorproto::extensionrange* range = message->add_extension_range();
    location.recordlegacylocation(
        range, descriptorpool::errorcollector::number);

    int start, end;
    io::tokenizer::token start_token;

    {
      locationrecorder start_location(
          location, descriptorproto::extensionrange::kstartfieldnumber);
      start_token = input_->current();
      do(consumeinteger(&start, "expected field number range."));
    }

    if (tryconsume("to")) {
      locationrecorder end_location(
          location, descriptorproto::extensionrange::kendfieldnumber);
      if (tryconsume("max")) {
        // set to the sentinel value - 1 since we increment the value below.
        // the actual value of the end of the range should be set with
        // adjustextensionrangeswithmaxendnumber.
        end = kmaxextensionrangesentinel - 1;
      } else {
        do(consumeinteger(&end, "expected integer."));
      }
    } else {
      locationrecorder end_location(
          location, descriptorproto::extensionrange::kendfieldnumber);
      end_location.startat(start_token);
      end_location.endat(start_token);
      end = start;
    }

    // users like to specify inclusive ranges, but in code we like the end
    // number to be exclusive.
    ++end;

    range->set_start(start);
    range->set_end(end);
  } while (tryconsume(","));

  do(consumeendofdeclaration(";", &extensions_location));
  return true;
}

bool parser::parseextend(repeatedptrfield<fielddescriptorproto>* extensions,
                         repeatedptrfield<descriptorproto>* messages,
                         const locationrecorder& parent_location,
                         int location_field_number_for_nested_type,
                         const locationrecorder& extend_location) {
  do(consume("extend"));

  // parse the extendee type.
  io::tokenizer::token extendee_start = input_->current();
  string extendee;
  do(parseuserdefinedtype(&extendee));
  io::tokenizer::token extendee_end = input_->previous();

  // parse the block.
  do(consumeendofdeclaration("{", &extend_location));

  bool is_first = true;

  do {
    if (atend()) {
      adderror("reached end of input in extend definition (missing '}').");
      return false;
    }

    // note that kextensionfieldnumber was already pushed by the parent.
    locationrecorder location(extend_location, extensions->size());

    fielddescriptorproto* field = extensions->add();

    {
      locationrecorder extendee_location(
          location, fielddescriptorproto::kextendeefieldnumber);
      extendee_location.startat(extendee_start);
      extendee_location.endat(extendee_end);

      if (is_first) {
        extendee_location.recordlegacylocation(
            field, descriptorpool::errorcollector::extendee);
        is_first = false;
      }
    }

    field->set_extendee(extendee);

    if (!parsemessagefield(field, messages, parent_location,
                           location_field_number_for_nested_type,
                           location)) {
      // this statement failed to parse.  skip it, but keep looping to parse
      // other statements.
      skipstatement();
    }
  } while (!tryconsumeendofdeclaration("}", null));

  return true;
}

// -------------------------------------------------------------------
// enums

bool parser::parseenumdefinition(enumdescriptorproto* enum_type,
                                 const locationrecorder& enum_location) {
  do(consume("enum"));

  {
    locationrecorder location(enum_location,
                              enumdescriptorproto::knamefieldnumber);
    location.recordlegacylocation(
        enum_type, descriptorpool::errorcollector::name);
    do(consumeidentifier(enum_type->mutable_name(), "expected enum name."));
  }

  do(parseenumblock(enum_type, enum_location));
  return true;
}

bool parser::parseenumblock(enumdescriptorproto* enum_type,
                            const locationrecorder& enum_location) {
  do(consumeendofdeclaration("{", &enum_location));

  while (!tryconsumeendofdeclaration("}", null)) {
    if (atend()) {
      adderror("reached end of input in enum definition (missing '}').");
      return false;
    }

    if (!parseenumstatement(enum_type, enum_location)) {
      // this statement failed to parse.  skip it, but keep looping to parse
      // other statements.
      skipstatement();
    }
  }

  return true;
}

bool parser::parseenumstatement(enumdescriptorproto* enum_type,
                                const locationrecorder& enum_location) {
  if (tryconsumeendofdeclaration(";", null)) {
    // empty statement; ignore
    return true;
  } else if (lookingat("option")) {
    locationrecorder location(enum_location,
                              enumdescriptorproto::koptionsfieldnumber);
    return parseoption(enum_type->mutable_options(), location,
                       option_statement);
  } else {
    locationrecorder location(enum_location,
        enumdescriptorproto::kvaluefieldnumber, enum_type->value_size());
    return parseenumconstant(enum_type->add_value(), location);
  }
}

bool parser::parseenumconstant(enumvaluedescriptorproto* enum_value,
                               const locationrecorder& enum_value_location) {
  // parse name.
  {
    locationrecorder location(enum_value_location,
                              enumvaluedescriptorproto::knamefieldnumber);
    location.recordlegacylocation(
        enum_value, descriptorpool::errorcollector::name);
    do(consumeidentifier(enum_value->mutable_name(),
                         "expected enum constant name."));
  }

  do(consume("=", "missing numeric value for enum constant."));

  // parse value.
  {
    locationrecorder location(
        enum_value_location, enumvaluedescriptorproto::knumberfieldnumber);
    location.recordlegacylocation(
        enum_value, descriptorpool::errorcollector::number);

    int number;
    do(consumesignedinteger(&number, "expected integer."));
    enum_value->set_number(number);
  }

  do(parseenumconstantoptions(enum_value, enum_value_location));

  do(consumeendofdeclaration(";", &enum_value_location));

  return true;
}

bool parser::parseenumconstantoptions(
    enumvaluedescriptorproto* value,
    const locationrecorder& enum_value_location) {
  if (!lookingat("[")) return true;

  locationrecorder location(
      enum_value_location, enumvaluedescriptorproto::koptionsfieldnumber);

  do(consume("["));

  do {
    do(parseoption(value->mutable_options(), location, option_assignment));
  } while (tryconsume(","));

  do(consume("]"));
  return true;
}

// -------------------------------------------------------------------
// services

bool parser::parseservicedefinition(servicedescriptorproto* service,
                                    const locationrecorder& service_location) {
  do(consume("service"));

  {
    locationrecorder location(service_location,
                              servicedescriptorproto::knamefieldnumber);
    location.recordlegacylocation(
        service, descriptorpool::errorcollector::name);
    do(consumeidentifier(service->mutable_name(), "expected service name."));
  }

  do(parseserviceblock(service, service_location));
  return true;
}

bool parser::parseserviceblock(servicedescriptorproto* service,
                               const locationrecorder& service_location) {
  do(consumeendofdeclaration("{", &service_location));

  while (!tryconsumeendofdeclaration("}", null)) {
    if (atend()) {
      adderror("reached end of input in service definition (missing '}').");
      return false;
    }

    if (!parseservicestatement(service, service_location)) {
      // this statement failed to parse.  skip it, but keep looping to parse
      // other statements.
      skipstatement();
    }
  }

  return true;
}

bool parser::parseservicestatement(servicedescriptorproto* service,
                                   const locationrecorder& service_location) {
  if (tryconsumeendofdeclaration(";", null)) {
    // empty statement; ignore
    return true;
  } else if (lookingat("option")) {
    locationrecorder location(
        service_location, servicedescriptorproto::koptionsfieldnumber);
    return parseoption(service->mutable_options(), location, option_statement);
  } else {
    locationrecorder location(service_location,
        servicedescriptorproto::kmethodfieldnumber, service->method_size());
    return parseservicemethod(service->add_method(), location);
  }
}

bool parser::parseservicemethod(methoddescriptorproto* method,
                                const locationrecorder& method_location) {
  do(consume("rpc"));

  {
    locationrecorder location(method_location,
                              methoddescriptorproto::knamefieldnumber);
    location.recordlegacylocation(
        method, descriptorpool::errorcollector::name);
    do(consumeidentifier(method->mutable_name(), "expected method name."));
  }

  // parse input type.
  do(consume("("));
  {
    locationrecorder location(method_location,
                              methoddescriptorproto::kinputtypefieldnumber);
    location.recordlegacylocation(
        method, descriptorpool::errorcollector::input_type);
    do(parseuserdefinedtype(method->mutable_input_type()));
  }
  do(consume(")"));

  // parse output type.
  do(consume("returns"));
  do(consume("("));
  {
    locationrecorder location(method_location,
                              methoddescriptorproto::koutputtypefieldnumber);
    location.recordlegacylocation(
        method, descriptorpool::errorcollector::output_type);
    do(parseuserdefinedtype(method->mutable_output_type()));
  }
  do(consume(")"));

  if (lookingat("{")) {
    // options!
    do(parseoptions(method_location,
                    methoddescriptorproto::koptionsfieldnumber,
                    method->mutable_options()));
  } else {
    do(consumeendofdeclaration(";", &method_location));
  }

  return true;
}


bool parser::parseoptions(const locationrecorder& parent_location,
                          const int optionsfieldnumber,
                          message* mutable_options) {
  // options!
  consumeendofdeclaration("{", &parent_location);
  while (!tryconsumeendofdeclaration("}", null)) {
    if (atend()) {
      adderror("reached end of input in method options (missing '}').");
      return false;
    }

    if (tryconsumeendofdeclaration(";", null)) {
      // empty statement; ignore
    } else {
      locationrecorder location(parent_location,
                                optionsfieldnumber);
      if (!parseoption(mutable_options, location, option_statement)) {
        // this statement failed to parse.  skip it, but keep looping to
        // parse other statements.
        skipstatement();
      }
    }
  }

  return true;
}

// -------------------------------------------------------------------

bool parser::parselabel(fielddescriptorproto::label* label) {
  if (tryconsume("optional")) {
    *label = fielddescriptorproto::label_optional;
    return true;
  } else if (tryconsume("repeated")) {
    *label = fielddescriptorproto::label_repeated;
    return true;
  } else if (tryconsume("required")) {
    *label = fielddescriptorproto::label_required;
    return true;
  } else {
    adderror("expected \"required\", \"optional\", or \"repeated\".");
    // we can actually reasonably recover here by just assuming the user
    // forgot the label altogether.
    *label = fielddescriptorproto::label_optional;
    return true;
  }
}

bool parser::parsetype(fielddescriptorproto::type* type,
                       string* type_name) {
  typenamemap::const_iterator iter = ktypenames.find(input_->current().text);
  if (iter != ktypenames.end()) {
    *type = iter->second;
    input_->next();
  } else {
    do(parseuserdefinedtype(type_name));
  }
  return true;
}

bool parser::parseuserdefinedtype(string* type_name) {
  type_name->clear();

  typenamemap::const_iterator iter = ktypenames.find(input_->current().text);
  if (iter != ktypenames.end()) {
    // note:  the only place enum types are allowed is for field types, but
    //   if we are parsing a field type then we would not get here because
    //   primitives are allowed there as well.  so this error message doesn't
    //   need to account for enums.
    adderror("expected message type.");

    // pretend to accept this type so that we can go on parsing.
    *type_name = input_->current().text;
    input_->next();
    return true;
  }

  // a leading "." means the name is fully-qualified.
  if (tryconsume(".")) type_name->append(".");

  // consume the first part of the name.
  string identifier;
  do(consumeidentifier(&identifier, "expected type name."));
  type_name->append(identifier);

  // consume more parts.
  while (tryconsume(".")) {
    type_name->append(".");
    do(consumeidentifier(&identifier, "expected identifier."));
    type_name->append(identifier);
  }

  return true;
}

// ===================================================================

bool parser::parsepackage(filedescriptorproto* file,
                          const locationrecorder& root_location) {
  if (file->has_package()) {
    adderror("multiple package definitions.");
    // don't append the new package to the old one.  just replace it.  not
    // that it really matters since this is an error anyway.
    file->clear_package();
  }

  do(consume("package"));

  {
    locationrecorder location(root_location,
                              filedescriptorproto::kpackagefieldnumber);
    location.recordlegacylocation(file, descriptorpool::errorcollector::name);

    while (true) {
      string identifier;
      do(consumeidentifier(&identifier, "expected identifier."));
      file->mutable_package()->append(identifier);
      if (!tryconsume(".")) break;
      file->mutable_package()->append(".");
    }

    location.endat(input_->previous());

    do(consumeendofdeclaration(";", &location));
  }

  return true;
}

bool parser::parseimport(repeatedptrfield<string>* dependency,
                         repeatedfield<int32>* public_dependency,
                         repeatedfield<int32>* weak_dependency,
                         const locationrecorder& root_location) {
  do(consume("import"));
  if (lookingat("public")) {
    locationrecorder location(
        root_location, filedescriptorproto::kpublicdependencyfieldnumber,
        public_dependency->size());
    do(consume("public"));
    *public_dependency->add() = dependency->size();
  } else if (lookingat("weak")) {
    locationrecorder location(
        root_location, filedescriptorproto::kweakdependencyfieldnumber,
        weak_dependency->size());
    do(consume("weak"));
    *weak_dependency->add() = dependency->size();
  }
  {
    locationrecorder location(root_location,
                              filedescriptorproto::kdependencyfieldnumber,
                              dependency->size());
    do(consumestring(dependency->add(),
      "expected a string naming the file to import."));

    location.endat(input_->previous());

    do(consumeendofdeclaration(";", &location));
  }
  return true;
}

// ===================================================================

sourcelocationtable::sourcelocationtable() {}
sourcelocationtable::~sourcelocationtable() {}

bool sourcelocationtable::find(
    const message* descriptor,
    descriptorpool::errorcollector::errorlocation location,
    int* line, int* column) const {
  const pair<int, int>* result =
    findornull(location_map_, make_pair(descriptor, location));
  if (result == null) {
    *line   = -1;
    *column = 0;
    return false;
  } else {
    *line   = result->first;
    *column = result->second;
    return true;
  }
}

void sourcelocationtable::add(
    const message* descriptor,
    descriptorpool::errorcollector::errorlocation location,
    int line, int column) {
  location_map_[make_pair(descriptor, location)] = make_pair(line, column);
}

void sourcelocationtable::clear() {
  location_map_.clear();
}

}  // namespace compiler
}  // namespace protobuf
}  // namespace google
