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

// author: jschorr@google.com (joseph schorr)
//  based on original protocol buffers design by
//  sanjay ghemawat, jeff dean, and others.

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stack>
#include <limits>
#include <vector>

#include <google/protobuf/text_format.h>

#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/map-util.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {

string message::debugstring() const {
  string debug_string;

  textformat::printtostring(*this, &debug_string);

  return debug_string;
}

string message::shortdebugstring() const {
  string debug_string;

  textformat::printer printer;
  printer.setsinglelinemode(true);

  printer.printtostring(*this, &debug_string);
  // single line mode currently might have an extra space at the end.
  if (debug_string.size() > 0 &&
      debug_string[debug_string.size() - 1] == ' ') {
    debug_string.resize(debug_string.size() - 1);
  }

  return debug_string;
}

string message::utf8debugstring() const {
  string debug_string;

  textformat::printer printer;
  printer.setuseutf8stringescaping(true);

  printer.printtostring(*this, &debug_string);

  return debug_string;
}

void message::printdebugstring() const {
  printf("%s", debugstring().c_str());
}


// ===========================================================================
// implementation of the parse information tree class.
textformat::parseinfotree::parseinfotree() { }

textformat::parseinfotree::~parseinfotree() {
  // remove any nested information trees, as they are owned by this tree.
  for (nestedmap::iterator it = nested_.begin(); it != nested_.end(); ++it) {
    stldeleteelements(&(it->second));
  }
}

void textformat::parseinfotree::recordlocation(
    const fielddescriptor* field,
    textformat::parselocation location) {
  locations_[field].push_back(location);
}

textformat::parseinfotree* textformat::parseinfotree::createnested(
    const fielddescriptor* field) {
  // owned by us in the map.
  textformat::parseinfotree* instance = new textformat::parseinfotree();
  vector<textformat::parseinfotree*>* trees = &nested_[field];
  google_check(trees);
  trees->push_back(instance);
  return instance;
}

void checkfieldindex(const fielddescriptor* field, int index) {
  if (field == null) { return; }

  if (field->is_repeated() && index == -1) {
    google_log(dfatal) << "index must be in range of repeated field values. "
                << "field: " << field->name();
  } else if (!field->is_repeated() && index != -1) {
    google_log(dfatal) << "index must be -1 for singular fields."
                << "field: " << field->name();
  }
}

textformat::parselocation textformat::parseinfotree::getlocation(
    const fielddescriptor* field, int index) const {
  checkfieldindex(field, index);
  if (index == -1) { index = 0; }

  const vector<textformat::parselocation>* locations =
      findornull(locations_, field);
  if (locations == null || index >= locations->size()) {
    return textformat::parselocation();
  }

  return (*locations)[index];
}

textformat::parseinfotree* textformat::parseinfotree::gettreefornested(
    const fielddescriptor* field, int index) const {
  checkfieldindex(field, index);
  if (index == -1) { index = 0; }

  const vector<textformat::parseinfotree*>* trees = findornull(nested_, field);
  if (trees == null || index >= trees->size()) {
    return null;
  }

  return (*trees)[index];
}


// ===========================================================================
// internal class for parsing an ascii representation of a protocol message.
// this class makes use of the protocol message compiler's tokenizer found
// in //google/protobuf/io/tokenizer.h. note that class's parse
// method is *not* thread-safe and should only be used in a single thread at
// a time.

// makes code slightly more readable.  the meaning of "do(foo)" is
// "execute foo and fail if it fails.", where failure is indicated by
// returning false. borrowed from parser.cc (thanks kenton!).
#define do(statement) if (statement) {} else return false

class textformat::parser::parserimpl {
 public:

  // determines if repeated values for a non-repeated field are
  // permitted, e.g., the string "foo: 1 foo: 2" for a
  // required/optional field named "foo".
  enum singularoverwritepolicy {
    allow_singular_overwrites = 0,   // the last value is retained
    forbid_singular_overwrites = 1,  // an error is issued
  };

  parserimpl(const descriptor* root_message_type,
             io::zerocopyinputstream* input_stream,
             io::errorcollector* error_collector,
             textformat::finder* finder,
             parseinfotree* parse_info_tree,
             singularoverwritepolicy singular_overwrite_policy,
             bool allow_unknown_field)
    : error_collector_(error_collector),
      finder_(finder),
      parse_info_tree_(parse_info_tree),
      tokenizer_error_collector_(this),
      tokenizer_(input_stream, &tokenizer_error_collector_),
      root_message_type_(root_message_type),
      singular_overwrite_policy_(singular_overwrite_policy),
      allow_unknown_field_(allow_unknown_field),
      had_errors_(false) {
    // for backwards-compatibility with proto1, we need to allow the 'f' suffix
    // for floats.
    tokenizer_.set_allow_f_after_float(true);

    // '#' starts a comment.
    tokenizer_.set_comment_style(io::tokenizer::sh_comment_style);

    // consume the starting token.
    tokenizer_.next();
  }
  ~parserimpl() { }

  // parses the ascii representation specified in input and saves the
  // information into the output pointer (a message). returns
  // false if an error occurs (an error will also be logged to
  // google_log(error)).
  bool parse(message* output) {
    // consume fields until we cannot do so anymore.
    while(true) {
      if (lookingattype(io::tokenizer::type_end)) {
        return !had_errors_;
      }

      do(consumefield(output));
    }
  }

