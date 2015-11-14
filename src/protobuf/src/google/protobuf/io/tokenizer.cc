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
// here we have a hand-written lexer.  at first you might ask yourself,
// "hand-written text processing?  is kenton crazy?!"  well, first of all,
// yes i am crazy, but that's beside the point.  there are actually reasons
// why i ended up writing this this way.
//
// the traditional approach to lexing is to use lex to generate a lexer for
// you.  unfortunately, lex's output is ridiculously ugly and difficult to
// integrate cleanly with c++ code, especially abstract code or code meant
// as a library.  better parser-generators exist but would add dependencies
// which most users won't already have, which we'd like to avoid.  (gnu flex
// has a c++ output option, but it's still ridiculously ugly, non-abstract,
// and not library-friendly.)
//
// the next approach that any good software engineer should look at is to
// use regular expressions.  and, indeed, i did.  i have code which
// implements this same class using regular expressions.  it's about 200
// lines shorter.  however:
// - rather than error messages telling you "this string has an invalid
//   escape sequence at line 5, column 45", you get error messages like
//   "parse error on line 5".  giving more precise errors requires adding
//   a lot of code that ends up basically as complex as the hand-coded
//   version anyway.
// - the regular expression to match a string literal looks like this:
//     kstring  = new re("(\"([^\"\\\\]|"              // non-escaped
//                       "\\\\[abfnrtv?\"'\\\\0-7]|"   // normal escape
//                       "\\\\x[0-9a-fa-f])*\"|"       // hex escape
//                       "\'([^\'\\\\]|"        // also support single-quotes.
//                       "\\\\[abfnrtv?\"'\\\\0-7]|"
//                       "\\\\x[0-9a-fa-f])*\')");
//   verifying the correctness of this line noise is actually harder than
//   verifying the correctness of consumestring(), defined below.  i'm not
//   even confident that the above is correct, after staring at it for some
//   time.
// - pcre is fast, but there's still more overhead involved than the code
//   below.
// - sadly, regular expressions are not part of the c standard library, so
//   using them would require depending on some other library.  for the
//   open source release, this could be really annoying.  nobody likes
//   downloading one piece of software just to find that they need to
//   download something else to make it work, and in all likelihood
//   people downloading protocol buffers will already be doing so just
//   to make something else work.  we could include a copy of pcre with
//   our code, but that obligates us to keep it up-to-date and just seems
//   like a big waste just to save 200 lines of code.
//
// on a similar but unrelated note, i'm even scared to use ctype.h.
// apparently functions like isalpha() are locale-dependent.  so, if we used
// that, then if this code is being called from some program that doesn't
// have its locale set to "c", it would behave strangely.  we can't just set
// the locale to "c" ourselves since we might break the calling program that
// way, particularly if it is multi-threaded.  wtf?  someone please let me
// (kenton) know if i'm missing something here...
//
// i'd love to hear about other alternatives, though, as this code isn't
// exactly pretty.

