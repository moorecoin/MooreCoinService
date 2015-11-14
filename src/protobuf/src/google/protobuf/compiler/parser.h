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
// implements parsing of .proto files to filedescriptorprotos.

#ifndef google_protobuf_compiler_parser_h__
#define google_protobuf_compiler_parser_h__

#include <map>
#include <string>
#include <utility>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/io/tokenizer.h>

namespace google {
namespace protobuf { class message; }

namespace protobuf {
namespace compiler {

// defined in this file.
class parser;
class sourcelocationtable;

// implements parsing of protocol definitions (such as .proto files).
//
// note that most users will be more interested in the importer class.
// parser is a lower-level class which simply converts a single .proto file
// to a filedescriptorproto.  it does not resolve import directives or perform
// many other kinds of validation needed to construct a complete
// filedescriptor.
class libprotobuf_export parser {
 public:
  parser();
  ~parser();

  // parse the entire input and construct a filedescriptorproto representing
  // it.  returns true if no errors occurred, false otherwise.
  bool parse(io::tokenizer* input, filedescriptorproto* file);

  // optional fetaures:

  // deprecated:  new code should use the sourcecodeinfo embedded in the
  //   filedescriptorproto.
  //
  // requests that locations of certain definitions be recorded to the given
  // sourcelocationtable while parsing.  this can be used to look up exact line
  // and column numbers for errors reported by descriptorpool during validation.
  // set to null (the default) to discard source location information.
  void recordsourcelocationsto(sourcelocationtable* location_table) {
    source_location_table_ = location_table;
  }

  // requests that errors be recorded to the given errorcollector while
  // parsing.  set to null (the default) to discard error messages.
  void recorderrorsto(io::errorcollector* error_collector) {
    error_collector_ = error_collector;
  }

  // returns the identifier used in the "syntax = " declaration, if one was
  // seen during the last call to parse(), or the empty string otherwise.
  const string& getsyntaxidentifier() { return syntax_identifier_; }

  // if set true, input files will be required to begin with a syntax
  // identifier.  otherwise, files may omit this.  if a syntax identifier
  // is provided, it must be 'syntax = "proto2";' and must appear at the
  // top of this file regardless of whether or not it was required.
  void setrequiresyntaxidentifier(bool value) {
    require_syntax_identifier_ = value;
  }

  // call setstopaftersyntaxidentifier(true) to tell the parser to stop
  // parsing as soon as it has seen the syntax identifier, or lack thereof.
  // this is useful for quickly identifying the syntax of the file without
  // parsing the whole thing.  if this is enabled, no error will be recorded
  // if the syntax identifier is something other than "proto2" (since
  // presumably the caller intends to deal with that), but other kinds of
  // errors (e.g. parse errors) will still be reported.  when this is enabled,
  // you may pass a null filedescriptorproto to parse().
  void setstopaftersyntaxidentifier(bool value) {
    stop_after_syntax_identifier_ = value;
  }

 private:
  class locationrecorder;

  // =================================================================
  // error recovery helpers

  // consume the rest of the current statement.  this consumes tokens
  // until it sees one of:
  //   ';'  consumes the token and returns.
  //   '{'  consumes the brace then calls skiprestofblock().
  //   '}'  returns without consuming.
  //   eof  returns (can't consume).
  // the parser often calls skipstatement() after encountering a syntax
  // error.  this allows it to go on parsing the following lines, allowing
  // it to report more than just one error in the file.
  void skipstatement();

  // consume the rest of the current block, including nested blocks,
  // ending after the closing '}' is encountered and consumed, or at eof.
  void skiprestofblock();

  // -----------------------------------------------------------------
  // single-token consuming helpers
  //
  // these make parsing code more readable.

  // true if the current token is type_end.
  inline bool atend();

  // true if the next token matches the given text.
  inline bool lookingat(const char* text);
  // true if the next token is of the given type.
  inline bool lookingattype(io::tokenizer::tokentype token_type);

  // if the next token exactly matches the text given, consume it and return
  // true.  otherwise, return false without logging an error.
  bool tryconsume(const char* text);