  bool parsefield(const fielddescriptor* field, message* output) {
    bool suc;
    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      suc = consumefieldmessage(output, output->getreflection(), field);
    } else {
      suc = consumefieldvalue(output, output->getreflection(), field);
    }
    return suc && lookingattype(io::tokenizer::type_end);
  }

  void reporterror(int line, int col, const string& message) {
    had_errors_ = true;
    if (error_collector_ == null) {
      if (line >= 0) {
        google_log(error) << "error parsing text-format "
                   << root_message_type_->full_name()
                   << ": " << (line + 1) << ":"
                   << (col + 1) << ": " << message;
      } else {
        google_log(error) << "error parsing text-format "
                   << root_message_type_->full_name()
                   << ": " << message;
      }
    } else {
      error_collector_->adderror(line, col, message);
    }
  }

  void reportwarning(int line, int col, const string& message) {
    if (error_collector_ == null) {
      if (line >= 0) {
        google_log(warning) << "warning parsing text-format "
                     << root_message_type_->full_name()
                     << ": " << (line + 1) << ":"
                     << (col + 1) << ": " << message;
      } else {
        google_log(warning) << "warning parsing text-format "
                     << root_message_type_->full_name()
                     << ": " << message;
      }
    } else {
      error_collector_->addwarning(line, col, message);
    }
  }

 private:
  google_disallow_evil_constructors(parserimpl);

  // reports an error with the given message with information indicating
  // the position (as derived from the current token).
  void reporterror(const string& message) {
    reporterror(tokenizer_.current().line, tokenizer_.current().column,
                message);
  }

  // reports a warning with the given message with information indicating
  // the position (as derived from the current token).
  void reportwarning(const string& message) {
    reportwarning(tokenizer_.current().line, tokenizer_.current().column,
                  message);
  }

  // consumes the specified message with the given starting delimeter.
  // this method checks to see that the end delimeter at the conclusion of
  // the consumption matches the starting delimeter passed in here.
  bool consumemessage(message* message, const string delimeter) {
    while (!lookingat(">") &&  !lookingat("}")) {
      do(consumefield(message));
    }

    // confirm that we have a valid ending delimeter.
    do(consume(delimeter));

    return true;
  }

  // consumes the current field (as returned by the tokenizer) on the
  // passed in message.
  bool consumefield(message* message) {
    const reflection* reflection = message->getreflection();
    const descriptor* descriptor = message->getdescriptor();

    string field_name;

    const fielddescriptor* field = null;
    int start_line = tokenizer_.current().line;
    int start_column = tokenizer_.current().column;

    if (tryconsume("[")) {
      // extension.
      do(consumeidentifier(&field_name));
      while (tryconsume(".")) {
        string part;
        do(consumeidentifier(&part));
        field_name += ".";
        field_name += part;
      }
      do(consume("]"));

      field = (finder_ != null
               ? finder_->findextension(message, field_name)
               : reflection->findknownextensionbyname(field_name));

      if (field == null) {
        if (!allow_unknown_field_) {
          reporterror("extension \"" + field_name + "\" is not defined or "
                      "is not an extension of \"" +
                      descriptor->full_name() + "\".");
          return false;
        } else {
          reportwarning("extension \"" + field_name + "\" is not defined or "
                        "is not an extension of \"" +
                        descriptor->full_name() + "\".");
        }
      }
    } else {
      do(consumeidentifier(&field_name));

      field = descriptor->findfieldbyname(field_name);
      // group names are expected to be capitalized as they appear in the
      // .proto file, which actually matches their type names, not their field
      // names.
      if (field == null) {
        string lower_field_name = field_name;
        lowerstring(&lower_field_name);
        field = descriptor->findfieldbyname(lower_field_name);
        // if the case-insensitive match worked but the field is not a group,
        if (field != null && field->type() != fielddescriptor::type_group) {
          field = null;
        }
      }
      // again, special-case group names as described above.
      if (field != null && field->type() == fielddescriptor::type_group
          && field->message_type()->name() != field_name) {
        field = null;
      }

      if (field == null) {
        if (!allow_unknown_field_) {
          reporterror("message type \"" + descriptor->full_name() +
                      "\" has no field named \"" + field_name + "\".");
          return false;
        } else {
          reportwarning("message type \"" + descriptor->full_name() +
                        "\" has no field named \"" + field_name + "\".");
        }
      }
    }

    // skips unknown field.
    if (field == null) {
      google_check(allow_unknown_field_);
      // try to guess the type of this field.
      // if this field is not a message, there should be a ":" between the
      // field name and the field value and also the field value should not
      // start with "{" or "<" which indicates the begining of a message body.
      // if there is no ":" or there is a "{" or "<" after ":", this field has
      // to be a message or the input is ill-formed.
      if (tryconsume(":") && !lookingat("{") && !lookingat("<")) {
        return skipfieldvalue();
      } else {
        return skipfieldmessage();
      }
    }

    // fail if the field is not repeated and it has already been specified.
    if ((singular_overwrite_policy_ == forbid_singular_overwrites) &&
        !field->is_repeated() && reflection->hasfield(*message, field)) {
      reporterror("non-repeated field \"" + field_name +
                  "\" is specified multiple times.");
      return false;
    }

    // perform special handling for embedded message types.
    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      // ':' is optional here.
      tryconsume(":");
      do(consumefieldmessage(message, reflection, field));
    } else {
      do(consume(":"));
      if (field->is_repeated() && tryconsume("[")) {
        // short repeated format, e.g.  "foo: [1, 2, 3]"
        while (true) {
          do(consumefieldvalue(message, reflection, field));
          if (tryconsume("]")) {
            break;
          }
          do(consume(","));
        }
      } else {
        do(consumefieldvalue(message, reflection, field));
      }
    }

    // for historical reasons, fields may optionally be separated by commas or
    // semicolons.
    tryconsume(";") || tryconsume(",");

    if (field->options().deprecated()) {
      reportwarning("text format contains deprecated field \""
                    + field_name + "\"");
    }

    // if a parse info tree exists, add the location for the parsed
    // field.
    if (parse_info_tree_ != null) {
      recordlocation(parse_info_tree_, field,
                     parselocation(start_line, start_column));
    }

    return true;
  }

  // skips the next field including the field's name and value.
  bool skipfield() {
    string field_name;
    if (tryconsume("[")) {
      // extension name.
      do(consumeidentifier(&field_name));
      while (tryconsume(".")) {
        string part;
        do(consumeidentifier(&part));
        field_name += ".";
        field_name += part;
      }
      do(consume("]"));
    } else {
      do(consumeidentifier(&field_name));
    }

    // try to guess the type of this field.
    // if this field is not a message, there should be a ":" between the
    // field name and the field value and also the field value should not
    // start with "{" or "<" which indicates the begining of a message body.
    // if there is no ":" or there is a "{" or "<" after ":", this field has
    // to be a message or the input is ill-formed.
    if (tryconsume(":") && !lookingat("{") && !lookingat("<")) {
      do(skipfieldvalue());
    } else {
      do(skipfieldmessage());
    }
    // for historical reasons, fields may optionally be separated by commas or
    // semicolons.
    tryconsume(";") || tryconsume(",");
    return true;
  }

  bool consumefieldmessage(message* message,
                           const reflection* reflection,
                           const fielddescriptor* field) {

    // if the parse information tree is not null, create a nested one
    // for the nested message.
    parseinfotree* parent = parse_info_tree_;
    if (parent != null) {
      parse_info_tree_ = createnested(parent, field);
    }

    string delimeter;
    if (tryconsume("<")) {
      delimeter = ">";
    } else {
      do(consume("{"));
      delimeter = "}";
    }

    if (field->is_repeated()) {
      do(consumemessage(reflection->addmessage(message, field), delimeter));
    } else {
      do(consumemessage(reflection->mutablemessage(message, field),
                        delimeter));
    }

    // reset the parse information tree.
    parse_info_tree_ = parent;
    return true;
  }

  // skips the whole body of a message including the begining delimeter and
  // the ending delimeter.
  bool skipfieldmessage() {
    string delimeter;
    if (tryconsume("<")) {
      delimeter = ">";
    } else {
      do(consume("{"));
      delimeter = "}";
    }
    while (!lookingat(">") &&  !lookingat("}")) {
      do(skipfield());
    }
    do(consume(delimeter));
    return true;
  }

  bool consumefieldvalue(message* message,
                         const reflection* reflection,
                         const fielddescriptor* field) {

// define an easy to use macro for setting fields. this macro checks
// to see if the field is repeated (in which case we need to use the add
// methods or not (in which case we need to use the set methods).
#define set_field(cpptype, value)                                  \
        if (field->is_repeated()) {                                \
          reflection->add##cpptype(message, field, value);         \
        } else {                                                   \
          reflection->set##cpptype(message, field, value);         \
        }                                                          \

    switch(field->cpp_type()) {
      case fielddescriptor::cpptype_int32: {
        int64 value;
        do(consumesignedinteger(&value, kint32max));
        set_field(int32, static_cast<int32>(value));
        break;
      }

      case fielddescriptor::cpptype_uint32: {
        uint64 value;
        do(consumeunsignedinteger(&value, kuint32max));
        set_field(uint32, static_cast<uint32>(value));
        break;
      }

      case fielddescriptor::cpptype_int64: {
        int64 value;
        do(consumesignedinteger(&value, kint64max));
        set_field(int64, value);
        break;
      }

      case fielddescriptor::cpptype_uint64: {
        uint64 value;
        do(consumeunsignedinteger(&value, kuint64max));
        set_field(uint64, value);
        break;
      }

      case fielddescriptor::cpptype_float: {
        double value;
        do(consumedouble(&value));
        set_field(float, static_cast<float>(value));
        break;
      }

      case fielddescriptor::cpptype_double: {
        double value;
        do(consumedouble(&value));
        set_field(double, value);
        break;
      }

      case fielddescriptor::cpptype_string: {
        string value;
        do(consumestring(&value));
        set_field(string, value);
        break;
      }

      case fielddescriptor::cpptype_bool: {
        if (lookingattype(io::tokenizer::type_integer)) {
          uint64 value;
          do(consumeunsignedinteger(&value, 1));
          set_field(bool, value);
        } else {
          string value;
          do(consumeidentifier(&value));
          if (value == "true" || value == "t") {
            set_field(bool, true);
          } else if (value == "false" || value == "f") {
            set_field(bool, false);
          } else {
            reporterror("invalid value for boolean field \"" + field->name()
                        + "\". value: \"" + value  + "\".");
            return false;
          }
        }
        break;
      }

      case fielddescriptor::cpptype_enum: {
        string value;
        const enumdescriptor* enum_type = field->enum_type();
        const enumvaluedescriptor* enum_value = null;

        if (lookingattype(io::tokenizer::type_identifier)) {
          do(consumeidentifier(&value));
          // find the enumeration value.
          enum_value = enum_type->findvaluebyname(value);

        } else if (lookingat("-") ||
                   lookingattype(io::tokenizer::type_integer)) {
          int64 int_value;
          do(consumesignedinteger(&int_value, kint32max));
          value = simpleitoa(int_value);        // for error reporting
          enum_value = enum_type->findvaluebynumber(int_value);
        } else {
          reporterror("expected integer or identifier.");
          return false;
        }

        if (enum_value == null) {
          reporterror("unknown enumeration value of \"" + value  + "\" for "
                      "field \"" + field->name() + "\".");
          return false;
        }

        set_field(enum, enum_value);
        break;
      }

      case fielddescriptor::cpptype_message: {
        // we should never get here. put here instead of a default
        // so that if new types are added, we get a nice compiler warning.
        google_log(fatal) << "reached an unintended state: cpptype_message";
        break;
      }
    }
#undef set_field
    return true;
  }

  bool skipfieldvalue() {
    if (lookingattype(io::tokenizer::type_string)) {
      while (lookingattype(io::tokenizer::type_string)) {
        tokenizer_.next();
      }
      return true;
    }
    // possible field values other than string:
    //   12345        => type_integer
    //   -12345       => type_symbol + type_integer
    //   1.2345       => type_float
    //   -1.2345      => type_symbol + type_float
    //   inf          => type_identifier
    //   -inf         => type_symbol + type_identifier
    //   type_integer => type_identifier
    // divides them into two group, one with type_symbol
    // and the other without:
    //   group one:
    //     12345        => type_integer
    //     1.2345       => type_float
    //     inf          => type_identifier
    //     type_integer => type_identifier
    //   group two:
    //     -12345       => type_symbol + type_integer
    //     -1.2345      => type_symbol + type_float
    //     -inf         => type_symbol + type_identifier
    // as we can see, the field value consists of an optional '-' and one of
    // type_integer, type_float and type_identifier.
    bool has_minus = tryconsume("-");
    if (!lookingattype(io::tokenizer::type_integer) &&
        !lookingattype(io::tokenizer::type_float) &&
        !lookingattype(io::tokenizer::type_identifier)) {
      return false;
    }
    // combination of '-' and type_identifier may result in an invalid field
    // value while other combinations all generate valid values.
    // we check if the value of this combination is valid here.
    // type_identifier after a '-' should be one of the float values listed
    // below:
    //   inf, inff, infinity, nan
    if (has_minus && lookingattype(io::tokenizer::type_identifier)) {
      string text = tokenizer_.current().text;
      lowerstring(&text);
      if (text != "inf" &&
          text != "infinity" &&
          text != "nan") {
        reporterror("invalid float number: " + text);
        return false;
      }
    }
    tokenizer_.next();
    return true;
  }

  // returns true if the current token's text is equal to that specified.
  bool lookingat(const string& text) {
    return tokenizer_.current().text == text;
  }

  // returns true if the current token's type is equal to that specified.
  bool lookingattype(io::tokenizer::tokentype token_type) {
    return tokenizer_.current().type == token_type;
  }

  // consumes an identifier and saves its value in the identifier parameter.
  // returns false if the token is not of type identfier.
  bool consumeidentifier(string* identifier) {
    if (!lookingattype(io::tokenizer::type_identifier)) {
      reporterror("expected identifier.");
      return false;
    }

    *identifier = tokenizer_.current().text;

    tokenizer_.next();
    return true;
  }

  // consumes a string and saves its value in the text parameter.
  // returns false if the token is not of type string.
  bool consumestring(string* text) {
    if (!lookingattype(io::tokenizer::type_string)) {
      reporterror("expected string.");
      return false;
    }

    text->clear();
    while (lookingattype(io::tokenizer::type_string)) {
      io::tokenizer::parsestringappend(tokenizer_.current().text, text);

      tokenizer_.next();
    }

    return true;
  }

  // consumes a uint64 and saves its value in the value parameter.
  // returns false if the token is not of type integer.
  bool consumeunsignedinteger(uint64* value, uint64 max_value) {
    if (!lookingattype(io::tokenizer::type_integer)) {
      reporterror("expected integer.");
      return false;
    }

    if (!io::tokenizer::parseinteger(tokenizer_.current().text,
                                     max_value, value)) {
      reporterror("integer out of range.");
      return false;
    }

    tokenizer_.next();
    return true;
  }

  // consumes an int64 and saves its value in the value parameter.
  // note that since the tokenizer does not support negative numbers,
  // we actually may consume an additional token (for the minus sign) in this
  // method. returns false if the token is not an integer
  // (signed or otherwise).
  bool consumesignedinteger(int64* value, uint64 max_value) {
    bool negative = false;

    if (tryconsume("-")) {
      negative = true;
      // two's complement always allows one more negative integer than
      // positive.
      ++max_value;
    }

    uint64 unsigned_value;

    do(consumeunsignedinteger(&unsigned_value, max_value));

    *value = static_cast<int64>(unsigned_value);

    if (negative) {
      *value = -*value;
    }

    return true;
  }

  // consumes a double and saves its value in the value parameter.
  // note that since the tokenizer does not support negative numbers,
  // we actually may consume an additional token (for the minus sign) in this
  // method. returns false if the token is not a double
  // (signed or otherwise).
  bool consumedouble(double* value) {
    bool negative = false;

    if (tryconsume("-")) {
      negative = true;
    }

    // a double can actually be an integer, according to the tokenizer.
    // therefore, we must check both cases here.
    if (lookingattype(io::tokenizer::type_integer)) {
      // we have found an integer value for the double.
      uint64 integer_value;
      do(consumeunsignedinteger(&integer_value, kuint64max));

      *value = static_cast<double>(integer_value);
    } else if (lookingattype(io::tokenizer::type_float)) {
      // we have found a float value for the double.
      *value = io::tokenizer::parsefloat(tokenizer_.current().text);

      // mark the current token as consumed.
      tokenizer_.next();
    } else if (lookingattype(io::tokenizer::type_identifier)) {
      string text = tokenizer_.current().text;
      lowerstring(&text);
      if (text == "inf" ||
          text == "infinity") {
        *value = std::numeric_limits<double>::infinity();
        tokenizer_.next();
      } else if (text == "nan") {
        *value = std::numeric_limits<double>::quiet_nan();
        tokenizer_.next();
      } else {
        reporterror("expected double.");
        return false;
      }
    } else {
      reporterror("expected double.");
      return false;
    }

    if (negative) {
      *value = -*value;
    }

    return true;
  }

  // consumes a token and confirms that it matches that specified in the
  // value parameter. returns false if the token found does not match that
  // which was specified.
  bool consume(const string& value) {
    const string& current_value = tokenizer_.current().text;

    if (current_value != value) {
      reporterror("expected \"" + value + "\", found \"" + current_value
                  + "\".");
      return false;
    }

    tokenizer_.next();

    return true;
  }

  // attempts to consume the supplied value. returns false if a the
  // token found does not match the value specified.
  bool tryconsume(const string& value) {
    if (tokenizer_.current().text == value) {
      tokenizer_.next();
      return true;
    } else {
      return false;
    }
  }

  // an internal instance of the tokenizer's error collector, used to
  // collect any base-level parse errors and feed them to the parserimpl.
  class parsererrorcollector : public io::errorcollector {
   public:
    explicit parsererrorcollector(textformat::parser::parserimpl* parser) :
        parser_(parser) { }

    virtual ~parsererrorcollector() { };

    virtual void adderror(int line, int column, const string& message) {
      parser_->reporterror(line, column, message);
    }

    virtual void addwarning(int line, int column, const string& message) {
      parser_->reportwarning(line, column, message);
    }

   private:
    google_disallow_evil_constructors(parsererrorcollector);
    textformat::parser::parserimpl* parser_;
  };

  io::errorcollector* error_collector_;
  textformat::finder* finder_;
  parseinfotree* parse_info_tree_;
  parsererrorcollector tokenizer_error_collector_;
  io::tokenizer tokenizer_;
  const descriptor* root_message_type_;
  singularoverwritepolicy singular_overwrite_policy_;
  bool allow_unknown_field_;
  bool had_errors_;
};

