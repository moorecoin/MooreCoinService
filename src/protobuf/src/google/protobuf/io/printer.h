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
// utility class for writing text to a zerocopyoutputstream.

#ifndef google_protobuf_io_printer_h__
#define google_protobuf_io_printer_h__

#include <string>
#include <map>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

class zerocopyoutputstream;     // zero_copy_stream.h

// this simple utility class assists in code generation.  it basically
// allows the caller to define a set of variables and then output some
// text with variable substitutions.  example usage:
//
//   printer printer(output, '$');
//   map<string, string> vars;
//   vars["name"] = "bob";
//   printer.print(vars, "my name is $name$.");
//
// the above writes "my name is bob." to the output stream.
//
// printer aggressively enforces correct usage, crashing (with assert failures)
// in the case of undefined variables in debug builds. this helps greatly in
// debugging code which uses it.
class libprotobuf_export printer {
 public:
  // create a printer that writes text to the given output stream.  use the
  // given character as the delimiter for variables.
  printer(zerocopyoutputstream* output, char variable_delimiter);
  ~printer();

  // print some text after applying variable substitutions.  if a particular
  // variable in the text is not defined, this will crash.  variables to be
  // substituted are identified by their names surrounded by delimiter
  // characters (as given to the constructor).  the variable bindings are
  // defined by the given map.
  void print(const map<string, string>& variables, const char* text);

  // like the first print(), except the substitutions are given as parameters.
  void print(const char* text);
  // like the first print(), except the substitutions are given as parameters.
  void print(const char* text, const char* variable, const string& value);
  // like the first print(), except the substitutions are given as parameters.
  void print(const char* text, const char* variable1, const string& value1,
                               const char* variable2, const string& value2);
  // like the first print(), except the substitutions are given as parameters.
  void print(const char* text, const char* variable1, const string& value1,
                               const char* variable2, const string& value2,
                               const char* variable3, const string& value3);
  // todo(kenton):  overloaded versions with more variables?  three seems
  //   to be enough.

  // indent text by two spaces.  after calling indent(), two spaces will be
  // inserted at the beginning of each line of text.  indent() may be called
  // multiple times to produce deeper indents.
  void indent();

  // reduces the current indent level by two spaces, or crashes if the indent
  // level is zero.
  void outdent();

  // write a string to the output buffer.
  // this method does not look for newlines to add indentation.
  void printraw(const string& data);

  // write a zero-delimited string to output buffer.
  // this method does not look for newlines to add indentation.
  void printraw(const char* data);

  // write some bytes to the output buffer.
  // this method does not look for newlines to add indentation.
  void writeraw(const char* data, int size);

  // true if any write to the underlying stream failed.  (we don't just
  // crash in this case because this is an i/o failure, not a programming
  // error.)
  bool failed() const { return failed_; }

 private:
  const char variable_delimiter_;

  zerocopyoutputstream* const output_;
  char* buffer_;
  int buffer_size_;

  string indent_;
  bool at_start_of_line_;
  bool failed_;

  google_disallow_evil_constructors(printer);
};

}  // namespace io
}  // namespace protobuf

}  // namespace google
#endif  // google_protobuf_io_printer_h__