  // these attempt to read some kind of token from the input.  if successful,
  // they return true.  otherwise they return false and add the given error
  // to the error list.

  // consume a token with the exact text given.
  bool consume(const char* text, const char* error);
  // same as above, but automatically generates the error "expected \"text\".",
  // where "text" is the expected token text.
  bool consume(const char* text);
  // consume a token of type identifier and store its text in "output".
  bool consumeidentifier(string* output, const char* error);
  // consume an integer and store its value in "output".
  bool consumeinteger(int* output, const char* error);
  // consume a signed integer and store its value in "output".
  bool consumesignedinteger(int* output, const char* error);
  // consume a 64-bit integer and store its value in "output".  if the value
  // is greater than max_value, an error will be reported.
  bool consumeinteger64(uint64 max_value, uint64* output, const char* error);
  // consume a number and store its value in "output".  this will accept
  // tokens of either integer or float type.
  bool consumenumber(double* output, const char* error);
  // consume a string literal and store its (unescaped) value in "output".
  bool consumestring(string* output, const char* error);

  // consume a token representing the end of the statement.  comments between
  // this token and the next will be harvested for documentation.  the given
  // locationrecorder should refer to the declaration that was just parsed;
  // it will be populated with these comments.
  //
  // todo(kenton):  the locationrecorder is const because historically locations
  //   have been passed around by const reference, for no particularly good
  //   reason.  we should probably go through and change them all to mutable
  //   pointer to make this more intuitive.
  bool tryconsumeendofdeclaration(const char* text,
                                  const locationrecorder* location);
  bool consumeendofdeclaration(const char* text,
                               const locationrecorder* location);

  // -----------------------------------------------------------------
  // error logging helpers

  // invokes error_collector_->adderror(), if error_collector_ is not null.
  void adderror(int line, int column, const string& error);

  // invokes error_collector_->adderror() with the line and column number
  // of the current token.
  void adderror(const string& error);

  // records a location in the sourcecodeinfo.location table (see
  // descriptor.proto).  we use raii to ensure that the start and end locations
  // are recorded -- the constructor records the start location and the
  // destructor records the end location.  since the parser is
  // recursive-descent, this works out beautifully.
  class libprotobuf_export locationrecorder {
   public:
    // construct the file's "root" location.
    locationrecorder(parser* parser);

    // construct a location that represents a declaration nested within the
    // given parent.  e.g. a field's location is nested within the location
    // for a message type.  the parent's path will be copied, so you should
    // call addpath() only to add the path components leading from the parent
    // to the child (as opposed to leading from the root to the child).
    locationrecorder(const locationrecorder& parent);

    // convenience constructors that call addpath() one or two times.
    locationrecorder(const locationrecorder& parent, int path1);
    locationrecorder(const locationrecorder& parent, int path1, int path2);

    ~locationrecorder();

    // add a path component.  see sourcecodeinfo.location.path in
    // descriptor.proto.
    void addpath(int path_component);

    // by default the location is considered to start at the current token at
    // the time the locationrecorder is created.  startat() sets the start
    // location to the given token instead.
    void startat(const io::tokenizer::token& token);

    // by default the location is considered to end at the previous token at
    // the time the locationrecorder is destroyed.  endat() sets the end
    // location to the given token instead.
    void endat(const io::tokenizer::token& token);

    // records the start point of this location to the sourcelocationtable that
    // was passed to recordsourcelocationsto(), if any.  sourcelocationtable
    // is an older way of keeping track of source locations which is still
    // used in some places.
    void recordlegacylocation(const message* descriptor,
        descriptorpool::errorcollector::errorlocation location);

    // attaches leading and trailing comments to the location.  the two strings
    // will be swapped into place, so after this is called *leading and
    // *trailing will be empty.
    //
    // todo(kenton):  see comment on tryconsumeendofdeclaration(), above, for
    //   why this is const.
    void attachcomments(string* leading, string* trailing) const;

   private:
    parser* parser_;
    sourcecodeinfo::location* location_;