#undef do

// ===========================================================================
// internal class for writing text to the io::zerocopyoutputstream. adapted
// from the printer found in //google/protobuf/io/printer.h
class textformat::printer::textgenerator {
 public:
  explicit textgenerator(io::zerocopyoutputstream* output,
                         int initial_indent_level)
    : output_(output),
      buffer_(null),
      buffer_size_(0),
      at_start_of_line_(true),
      failed_(false),
      indent_(""),
      initial_indent_level_(initial_indent_level) {
    indent_.resize(initial_indent_level_ * 2, ' ');
  }

  ~textgenerator() {
    // only backup() if we're sure we've successfully called next() at least
    // once.
    if (!failed_ && buffer_size_ > 0) {
      output_->backup(buffer_size_);
    }
  }

  // indent text by two spaces.  after calling indent(), two spaces will be
  // inserted at the beginning of each line of text.  indent() may be called
  // multiple times to produce deeper indents.
  void indent() {
    indent_ += "  ";
  }

  // reduces the current indent level by two spaces, or crashes if the indent
  // level is zero.
  void outdent() {
    if (indent_.empty() ||
        indent_.size() < initial_indent_level_ * 2) {
      google_log(dfatal) << " outdent() without matching indent().";
      return;
    }

    indent_.resize(indent_.size() - 2);
  }