#include <google/protobuf/io/tokenizer.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace io {
namespace {

// as mentioned above, i don't trust ctype.h due to the presence of "locales".
// so, i have written replacement functions here.  someone please smack me if
// this is a bad idea or if there is some way around this.
//
// these "character classes" are designed to be used in template methods.
// for instance, tokenizer::consumezeroormore<whitespace>() will eat
// whitespace.

// note:  no class is allowed to contain '\0', since this is used to mark end-
//   of-input and is handled specially.

#define character_class(name, expression)      \
  class name {                                 \
   public:                                     \
    static inline bool inclass(char c) {       \
      return expression;                       \
    }                                          \
  }

character_class(whitespace, c == ' ' || c == '\n' || c == '\t' ||
                            c == '\r' || c == '\v' || c == '\f');
character_class(whitespacenonewline, c == ' ' || c == '\t' ||
                                     c == '\r' || c == '\v' || c == '\f');

character_class(unprintable, c < ' ' && c > '\0');

character_class(digit, '0' <= c && c <= '9');
character_class(octaldigit, '0' <= c && c <= '7');
character_class(hexdigit, ('0' <= c && c <= '9') ||
                          ('a' <= c && c <= 'f') ||
                          ('a' <= c && c <= 'f'));

character_class(letter, ('a' <= c && c <= 'z') ||
                        ('a' <= c && c <= 'z') ||
                        (c == '_'));

character_class(alphanumeric, ('a' <= c && c <= 'z') ||
                              ('a' <= c && c <= 'z') ||
                              ('0' <= c && c <= '9') ||
                              (c == '_'));

character_class(escape, c == 'a' || c == 'b' || c == 'f' || c == 'n' ||
                        c == 'r' || c == 't' || c == 'v' || c == '\\' ||
                        c == '?' || c == '\'' || c == '\"');

#undef character_class

// given a char, interpret it as a numeric digit and return its value.
// this supports any number base up to 36.
inline int digitvalue(char digit) {
  if ('0' <= digit && digit <= '9') return digit - '0';
  if ('a' <= digit && digit <= 'z') return digit - 'a' + 10;
  if ('a' <= digit && digit <= 'z') return digit - 'a' + 10;
  return -1;
}

// inline because it's only used in one place.
inline char translateescape(char c) {
  switch (c) {
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'n':  return '\n';
    case 'r':  return '\r';
    case 't':  return '\t';
    case 'v':  return '\v';
    case '\\': return '\\';
    case '?':  return '\?';    // trigraphs = :(
    case '\'': return '\'';
    case '"':  return '\"';

    // we expect escape sequences to have been validated separately.
    default:   return '?';
  }
}

}  // anonymous namespace

errorcollector::~errorcollector() {}

// ===================================================================

tokenizer::tokenizer(zerocopyinputstream* input,
                     errorcollector* error_collector)
  : input_(input),
    error_collector_(error_collector),
    buffer_(null),
    buffer_size_(0),
    buffer_pos_(0),
    read_error_(false),
    line_(0),
    column_(0),
    record_target_(null),
    record_start_(-1),
    allow_f_after_float_(false),
    comment_style_(cpp_comment_style) {

  current_.line = 0;
  current_.column = 0;
  current_.end_column = 0;
  current_.type = type_start;

  refresh();
}

tokenizer::~tokenizer() {
  // if we had any buffer left unread, return it to the underlying stream
  // so that someone else can read it.
  if (buffer_size_ > buffer_pos_) {
    input_->backup(buffer_size_ - buffer_pos_);
  }
}

// -------------------------------------------------------------------
// internal helpers.

void tokenizer::nextchar() {
  // update our line and column counters based on the character being
  // consumed.
  if (current_char_ == '\n') {
    ++line_;
    column_ = 0;
  } else if (current_char_ == '\t') {
    column_ += ktabwidth - column_ % ktabwidth;
  } else {
    ++column_;
  }

  // advance to the next character.
  ++buffer_pos_;
  if (buffer_pos_ < buffer_size_) {
    current_char_ = buffer_[buffer_pos_];
  } else {
    refresh();
  }
}

void tokenizer::refresh() {
  if (read_error_) {
    current_char_ = '\0';
    return;
  }

  // if we're in a token, append the rest of the buffer to it.
  if (record_target_ != null && record_start_ < buffer_size_) {
    record_target_->append(buffer_ + record_start_, buffer_size_ - record_start_);
    record_start_ = 0;
  }

  const void* data = null;
  buffer_ = null;
  buffer_pos_ = 0;
  do {
    if (!input_->next(&data, &buffer_size_)) {
      // end of stream (or read error)
      buffer_size_ = 0;
      read_error_ = true;
      current_char_ = '\0';
      return;
    }
  } while (buffer_size_ == 0);

  buffer_ = static_cast<const char*>(data);

  current_char_ = buffer_[0];
}

inline void tokenizer::recordto(string* target) {
  record_target_ = target;
  record_start_ = buffer_pos_;
}