    void init(const locationrecorder& parent);
  };

  // =================================================================
  // parsers for various language constructs

  // parses the "syntax = \"proto2\";" line at the top of the file.  returns
  // false if it failed to parse or if the syntax identifier was not
  // recognized.
  bool parsesyntaxidentifier();

  // these methods parse various individual bits of code.  they return
  // false if they completely fail to parse the construct.  in this case,
  // it is probably necessary to skip the rest of the statement to recover.
  // however, if these methods return true, it does not mean that there
  // were no errors; only that there were no *syntax* errors.  for instance,
  // if a service method is defined using proper syntax but uses a primitive
  // type as its input or output, parsemethodfield() still returns true
  // and only reports the error by calling adderror().  in practice, this
  // makes logic much simpler for the caller.

  // parse a top-level message, enum, service, etc.
  bool parsetoplevelstatement(filedescriptorproto* file,
                              const locationrecorder& root_location);

  // parse various language high-level language construrcts.
  bool parsemessagedefinition(descriptorproto* message,
                              const locationrecorder& message_location);
  bool parseenumdefinition(enumdescriptorproto* enum_type,
                           const locationrecorder& enum_location);
  bool parseservicedefinition(servicedescriptorproto* service,
                              const locationrecorder& service_location);
  bool parsepackage(filedescriptorproto* file,
                    const locationrecorder& root_location);
  bool parseimport(repeatedptrfield<string>* dependency,
                   repeatedfield<int32>* public_dependency,
                   repeatedfield<int32>* weak_dependency,
                   const locationrecorder& root_location);
  bool parseoption(message* options,
                   const locationrecorder& options_location);

  // these methods parse the contents of a message, enum, or service type and
  // add them to the given object.  they consume the entire block including
  // the beginning and ending brace.
  bool parsemessageblock(descriptorproto* message,
                         const locationrecorder& message_location);
  bool parseenumblock(enumdescriptorproto* enum_type,
                      const locationrecorder& enum_location);
  bool parseserviceblock(servicedescriptorproto* service,
                         const locationrecorder& service_location);

  // parse one statement within a message, enum, or service block, inclunding
  // final semicolon.
  bool parsemessagestatement(descriptorproto* message,
                             const locationrecorder& message_location);
  bool parseenumstatement(enumdescriptorproto* message,
                          const locationrecorder& enum_location);
  bool parseservicestatement(servicedescriptorproto* message,
                             const locationrecorder& service_location);

  // parse a field of a message.  if the field is a group, its type will be
  // added to "messages".
  //
  // parent_location and location_field_number_for_nested_type are needed when
  // parsing groups -- we need to generate a nested message type within the
  // parent and record its location accordingly.  since the parent could be
  // either a filedescriptorproto or a descriptorproto, we must pass in the
  // correct field number to use.
  bool parsemessagefield(fielddescriptorproto* field,
                         repeatedptrfield<descriptorproto>* messages,
                         const locationrecorder& parent_location,
                         int location_field_number_for_nested_type,
                         const locationrecorder& field_location);

  // parse an "extensions" declaration.
  bool parseextensions(descriptorproto* message,
                       const locationrecorder& extensions_location);

  // parse an "extend" declaration.  (see also comments for
  // parsemessagefield().)
  bool parseextend(repeatedptrfield<fielddescriptorproto>* extensions,
                   repeatedptrfield<descriptorproto>* messages,
                   const locationrecorder& parent_location,
                   int location_field_number_for_nested_type,
                   const locationrecorder& extend_location);

  // parse a single enum value within an enum block.
  bool parseenumconstant(enumvaluedescriptorproto* enum_value,
                         const locationrecorder& enum_value_location);

  // parse enum constant options, i.e. the list in square brackets at the end
  // of the enum constant value definition.
  bool parseenumconstantoptions(enumvaluedescriptorproto* value,
                                const locationrecorder& enum_value_location);

  // parse a single method within a service definition.
  bool parseservicemethod(methoddescriptorproto* method,
                          const locationrecorder& method_location);