  // print text to the output stream.
  void print(const string& str) {
    print(str.data(), str.size());
  }

  // print text to the output stream.
  void print(const char* text) {
    print(text, strlen(text));
  }

  // print text to the output stream.
  void print(const char* text, int size) {
    int pos = 0;  // the number of bytes we've written so far.

    for (int i = 0; i < size; i++) {
      if (text[i] == '\n') {
        // saw newline.  if there is more text, we may need to insert an indent
        // here.  so, write what we have so far, including the '\n'.
        write(text + pos, i - pos + 1);
        pos = i + 1;

        // setting this true will cause the next write() to insert an indent
        // first.
        at_start_of_line_ = true;
      }
    }

    // write the rest.
    write(text + pos, size - pos);
  }

  // true if any write to the underlying stream failed.  (we don't just
  // crash in this case because this is an i/o failure, not a programming
  // error.)
  bool failed() const { return failed_; }

 private:
  google_disallow_evil_constructors(textgenerator);

  void write(const char* data, int size) {
    if (failed_) return;
    if (size == 0) return;

    if (at_start_of_line_) {
      // insert an indent.
      at_start_of_line_ = false;
      write(indent_.data(), indent_.size());
      if (failed_) return;
    }

    while (size > buffer_size_) {
      // data exceeds space in the buffer.  copy what we can and request a
      // new buffer.
      memcpy(buffer_, data, buffer_size_);
      data += buffer_size_;
      size -= buffer_size_;
      void* void_buffer;
      failed_ = !output_->next(&void_buffer, &buffer_size_);
      if (failed_) return;
      buffer_ = reinterpret_cast<char*>(void_buffer);
    }

    // buffer is big enough to receive the data; copy it.
    memcpy(buffer_, data, size);
    buffer_ += size;
    buffer_size_ -= size;
  }

