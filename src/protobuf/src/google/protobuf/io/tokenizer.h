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
// class for parsing tokenized text from a zerocopyinputstream.

#ifndef google_protobuf_io_tokenizer_h__
#define google_protobuf_io_tokenizer_h__

#include <string>
#include <vector>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

class zerocopyinputstream;     // zero_copy_stream.h

// defined in this file.
class errorcollector;
class tokenizer;

// abstract interface for an object which collects the errors that occur
// during parsing.  a typical implementation might simply print the errors
// to stdout.
class libprotobuf_export errorcollector {
 public:
  inline errorcollector() {}
  virtual ~errorcollector();

  // indicates that there was an error in the input at the given line and
  // column numbers.  the numbers are zero-based, so you may want to add
  // 1 to each before printing them.
  virtual void adderror(int line, int column, const string& message) = 0;

  // indicates that there was a warning in the input at the given line and
  // column numbers.  the numbers are zero-based, so you may want to add
  // 1 to each before printing them.
  virtual void addwarning(int line, int column, const string& message) { }

 private:
  google_disallow_evil_constructors(errorcollector);
};

// this class converts a stream of raw text into a stream of tokens for
// the protocol definition parser to parse.  the tokens recognized are
// similar to those that make up the c language; see the tokentype enum for
// precise descriptions.  whitespace and comments are skipped.  by default,
// c- and c++-style comments are recognized, but other styles can be used by
// calling set_comment_style().
class libprotobuf_export tokenizer {
 public:
  // construct a tokenizer that reads and tokenizes text from the given
  // input stream and writes errors to the given error_collector.
  // the caller keeps ownership of input and error_collector.
  tokenizer(zerocopyinputstream* input, errorcollector* error_collector);
  ~tokenizer();

  enum tokentype {
    type_start,       // next() has not yet been called.
    type_end,         // end of input reached.  "text" is empty.

    type_identifier,  // a sequence of letters, digits, and underscores, not
                      // starting with a digit.  it is an error for a number
                      // to be followed by an identifier with no space in
                      // between.
    type_integer,     // a sequence of digits representing an integer.  normally
                      // the digits are decimal, but a prefix of "0x" indicates
                      // a hex number and a leading zero indicates octal, just
                      // like with c numeric literals.  a leading negative sign
                      // is not included in the token; it's up to the parser to
                      // interpret the unary minus operator on its own.
    type_float,       // a floating point literal, with a fractional part and/or
                      // an exponent.  always in decimal.  again, never
                      // negative.
    type_string,      // a quoted sequence of escaped characters.  either single
                      // or double quotes can be used, but they must match.
                      // a string literal cannot cross a line break.
    type_symbol,      // any other printable character, like '!' or '+'.
                      // symbols are always a single character, so "!+$%" is
                      // four tokens.
  };

  // structure representing a token read from the token stream.
  struct token {
    tokentype type;
    string text;       // the exact text of the token as it appeared in
                       // the input.  e.g. tokens of type_string will still
                       // be escaped and in quotes.

    // "line" and "column" specify the position of the first character of
    // the token within the input stream.  they are zero-based.
    int line;
    int column;
    int end_column;
  };

  // get the current token.  this is updated when next() is called.  before
  // the first call to next(), current() has type type_start and no contents.
  const token& current();

  // return the previous token -- i.e. what current() returned before the
  // previous call to next().
  const token& previous();

  // advance to the next token.  returns false if the end of the input is
  // reached.
  bool next();

  // like next(), but also collects comments which appear between the previous
  // and next tokens.
  //
  // comments which appear to be attached to the previous token are stored
  // in *prev_tailing_comments.  comments which appear to be attached to the
  // next token are stored in *next_leading_comments.  comments appearing in
  // between which do not appear to be attached to either will be added to
  // detached_comments.  any of these parameters can be null to simply discard
  // the comments.
  //
  // a series of line comments appearing on consecutive lines, with no other
  // tokens appearing on those lines, will be treated as a single comment.
  //
  // only the comment content is returned; comment markers (e.g. //) are
  // stripped out.  for block comments, leading whitespace and an asterisk will
  // be stripped from the beginning of each line other than the first.  newlines
  // are included in the output.
  //
  // examples:
  //
  //   optional int32 foo = 1;  // comment attached to foo.
  //   // comment attached to bar.
  //   optional int32 bar = 2;
  //
  //   optional string baz = 3;
  //   // comment attached to baz.
  //   // another line attached to baz.
  //
  //   // comment attached to qux.
  //   //
  //   // another line attached to qux.
  //   optional double qux = 4;
  //
  //   // detached comment.  this is not attached to qux or corge
  //   // because there are blank lines separating it from both.
  //
  //   optional string corge = 5;
  //   /* block comment attached
  //    * to corge.  leading asterisks
  //    * will be removed. */
  //   /* block comment attached to
  //    * grault. */
  //   optional int32 grault = 6;
  bool nextwithcomments(string* prev_trailing_comments,
                        vector<string>* detached_comments,
                        string* next_leading_comments);

  // parse helpers ---------------------------------------------------

  // parses a type_float token.  this never fails, so long as the text actually
  // comes from a type_float token parsed by tokenizer.  if it doesn't, the
  // result is undefined (possibly an assert failure).
  static double parsefloat(const string& text);

  // parses a type_string token.  this never fails, so long as the text actually
  // comes from a type_string token parsed by tokenizer.  if it doesn't, the
  // result is undefined (possibly an assert failure).
  static void parsestring(const string& text, string* output);