inline void tokenizer::stoprecording() {
  // note:  the if() is necessary because some stl implementations crash when
  //   you call string::append(null, 0), presumably because they are trying to
  //   be helpful by detecting the null pointer, even though there's nothing
  //   wrong with reading zero bytes from null.
  if (buffer_pos_ != record_start_) {
    record_target_->append(buffer_ + record_start_, buffer_pos_ - record_start_);
  }
  record_target_ = null;
  record_start_ = -1;
}

inline void tokenizer::starttoken() {
  current_.type = type_start;    // just for the sake of initializing it.
  current_.text.clear();
  current_.line = line_;
  current_.column = column_;
  recordto(&current_.text);
}

inline void tokenizer::endtoken() {
  stoprecording();
  current_.end_column = column_;
}

// -------------------------------------------------------------------
// helper methods that consume characters.

template<typename characterclass>
inline bool tokenizer::lookingat() {
  return characterclass::inclass(current_char_);
}

template<typename characterclass>
inline bool tokenizer::tryconsumeone() {
  if (characterclass::inclass(current_char_)) {
    nextchar();
    return true;
  } else {
    return false;
  }
}

inline bool tokenizer::tryconsume(char c) {
  if (current_char_ == c) {
    nextchar();
    return true;
  } else {
    return false;
  }
}

template<typename characterclass>
inline void tokenizer::consumezeroormore() {
  while (characterclass::inclass(current_char_)) {
    nextchar();
  }
}

template<typename characterclass>
inline void tokenizer::consumeoneormore(const char* error) {
  if (!characterclass::inclass(current_char_)) {
    adderror(error);
  } else {
    do {
      nextchar();
    } while (characterclass::inclass(current_char_));
  }
}

// -------------------------------------------------------------------
// methods that read whole patterns matching certain kinds of tokens
// or comments.

void tokenizer::consumestring(char delimiter) {
  while (true) {
    switch (current_char_) {
      case '\0':
      case '\n': {
        adderror("string literals cannot cross line boundaries.");
        return;
      }

      case '\\': {
        // an escape sequence.
        nextchar();
        if (tryconsumeone<escape>()) {
          // valid escape sequence.
        } else if (tryconsumeone<octaldigit>()) {
          // possibly followed by two more octal digits, but these will
          // just be consumed by the main loop anyway so we don't need
          // to do so explicitly here.
        } else if (tryconsume('x') || tryconsume('x')) {
          if (!tryconsumeone<hexdigit>()) {
            adderror("expected hex digits for escape sequence.");
          }
          // possibly followed by another hex digit, but again we don't care.
        } else if (tryconsume('u')) {
          if (!tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>()) {
            adderror("expected four hex digits for \\u escape sequence.");
          }
        } else if (tryconsume('u')) {
          // we expect 8 hex digits; but only the range up to 0x10ffff is
          // legal.
          if (!tryconsume('0') ||
              !tryconsume('0') ||
              !(tryconsume('0') || tryconsume('1')) ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>() ||
              !tryconsumeone<hexdigit>()) {
            adderror("expected eight hex digits up to 10ffff for \\u escape "
                     "sequence");
          }
        } else {
          adderror("invalid escape sequence in string literal.");
        }
        break;
      }

      default: {
        if (current_char_ == delimiter) {
          nextchar();
          return;
        }
        nextchar();
        break;
      }
    }
  }
}