  io::zerocopyoutputstream* const output_;
  char* buffer_;
  int buffer_size_;
  bool at_start_of_line_;
  bool failed_;

  string indent_;
  int initial_indent_level_;
};

// ===========================================================================

textformat::finder::~finder() {
}

textformat::parser::parser()
  : error_collector_(null),
    finder_(null),
    parse_info_tree_(null),
    allow_partial_(false),
    allow_unknown_field_(false) {
}

textformat::parser::~parser() {}

bool textformat::parser::parse(io::zerocopyinputstream* input,
                               message* output) {
  output->clear();
  parserimpl parser(output->getdescriptor(), input, error_collector_,
                    finder_, parse_info_tree_,
                    parserimpl::forbid_singular_overwrites,
                    allow_unknown_field_);
  return mergeusingimpl(input, output, &parser);
}

bool textformat::parser::parsefromstring(const string& input,
                                         message* output) {
  io::arrayinputstream input_stream(input.data(), input.size());
  return parse(&input_stream, output);
}

bool textformat::parser::merge(io::zerocopyinputstream* input,
                               message* output) {
  parserimpl parser(output->getdescriptor(), input, error_collector_,
                    finder_, parse_info_tree_,
                    parserimpl::allow_singular_overwrites,
                    allow_unknown_field_);
  return mergeusingimpl(input, output, &parser);
}

