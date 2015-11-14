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

#include <google/protobuf/io/printer.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {
namespace io {

printer::printer(zerocopyoutputstream* output, char variable_delimiter)
  : variable_delimiter_(variable_delimiter),
    output_(output),
    buffer_(null),
    buffer_size_(0),
    at_start_of_line_(true),
    failed_(false) {
}

printer::~printer() {
  // only backup() if we have called next() at least once and never failed.
  if (buffer_size_ > 0 && !failed_) {
    output_->backup(buffer_size_);
  }
}

void printer::print(const map<string, string>& variables, const char* text) {
  int size = strlen(text);
  int pos = 0;  // the number of bytes we've written so far.

  for (int i = 0; i < size; i++) {
    if (text[i] == '\n') {
      // saw newline.  if there is more text, we may need to insert an indent
      // here.  so, write what we have so far, including the '\n'.
      writeraw(text + pos, i - pos + 1);
      pos = i + 1;

      // setting this true will cause the next writeraw() to insert an indent
      // first.
      at_start_of_line_ = true;

    } else if (text[i] == variable_delimiter_) {
      // saw the start of a variable name.

      // write what we have so far.
      writeraw(text + pos, i - pos);
      pos = i + 1;

      // find closing delimiter.
      const char* end = strchr(text + pos, variable_delimiter_);
      if (end == null) {
        google_log(dfatal) << " unclosed variable name.";
        end = text + pos;
      }
      int endpos = end - text;

      string varname(text + pos, endpos - pos);
      if (varname.empty()) {
        // two delimiters in a row reduce to a literal delimiter character.
        writeraw(&variable_delimiter_, 1);
      } else {
        // replace with the variable's value.
        map<string, string>::const_iterator iter = variables.find(varname);
        if (iter == variables.end()) {
          google_log(dfatal) << " undefined variable: " << varname;
        } else {
          writeraw(iter->second.data(), iter->second.size());
        }
      }

      // advance past this variable.
      i = endpos;
      pos = endpos + 1;
    }
  }

  // write the rest.
  writeraw(text + pos, size - pos);
}

void printer::print(const char* text) {
  static map<string, string> empty;
  print(empty, text);
}

void printer::print(const char* text,
                    const char* variable, const string& value) {
  map<string, string> vars;
  vars[variable] = value;
  print(vars, text);
}

void printer::print(const char* text,
                    const char* variable1, const string& value1,
                    const char* variable2, const string& value2) {
  map<string, string> vars;
  vars[variable1] = value1;
  vars[variable2] = value2;
  print(vars, text);
}

void printer::print(const char* text,
                    const char* variable1, const string& value1,
                    const char* variable2, const string& value2,
                    const char* variable3, const string& value3) {
  map<string, string> vars;
  vars[variable1] = value1;
  vars[variable2] = value2;
  vars[variable3] = value3;
  print(vars, text);
}

void printer::indent() {
  indent_ += "  ";
}

void printer::outdent() {
  if (indent_.empty()) {
    google_log(dfatal) << " outdent() without matching indent().";
    return;
  }

  indent_.resize(indent_.size() - 2);
}

void printer::printraw(const string& data) {
  writeraw(data.data(), data.size());
}

void printer::printraw(const char* data) {
  if (failed_) return;
  writeraw(data, strlen(data));
}

void printer::writeraw(const char* data, int size) {
  if (failed_) return;
  if (size == 0) return;

  if (at_start_of_line_ && (size > 0) && (data[0] != '\n')) {
    // insert an indent.
    at_start_of_line_ = false;
    writeraw(indent_.data(), indent_.size());
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

}  // namespace io
}  // namespace protobuf
}  // namespace google