tokenizer::tokentype tokenizer::consumenumber(bool started_with_zero,
                                              bool started_with_dot) {
  bool is_float = false;

  if (started_with_zero && (tryconsume('x') || tryconsume('x'))) {
    // a hex number (started with "0x").
    consumeoneormore<hexdigit>("\"0x\" must be followed by hex digits.");

  } else if (started_with_zero && lookingat<digit>()) {
    // an octal number (had a leading zero).
    consumezeroormore<octaldigit>();
    if (lookingat<digit>()) {
      adderror("numbers starting with leading zero must be in octal.");
      consumezeroormore<digit>();
    }

  } else {
    // a decimal number.
    if (started_with_dot) {
      is_float = true;
      consumezeroormore<digit>();
    } else {
      consumezeroormore<digit>();

      if (tryconsume('.')) {
        is_float = true;
        consumezeroormore<digit>();
      }
    }

    if (tryconsume('e') || tryconsume('e')) {
      is_float = true;
      tryconsume('-') || tryconsume('+');
      consumeoneormore<digit>("\"e\" must be followed by exponent.");
    }

    if (allow_f_after_float_ && (tryconsume('f') || tryconsume('f'))) {
      is_float = true;
    }
  }

  if (lookingat<letter>()) {
    adderror("need space between number and identifier.");
  } else if (current_char_ == '.') {
    if (is_float) {
      adderror(
        "already saw decimal point or exponent; can't have another one.");
    } else {
      adderror("hex and octal numbers must be integers.");
    }
  }

  return is_float ? type_float : type_integer;
}

void tokenizer::consumelinecomment(string* content) {
  if (content != null) recordto(content);

  while (current_char_ != '\0' && current_char_ != '\n') {
    nextchar();
  }
  tryconsume('\n');

  if (content != null) stoprecording();
}

void tokenizer::consumeblockcomment(string* content) {
  int start_line = line_;
  int start_column = column_ - 2;

  if (content != null) recordto(content);

  while (true) {
    while (current_char_ != '\0' &&
           current_char_ != '*' &&
           current_char_ != '/' &&
           current_char_ != '\n') {
      nextchar();
    }

    if (tryconsume('\n')) {
      if (content != null) stoprecording();

      // consume leading whitespace and asterisk;
      consumezeroormore<whitespacenonewline>();
      if (tryconsume('*')) {
        if (tryconsume('/')) {
          // end of comment.
          break;
        }
      }

      if (content != null) recordto(content);
    } else if (tryconsume('*') && tryconsume('/')) {
      // end of comment.
      if (content != null) {
        stoprecording();
        // strip trailing "*/".
        content->erase(content->size() - 2);
      }
      break;
    } else if (tryconsume('/') && current_char_ == '*') {
      // note:  we didn't consume the '*' because if there is a '/' after it
      //   we want to interpret that as the end of the comment.
      adderror(
        "\"/*\" inside block comment.  block comments cannot be nested.");
    } else if (current_char_ == '\0') {
      adderror("end-of-file inside block comment.");
      error_collector_->adderror(
        start_line, start_column, "  comment started here.");
      if (content != null) stoprecording();
      break;
    }
  }
}

tokenizer::nextcommentstatus tokenizer::tryconsumecommentstart() {
  if (comment_style_ == cpp_comment_style && tryconsume('/')) {
    if (tryconsume('/')) {
      return line_comment;
    } else if (tryconsume('*')) {
      return block_comment;
    } else {
      // oops, it was just a slash.  return it.
      current_.type = type_symbol;
      current_.text = "/";
      current_.line = line_;
      current_.column = column_ - 1;
      current_.end_column = column_;
      return slash_not_comment;
    }
  } else if (comment_style_ == sh_comment_style && tryconsume('#')) {
    return line_comment;
  } else {
    return no_comment;
  }
}

// -------------------------------------------------------------------