bool textformat::parser::mergefromstring(const string& input,
                                         message* output) {
  io::arrayinputstream input_stream(input.data(), input.size());
  return merge(&input_stream, output);
}

bool textformat::parser::mergeusingimpl(io::zerocopyinputstream* input,
                                        message* output,
                                        parserimpl* parser_impl) {
  if (!parser_impl->parse(output)) return false;
  if (!allow_partial_ && !output->isinitialized()) {
    vector<string> missing_fields;
    output->findinitializationerrors(&missing_fields);
    parser_impl->reporterror(-1, 0, "message missing required fields: " +
                                    joinstrings(missing_fields, ", "));
    return false;
  }
  return true;
}

bool textformat::parser::parsefieldvaluefromstring(
    const string& input,
    const fielddescriptor* field,
    message* output) {
  io::arrayinputstream input_stream(input.data(), input.size());
  parserimpl parser(output->getdescriptor(), &input_stream, error_collector_,
                    finder_, parse_info_tree_,
                    parserimpl::allow_singular_overwrites,
                    allow_unknown_field_);
  return parser.parsefield(field, output);
}

/* static */ bool textformat::parse(io::zerocopyinputstream* input,
                                    message* output) {
  return parser().parse(input, output);
}

/* static */ bool textformat::merge(io::zerocopyinputstream* input,
                                    message* output) {
  return parser().merge(input, output);
}

/* static */ bool textformat::parsefromstring(const string& input,
                                              message* output) {
  return parser().parsefromstring(input, output);
}

/* static */ bool textformat::mergefromstring(const string& input,
                                              message* output) {
  return parser().mergefromstring(input, output);
}

// ===========================================================================

textformat::printer::printer()
  : initial_indent_level_(0),
    single_line_mode_(false),
    use_short_repeated_primitives_(false),
    utf8_string_escaping_(false) {}

textformat::printer::~printer() {}

bool textformat::printer::printtostring(const message& message,
                                        string* output) const {
  google_dcheck(output) << "output specified is null";

  output->clear();
  io::stringoutputstream output_stream(output);

  bool result = print(message, &output_stream);

  return result;
}