  // identical to parsestring, but appends to output.
  static void parsestringappend(const string& text, string* output);

  // parses a type_integer token.  returns false if the result would be
  // greater than max_value.  otherwise, returns true and sets *output to the
  // result.  if the text is not from a token of type type_integer originally
  // parsed by a tokenizer, the result is undefined (possibly an assert
  // failure).
  static bool parseinteger(const string& text, uint64 max_value,
                           uint64* output);

  // options ---------------------------------------------------------

  // set true to allow floats to be suffixed with the letter 'f'.  tokens
  // which would otherwise be integers but which have the 'f' suffix will be
  // forced to be interpreted as floats.  for all other purposes, the 'f' is
  // ignored.
  void set_allow_f_after_float(bool value) { allow_f_after_float_ = value; }

  // valid values for set_comment_style().
  enum commentstyle {
    // line comments begin with "//", block comments are delimited by "/*" and
    // "*/".
    cpp_comment_style,
    // line comments begin with "#".  no way to write block comments.
    sh_comment_style
  };

  // sets the comment style.
  void set_comment_style(commentstyle style) { comment_style_ = style; }

  // -----------------------------------------------------------------
 private:
  google_disallow_evil_constructors(tokenizer);

  token current_;           // returned by current().
  token previous_;          // returned by previous().

  zerocopyinputstream* input_;
  errorcollector* error_collector_;

  char current_char_;       // == buffer_[buffer_pos_], updated by nextchar().
  const char* buffer_;      // current buffer returned from input_.
  int buffer_size_;         // size of buffer_.
  int buffer_pos_;          // current position within the buffer.
  bool read_error_;         // did we previously encounter a read error?

  // line and column number of current_char_ within the whole input stream.
  int line_;
  int column_;

  // string to which text should be appended as we advance through it.
  // call recordto(&str) to start recording and stoprecording() to stop.
  // e.g. starttoken() calls recordto(&current_.text).  record_start_ is the
  // position within the current buffer where recording started.
  string* record_target_;
  int record_start_;

  // options.
  bool allow_f_after_float_;
  commentstyle comment_style_;

  // since we count columns we need to interpret tabs somehow.  we'll take
  // the standard 8-character definition for lack of any way to do better.
  static const int ktabwidth = 8;

  // -----------------------------------------------------------------
  // helper methods.

  // consume this character and advance to the next one.
  void nextchar();

  // read a new buffer from the input.
  void refresh();

  inline void recordto(string* target);
  inline void stoprecording();

  // called when the current character is the first character of a new
  // token (not including whitespace or comments).
  inline void starttoken();
  // called when the current character is the first character after the
  // end of the last token.  after this returns, current_.text will
  // contain all text consumed since starttoken() was called.
  inline void endtoken();

  // convenience method to add an error at the current line and column.
  void adderror(const string& message) {
    error_collector_->adderror(line_, column_, message);
  }

  // -----------------------------------------------------------------
  // the following four methods are used to consume tokens of specific
  // types.  they are actually used to consume all characters *after*
  // the first, since the calling function consumes the first character
  // in order to decide what kind of token is being read.

  // read and consume a string, ending when the given delimiter is
  // consumed.
  void consumestring(char delimiter);

  // read and consume a number, returning type_float or type_integer
  // depending on what was read.  this needs to know if the first
  // character was a zero in order to correctly recognize hex and octal
  // numbers.
  // it also needs to know if the first characted was a . to parse floating
  // point correctly.
  tokentype consumenumber(bool started_with_zero, bool started_with_dot);

  // consume the rest of a line.
  void consumelinecomment(string* content);
  // consume until "*/".
  void consumeblockcomment(string* content);

  enum nextcommentstatus {
    // started a line comment.
    line_comment,

    // started a block comment.
    block_comment,

    // consumed a slash, then realized it wasn't a comment.  current_ has
    // been filled in with a slash token.  the caller should return it.
    slash_not_comment,

    // we do not appear to be starting a comment here.
    no_comment
  };

  // if we're at the start of a new comment, consume it and return what kind
  // of comment it is.
  nextcommentstatus tryconsumecommentstart();

  // -----------------------------------------------------------------
  // these helper methods make the parsing code more readable.  the
  // "character classes" refered to are defined at the top of the .cc file.
  // basically it is a c++ class with one method:
  //   static bool inclass(char c);
  // the method returns true if c is a member of this "class", like "letter"
  // or "digit".

  // returns true if the current character is of the given character
  // class, but does not consume anything.
  template<typename characterclass>
  inline bool lookingat();

  // if the current character is in the given class, consume it and return
  // true.  otherwise return false.
  // e.g. tryconsumeone<letter>()
  template<typename characterclass>
  inline bool tryconsumeone();

  // like above, but try to consume the specific character indicated.
  inline bool tryconsume(char c);

  // consume zero or more of the given character class.
  template<typename characterclass>
  inline void consumezeroormore();

  // consume one or more of the given character class or log the given
  // error message.
  // e.g. consumeoneormore<digit>("expected digits.");
  template<typename characterclass>
  inline void consumeoneormore(const char* error);
};

// inline methods ====================================================
inline const tokenizer::token& tokenizer::current() {
  return current_;
}

inline const tokenizer::token& tokenizer::previous() {
  return previous_;
}

inline void tokenizer::parsestring(const string& text, string* output) {
  output->clear();
  parsestringappend(text, output);
}

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_tokenizer_h__