bool tokenizer::next() {
  previous_ = current_;

  while (!read_error_) {
    consumezeroormore<whitespace>();

    switch (tryconsumecommentstart()) {
      case line_comment:
        consumelinecomment(null);
        continue;
      case block_comment:
        consumeblockcomment(null);
        continue;
      case slash_not_comment:
        return true;
      case no_comment:
        break;
    }

    // check for eof before continuing.
    if (read_error_) break;

    if (lookingat<unprintable>() || current_char_ == '\0') {
      adderror("invalid control characters encountered in text.");
      nextchar();
      // skip more unprintable characters, too.  but, remember that '\0' is
      // also what current_char_ is set to after eof / read error.  we have
      // to be careful not to go into an infinite loop of trying to consume
      // it, so make sure to check read_error_ explicitly before consuming
      // '\0'.
      while (tryconsumeone<unprintable>() ||
             (!read_error_ && tryconsume('\0'))) {
        // ignore.
      }

    } else {
      // reading some sort of token.
      starttoken();

      if (tryconsumeone<letter>()) {
        consumezeroormore<alphanumeric>();
        current_.type = type_identifier;
      } else if (tryconsume('0')) {
        current_.type = consumenumber(true, false);
      } else if (tryconsume('.')) {
        // this could be the beginning of a floating-point number, or it could
        // just be a '.' symbol.

        if (tryconsumeone<digit>()) {
          // it's a floating-point number.
          if (previous_.type == type_identifier &&
              current_.line == previous_.line &&
              current_.column == previous_.end_column) {
            // we don't accept syntax like "blah.123".
            error_collector_->adderror(line_, column_ - 2,
              "need space between identifier and decimal point.");
          }
          current_.type = consumenumber(false, true);
        } else {
          current_.type = type_symbol;
        }
      } else if (tryconsumeone<digit>()) {
        current_.type = consumenumber(false, false);
      } else if (tryconsume('\"')) {
        consumestring('\"');
        current_.type = type_string;
      } else if (tryconsume('\'')) {
        consumestring('\'');
        current_.type = type_string;
      } else {
        nextchar();
        current_.type = type_symbol;
      }

      endtoken();
      return true;
    }
  }

  // eof
  current_.type = type_end;
  current_.text.clear();
  current_.line = line_;
  current_.column = column_;
  current_.end_column = column_;
  return false;
}

namespace {

// helper class for collecting comments and putting them in the right places.
//
// this basically just buffers the most recent comment until it can be decided
// exactly where that comment should be placed.  when flush() is called, the
// current comment goes into either prev_trailing_comments or detached_comments.
// when the commentcollector is destroyed, the last buffered comment goes into
// next_leading_comments.
class commentcollector {
 public:
  commentcollector(string* prev_trailing_comments,
                   vector<string>* detached_comments,
                   string* next_leading_comments)
      : prev_trailing_comments_(prev_trailing_comments),
        detached_comments_(detached_comments),
        next_leading_comments_(next_leading_comments),
        has_comment_(false),
        is_line_comment_(false),
        can_attach_to_prev_(true) {
    if (prev_trailing_comments != null) prev_trailing_comments->clear();
    if (detached_comments != null) detached_comments->clear();
    if (next_leading_comments != null) next_leading_comments->clear();
  }

  ~commentcollector() {
    // whatever is in the buffer is a leading comment.
    if (next_leading_comments_ != null && has_comment_) {
      comment_buffer_.swap(*next_leading_comments_);
    }
  }

  // about to read a line comment.  get the comment buffer pointer in order to
  // read into it.
  string* getbufferforlinecomment() {
    // we want to combine with previous line comments, but not block comments.
    if (has_comment_ && !is_line_comment_) {
      flush();
    }
    has_comment_ = true;
    is_line_comment_ = true;
    return &comment_buffer_;
  }

  // about to read a block comment.  get the comment buffer pointer in order to
  // read into it.
  string* getbufferforblockcomment() {
    if (has_comment_) {
      flush();
    }
    has_comment_ = true;
    is_line_comment_ = false;
    return &comment_buffer_;
  }

  void clearbuffer() {
    comment_buffer_.clear();
    has_comment_ = false;
  }

  // called once we know that the comment buffer is complete and is *not*
  // connected to the next token.
  void flush() {
    if (has_comment_) {
      if (can_attach_to_prev_) {
        if (prev_trailing_comments_ != null) {
          prev_trailing_comments_->append(comment_buffer_);
        }
        can_attach_to_prev_ = false;
      } else {
        if (detached_comments_ != null) {
          detached_comments_->push_back(comment_buffer_);
        }
      }
      clearbuffer();
    }
  }

  void detachfromprev() {
    can_attach_to_prev_ = false;
  }

 private:
  string* prev_trailing_comments_;
  vector<string>* detached_comments_;
  string* next_leading_comments_;