bool textformat::printer::printunknownfieldstostring(
    const unknownfieldset& unknown_fields,
    string* output) const {
  google_dcheck(output) << "output specified is null";

  output->clear();
  io::stringoutputstream output_stream(output);
  return printunknownfields(unknown_fields, &output_stream);
}

bool textformat::printer::print(const message& message,
                                io::zerocopyoutputstream* output) const {
  textgenerator generator(output, initial_indent_level_);

  print(message, generator);

  // output false if the generator failed internally.
  return !generator.failed();
}

bool textformat::printer::printunknownfields(
    const unknownfieldset& unknown_fields,
    io::zerocopyoutputstream* output) const {
  textgenerator generator(output, initial_indent_level_);

  printunknownfields(unknown_fields, generator);

  // output false if the generator failed internally.
  return !generator.failed();
}

void textformat::printer::print(const message& message,
                                textgenerator& generator) const {
  const reflection* reflection = message.getreflection();
  vector<const fielddescriptor*> fields;
  reflection->listfields(message, &fields);
  for (int i = 0; i < fields.size(); i++) {
    printfield(message, reflection, fields[i], generator);
  }
  printunknownfields(reflection->getunknownfields(message), generator);
}

void textformat::printer::printfieldvaluetostring(
    const message& message,
    const fielddescriptor* field,
    int index,
    string* output) const {

  google_dcheck(output) << "output specified is null";

  output->clear();
  io::stringoutputstream output_stream(output);
  textgenerator generator(&output_stream, initial_indent_level_);

  printfieldvalue(message, message.getreflection(), field, index, generator);
}

void textformat::printer::printfield(const message& message,
                                     const reflection* reflection,
                                     const fielddescriptor* field,
                                     textgenerator& generator) const {
  if (use_short_repeated_primitives_ &&
      field->is_repeated() &&
      field->cpp_type() != fielddescriptor::cpptype_string &&
      field->cpp_type() != fielddescriptor::cpptype_message) {
    printshortrepeatedfield(message, reflection, field, generator);
    return;
  }

  int count = 0;

  if (field->is_repeated()) {
    count = reflection->fieldsize(message, field);
  } else if (reflection->hasfield(message, field)) {
    count = 1;
  }

  for (int j = 0; j < count; ++j) {
    printfieldname(message, reflection, field, generator);

    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      if (single_line_mode_) {
        generator.print(" { ");
      } else {
        generator.print(" {\n");
        generator.indent();
      }
    } else {
      generator.print(": ");
    }

    // write the field value.
    int field_index = j;
    if (!field->is_repeated()) {
      field_index = -1;
    }

    printfieldvalue(message, reflection, field, field_index, generator);

    if (field->cpp_type() == fielddescriptor::cpptype_message) {
      if (single_line_mode_) {
        generator.print("} ");
      } else {
        generator.outdent();
        generator.print("}\n");
      }
    } else {
      if (single_line_mode_) {
        generator.print(" ");
      } else {
        generator.print("\n");
      }
    }
  }
}

void textformat::printer::printshortrepeatedfield(
    const message& message,
    const reflection* reflection,
    const fielddescriptor* field,
    textgenerator& generator) const {
  // print primitive repeated field in short form.
  printfieldname(message, reflection, field, generator);

  int size = reflection->fieldsize(message, field);
  generator.print(": [");
  for (int i = 0; i < size; i++) {
    if (i > 0) generator.print(", ");
    printfieldvalue(message, reflection, field, i, generator);
  }
  if (single_line_mode_) {
    generator.print("] ");
  } else {
    generator.print("]\n");
  }
}

void textformat::printer::printfieldname(const message& message,
                                         const reflection* reflection,
                                         const fielddescriptor* field,
                                         textgenerator& generator) const {
  if (field->is_extension()) {
    generator.print("[");
    // we special-case messageset elements for compatibility with proto1.
    if (field->containing_type()->options().message_set_wire_format()
        && field->type() == fielddescriptor::type_message
        && field->is_optional()
        && field->extension_scope() == field->message_type()) {
      generator.print(field->message_type()->full_name());
    } else {
      generator.print(field->full_name());
    }
    generator.print("]");
  } else {
    if (field->type() == fielddescriptor::type_group) {
      // groups must be serialized with their original capitalization.
      generator.print(field->message_type()->name());
    } else {
      generator.print(field->name());
    }
  }
}