  // parse options of a single method or stream.
  bool parseoptions(const locationrecorder& parent_location,
                    const int optionsfieldnumber,
                    message* mutable_options);

  // parse "required", "optional", or "repeated" and fill in "label"
  // with the value.
  bool parselabel(fielddescriptorproto::label* label);

  // parse a type name and fill in "type" (if it is a primitive) or
  // "type_name" (if it is not) with the type parsed.
  bool parsetype(fielddescriptorproto::type* type,
                 string* type_name);
  // parse a user-defined type and fill in "type_name" with the name.
  // if a primitive type is named, it is treated as an error.
  bool parseuserdefinedtype(string* type_name);

  // parses field options, i.e. the stuff in square brackets at the end
  // of a field definition.  also parses default value.
  bool parsefieldoptions(fielddescriptorproto* field,
                         const locationrecorder& field_location);

  // parse the "default" option.  this needs special handling because its
  // type is the field's type.
  bool parsedefaultassignment(fielddescriptorproto* field,
                              const locationrecorder& field_location);

  enum optionstyle {
    option_assignment,  // just "name = value"
    option_statement    // "option name = value;"
  };

  // parse a single option name/value pair, e.g. "ctype = cord".  the name
  // identifies a field of the given message, and the value of that field
  // is set to the parsed value.
  bool parseoption(message* options,
                   const locationrecorder& options_location,
                   optionstyle style);

  // parses a single part of a multipart option name. a multipart name consists
  // of names separated by dots. each name is either an identifier or a series
  // of identifiers separated by dots and enclosed in parentheses. e.g.,
  // "foo.(bar.baz).qux".
  bool parseoptionnamepart(uninterpretedoption* uninterpreted_option,
                           const locationrecorder& part_location);

  // parses a string surrounded by balanced braces.  strips off the outer
  // braces and stores the enclosed string in *value.
  // e.g.,
  //     { foo }                     *value gets 'foo'
  //     { foo { bar: box } }        *value gets 'foo { bar: box }'
  //     {}                          *value gets ''
  //
  // requires: lookingat("{")
  // when finished successfully, we are looking at the first token past
  // the ending brace.
  bool parseuninterpretedblock(string* value);

  // =================================================================

  io::tokenizer* input_;
  io::errorcollector* error_collector_;
  sourcecodeinfo* source_code_info_;
  sourcelocationtable* source_location_table_;  // legacy
  bool had_errors_;
  bool require_syntax_identifier_;
  bool stop_after_syntax_identifier_;
  string syntax_identifier_;

  // leading doc comments for the next declaration.  these are not complete
  // yet; use consumeendofdeclaration() to get the complete comments.
  string upcoming_doc_comments_;

  google_disallow_evil_constructors(parser);
};

// a table mapping (descriptor, errorlocation) pairs -- as reported by
// descriptorpool when validating descriptors -- to line and column numbers
// within the original source code.
//
// this is semi-obsolete:  filedescriptorproto.source_code_info now contains
// far more complete information about source locations.  however, as of this
// writing you still need to use sourcelocationtable when integrating with
// descriptorpool.
class libprotobuf_export sourcelocationtable {
 public:
  sourcelocationtable();
  ~sourcelocationtable();

  // finds the precise location of the given error and fills in *line and
  // *column with the line and column numbers.  if not found, sets *line to
  // -1 and *column to 0 (since line = -1 is used to mean "error has no exact
  // location" in the errorcollector interface).  returns true if found, false
  // otherwise.
  bool find(const message* descriptor,
            descriptorpool::errorcollector::errorlocation location,
            int* line, int* column) const;

  // adds a location to the table.
  void add(const message* descriptor,
           descriptorpool::errorcollector::errorlocation location,
           int line, int column);

  // clears the contents of the table.
  void clear();

 private:
  typedef map<
    pair<const message*, descriptorpool::errorcollector::errorlocation>,
    pair<int, int> > locationmap;
  locationmap location_map_;
};

}  // namespace compiler
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_compiler_parser_h__