  string comment_buffer_;

  // true if any comments were read into comment_buffer_.  this can be true even
  // if comment_buffer_ is empty, namely if the comment was "/**/".
  bool has_comment_;

  // is the comment in the comment buffer a line comment?
  bool is_line_comment_;

  // is it still possible that we could be reading a comment attached to the
  // previous token?
  bool can_attach_to_prev_;
};

} // namespace

bool tokenizer::nextwithcomments(string* prev_trailing_comments,
                                 vector<string>* detached_comments,
                                 string* next_leading_comments) {
  commentcollector collector(prev_trailing_comments, detached_comments,
                             next_leading_comments);

  if (current_.type == type_start) {
    collector.detachfromprev();
  } else {
    // a comment appearing on the same line must be attached to the previous
    // declaration.
    consumezeroormore<whitespacenonewline>();
    switch (tryconsumecommentstart()) {
      case line_comment:
        consumelinecomment(collector.getbufferforlinecomment());

        // don't allow comments on subsequent lines to be attached to a trailing
        // comment.
        collector.flush();
        break;
      case block_comment:
        consumeblockcomment(collector.getbufferforblockcomment());

        consumezeroormore<whitespacenonewline>();
        if (!tryconsume('\n')) {
          // oops, the next token is on the same line.  if we recorded a comment
          // we really have no idea which token it should be attached to.
          collector.clearbuffer();
          return next();
        }

        // don't allow comments on subsequent lines to be attached to a trailing
        // comment.
        collector.flush();
        break;
      case slash_not_comment:
        return true;
      case no_comment:
        if (!tryconsume('\n')) {
          // the next token is on the same line.  there are no comments.
          return next();
        }
        break;
    }
  }

  // ok, we are now on the line *after* the previous token.
  while (true) {
    consumezeroormore<whitespacenonewline>();

    switch (tryconsumecommentstart()) {
      case line_comment:
        consumelinecomment(collector.getbufferforlinecomment());
        break;
      case block_comment:
        consumeblockcomment(collector.getbufferforblockcomment());

        // consume the rest of the line so that we don't interpret it as a
        // blank line the next time around the loop.
        consumezeroormore<whitespacenonewline>();
        tryconsume('\n');
        break;
      case slash_not_comment:
        return true;
      case no_comment:
        if (tryconsume('\n')) {
          // completely blank line.
          collector.flush();
          collector.detachfromprev();
        } else {
          bool result = next();
          if (!result ||
              current_.text == "}" ||
              current_.text == "]" ||
              current_.text == ")") {
            // it looks like we're at the end of a scope.  in this case it
            // makes no sense to attach a comment to the following token.
            collector.flush();
          }
          return result;
        }
        break;
    }
  }
}

// -------------------------------------------------------------------
// token-parsing helpers.  remember that these don't need to report
// errors since any errors should already have been reported while
// tokenizing.  also, these can assume that whatever text they
// are given is text that the tokenizer actually parsed as a token
// of the given type.

bool tokenizer::parseinteger(const string& text, uint64 max_value,
                             uint64* output) {
  // sadly, we can't just use strtoul() since it is only 32-bit and strtoull()
  // is non-standard.  i hate the c standard library.  :(

//  return strtoull(text.c_str(), null, 0);

  const char* ptr = text.c_str();
  int base = 10;
  if (ptr[0] == '0') {
    if (ptr[1] == 'x' || ptr[1] == 'x') {
      // this is hex.
      base = 16;
      ptr += 2;
    } else {
      // this is octal.
      base = 8;
    }
  }

  uint64 result = 0;
  for (; *ptr != '\0'; ptr++) {
    int digit = digitvalue(*ptr);
    google_log_if(dfatal, digit < 0 || digit >= base)
      << " tokenizer::parseinteger() passed text that could not have been"
         " tokenized as an integer: " << cescape(text);
    if (digit > max_value || result > (max_value - digit) / base) {
      // overflow.
      return false;
    }
    result = result * base + digit;
  }

  *output = result;
  return true;
}