void textformat::printer::printfieldvalue(
    const message& message,
    const reflection* reflection,
    const fielddescriptor* field,
    int index,
    textgenerator& generator) const {
  google_dcheck(field->is_repeated() || (index == -1))
      << "index must be -1 for non-repeated fields";

  switch (field->cpp_type()) {
#define output_field(cpptype, method, to_string)                             \
      case fielddescriptor::cpptype_##cpptype:                               \
        generator.print(to_string(field->is_repeated() ?                     \
          reflection->getrepeated##method(message, field, index) :           \
          reflection->get##method(message, field)));                         \
        break;                                                               \

      output_field( int32,  int32, simpleitoa);
      output_field( int64,  int64, simpleitoa);
      output_field(uint32, uint32, simpleitoa);
      output_field(uint64, uint64, simpleitoa);
      output_field( float,  float, simpleftoa);
      output_field(double, double, simpledtoa);
#undef output_field

      case fielddescriptor::cpptype_string: {
        string scratch;
        const string& value = field->is_repeated() ?
            reflection->getrepeatedstringreference(
              message, field, index, &scratch) :
            reflection->getstringreference(message, field, &scratch);

        generator.print("\"");
        if (utf8_string_escaping_) {
          generator.print(strings::utf8safecescape(value));
        } else {
          generator.print(cescape(value));
        }
        generator.print("\"");

        break;
      }

      case fielddescriptor::cpptype_bool:
        if (field->is_repeated()) {
          generator.print(reflection->getrepeatedbool(message, field, index)
                          ? "true" : "false");
        } else {
          generator.print(reflection->getbool(message, field)
                          ? "true" : "false");
        }
        break;

      case fielddescriptor::cpptype_enum:
        generator.print(field->is_repeated() ?
          reflection->getrepeatedenum(message, field, index)->name() :
          reflection->getenum(message, field)->name());
        break;

      case fielddescriptor::cpptype_message:
        print(field->is_repeated() ?
                reflection->getrepeatedmessage(message, field, index) :
                reflection->getmessage(message, field),
              generator);
        break;
  }
}

/* static */ bool textformat::print(const message& message,
                                    io::zerocopyoutputstream* output) {
  return printer().print(message, output);
}

/* static */ bool textformat::printunknownfields(
    const unknownfieldset& unknown_fields,
    io::zerocopyoutputstream* output) {
  return printer().printunknownfields(unknown_fields, output);
}

/* static */ bool textformat::printtostring(
    const message& message, string* output) {
  return printer().printtostring(message, output);
}

/* static */ bool textformat::printunknownfieldstostring(
    const unknownfieldset& unknown_fields, string* output) {
  return printer().printunknownfieldstostring(unknown_fields, output);
}

/* static */ void textformat::printfieldvaluetostring(
    const message& message,
    const fielddescriptor* field,
    int index,
    string* output) {
  return printer().printfieldvaluetostring(message, field, index, output);
}

/* static */ bool textformat::parsefieldvaluefromstring(
    const string& input,
    const fielddescriptor* field,
    message* message) {
  return parser().parsefieldvaluefromstring(input, field, message);
}

// prints an integer as hex with a fixed number of digits dependent on the
// integer type.
template<typename inttype>
static string paddedhex(inttype value) {
  string result;
  result.reserve(sizeof(value) * 2);
  for (int i = sizeof(value) * 2 - 1; i >= 0; i--) {
    result.push_back(int_to_hex_digit(value >> (i*4) & 0x0f));
  }
  return result;
}

void textformat::printer::printunknownfields(
    const unknownfieldset& unknown_fields, textgenerator& generator) const {
  for (int i = 0; i < unknown_fields.field_count(); i++) {
    const unknownfield& field = unknown_fields.field(i);
    string field_number = simpleitoa(field.number());

    switch (field.type()) {
      case unknownfield::type_varint:
        generator.print(field_number);
        generator.print(": ");
        generator.print(simpleitoa(field.varint()));
        if (single_line_mode_) {
          generator.print(" ");
        } else {
          generator.print("\n");
        }
        break;
      case unknownfield::type_fixed32: {
        generator.print(field_number);
        generator.print(": 0x");
        char buffer[kfasttobuffersize];
        generator.print(fasthex32tobuffer(field.fixed32(), buffer));
        if (single_line_mode_) {
          generator.print(" ");
        } else {
          generator.print("\n");
        }
        break;
      }
      case unknownfield::type_fixed64: {
        generator.print(field_number);
        generator.print(": 0x");
        char buffer[kfasttobuffersize];
        generator.print(fasthex64tobuffer(field.fixed64(), buffer));
        if (single_line_mode_) {
          generator.print(" ");
        } else {
          generator.print("\n");
        }
        break;
      }
      case unknownfield::type_length_delimited: {
        generator.print(field_number);
        const string& value = field.length_delimited();
        unknownfieldset embedded_unknown_fields;
        if (!value.empty() && embedded_unknown_fields.parsefromstring(value)) {
          // this field is parseable as a message.
          // so it is probably an embedded message.
          if (single_line_mode_) {
            generator.print(" { ");
          } else {
            generator.print(" {\n");
            generator.indent();
          }
          printunknownfields(embedded_unknown_fields, generator);
          if (single_line_mode_) {
            generator.print("} ");
          } else {
            generator.outdent();
            generator.print("}\n");
          }
        } else {
          // this field is not parseable as a message.
          // so it is probably just a plain string.
          generator.print(": \"");
          generator.print(cescape(value));
          generator.print("\"");
          if (single_line_mode_) {
            generator.print(" ");
          } else {
            generator.print("\n");
          }
        }
        break;
      }
      case unknownfield::type_group:
        generator.print(field_number);
        if (single_line_mode_) {
          generator.print(" { ");
        } else {
          generator.print(" {\n");
          generator.indent();
        }
        printunknownfields(field.group(), generator);
        if (single_line_mode_) {
          generator.print("} ");
        } else {
          generator.outdent();
          generator.print("}\n");
        }
        break;
    }
  }
}

}  // namespace protobuf
}  // namespace google