double tokenizer::parsefloat(const string& text) {
  const char* start = text.c_str();
  char* end;
  double result = nolocalestrtod(start, &end);

  // "1e" is not a valid float, but if the tokenizer reads it, it will
  // report an error but still return it as a valid token.  we need to
  // accept anything the tokenizer could possibly return, error or not.
  if (*end == 'e' || *end == 'e') {
    ++end;
    if (*end == '-' || *end == '+') ++end;
  }

  // if the tokenizer had allow_f_after_float_ enabled, the float may be
  // suffixed with the letter 'f'.
  if (*end == 'f' || *end == 'f') {
    ++end;
  }

  google_log_if(dfatal, end - start != text.size() || *start == '-')
    << " tokenizer::parsefloat() passed text that could not have been"
       " tokenized as a float: " << cescape(text);
  return result;
}

// helper to append a unicode code point to a string as utf8, without bringing
// in any external dependencies.
static void appendutf8(uint32 code_point, string* output) {
  uint32 tmp = 0;
  int len = 0;
  if (code_point <= 0x7f) {
    tmp = code_point;
    len = 1;
  } else if (code_point <= 0x07ff) {
    tmp = 0x0000c080 |
        ((code_point & 0x07c0) << 2) |
        (code_point & 0x003f);
    len = 2;
  } else if (code_point <= 0xffff) {
    tmp = 0x00e08080 |
        ((code_point & 0xf000) << 4) |
        ((code_point & 0x0fc0) << 2) |
        (code_point & 0x003f);
    len = 3;
  } else if (code_point <= 0x1fffff) {
    tmp = 0xf0808080 |
        ((code_point & 0x1c0000) << 6) |
        ((code_point & 0x03f000) << 4) |
        ((code_point & 0x000fc0) << 2) |
        (code_point & 0x003f);
    len = 4;
  } else {
    // utf-16 is only defined for code points up to 0x10ffff, and utf-8 is
    // normally only defined up to there as well.
    stringappendf(output, "\\u%08x", code_point);
    return;
  }
  tmp = ghtonl(tmp);
  output->append(reinterpret_cast<const char*>(&tmp) + sizeof(tmp) - len, len);
}

// try to read <len> hex digits from ptr, and stuff the numeric result into
// *result. returns true if that many digits were successfully consumed.
static bool readhexdigits(const char* ptr, int len, uint32* result) {
  *result = 0;
  if (len == 0) return false;
  for (const char* end = ptr + len; ptr < end; ++ptr) {
    if (*ptr == '\0') return false;
    *result = (*result << 4) + digitvalue(*ptr);
  }
  return true;
}

// handling utf-16 surrogate pairs. utf-16 encodes code points in the range
// 0x10000...0x10ffff as a pair of numbers, a head surrogate followed by a trail
// surrogate. these numbers are in a reserved range of unicode code points, so
// if we encounter such a pair we know how to parse it and convert it into a
// single code point.
static const uint32 kminheadsurrogate = 0xd800;
static const uint32 kmaxheadsurrogate = 0xdc00;
static const uint32 kmintrailsurrogate = 0xdc00;
static const uint32 kmaxtrailsurrogate = 0xe000;

static inline bool isheadsurrogate(uint32 code_point) {
  return (code_point >= kminheadsurrogate) && (code_point < kmaxheadsurrogate);
}

static inline bool istrailsurrogate(uint32 code_point) {
  return (code_point >= kmintrailsurrogate) &&
      (code_point < kmaxtrailsurrogate);
}

// combine a head and trail surrogate into a single unicode code point.
static uint32 assembleutf16(uint32 head_surrogate, uint32 trail_surrogate) {
  google_dcheck(isheadsurrogate(head_surrogate));
  google_dcheck(istrailsurrogate(trail_surrogate));
  return 0x10000 + (((head_surrogate - kminheadsurrogate) << 10) |
      (trail_surrogate - kmintrailsurrogate));
}

// convert the escape sequence parameter to a number of expected hex digits.
static inline int unicodelength(char key) {
  if (key == 'u') return 4;
  if (key == 'u') return 8;
  return 0;
}

// given a pointer to the 'u' or 'u' starting a unicode escape sequence, attempt
// to parse that sequence. on success, returns a pointer to the first char
// beyond that sequence, and fills in *code_point. on failure, returns ptr
// itself.
static const char* fetchunicodepoint(const char* ptr, uint32* code_point) {
  const char* p = ptr;
  // fetch the code point.
  const int len = unicodelength(*p++);
  if (!readhexdigits(p, len, code_point))
    return ptr;
  p += len;

  // check if the code point we read is a "head surrogate." if so, then we
  // expect it to be immediately followed by another code point which is a valid
  // "trail surrogate," and together they form a utf-16 pair which decodes into
  // a single unicode point. trail surrogates may only use \u, not \u.
  if (isheadsurrogate(*code_point) && *p == '\\' && *(p + 1) == 'u') {
    uint32 trail_surrogate;
    if (readhexdigits(p + 2, 4, &trail_surrogate) &&
        istrailsurrogate(trail_surrogate)) {
      *code_point = assembleutf16(*code_point, trail_surrogate);
      p += 6;
    }
    // if this failed, then we just emit the head surrogate as a code point.
    // it's bogus, but so is the string.
  }

  return p;
}

// the text string must begin and end with single or double quote
// characters.
void tokenizer::parsestringappend(const string& text, string* output) {
  // reminder: text[0] is always a quote character.  (if text is
  // empty, it's invalid, so we'll just return).
  const size_t text_size = text.size();
  if (text_size == 0) {
    google_log(dfatal)
      << " tokenizer::parsestringappend() passed text that could not"
         " have been tokenized as a string: " << cescape(text);
    return;
  }

  // reserve room for new string. the branch is necessary because if
  // there is already space available the reserve() call might
  // downsize the output.
  const size_t new_len = text_size + output->size();
  if (new_len > output->capacity()) {
    output->reserve(new_len);
  }

  // loop through the string copying characters to "output" and
  // interpreting escape sequences.  note that any invalid escape
  // sequences or other errors were already reported while tokenizing.
  // in this case we do not need to produce valid results.
  for (const char* ptr = text.c_str() + 1; *ptr != '\0'; ptr++) {
    if (*ptr == '\\' && ptr[1] != '\0') {
      // an escape sequence.
      ++ptr;

      if (octaldigit::inclass(*ptr)) {
        // an octal escape.  may one, two, or three digits.
        int code = digitvalue(*ptr);
        if (octaldigit::inclass(ptr[1])) {
          ++ptr;
          code = code * 8 + digitvalue(*ptr);
        }
        if (octaldigit::inclass(ptr[1])) {
          ++ptr;
          code = code * 8 + digitvalue(*ptr);
        }
        output->push_back(static_cast<char>(code));

      } else if (*ptr == 'x') {
        // a hex escape.  may zero, one, or two digits.  (the zero case
        // will have been caught as an error earlier.)
        int code = 0;
        if (hexdigit::inclass(ptr[1])) {
          ++ptr;
          code = digitvalue(*ptr);
        }
        if (hexdigit::inclass(ptr[1])) {
          ++ptr;
          code = code * 16 + digitvalue(*ptr);
        }
        output->push_back(static_cast<char>(code));

      } else if (*ptr == 'u' || *ptr == 'u') {
        uint32 unicode;
        const char* end = fetchunicodepoint(ptr, &unicode);
        if (end == ptr) {
          // failure: just dump out what we saw, don't try to parse it.
          output->push_back(*ptr);
        } else {
          appendutf8(unicode, output);
          ptr = end - 1;  // because we're about to ++ptr.
        }
      } else {
        // some other escape code.
        output->push_back(translateescape(*ptr));
      }

    } else if (*ptr == text[0] && ptr[1] == '\0') {
      // ignore final quote matching the starting quote.
    } else {
      output->push_back(*ptr);
    }
  }
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
