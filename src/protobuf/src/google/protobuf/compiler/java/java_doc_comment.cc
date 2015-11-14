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

#include <google/protobuf/compiler/java/java_doc_comment.h>

#include <vector>

#include <google/protobuf/io/printer.h>
#include <google/protobuf/stubs/strutil.h>

namespace google {
namespace protobuf {
namespace compiler {
namespace java {

string escapejavadoc(const string& input) {
  string result;
  result.reserve(input.size() * 2);

  char prev = '*';

  for (string::size_type i = 0; i < input.size(); i++) {
    char c = input[i];
    switch (c) {
      case '*':
        // avoid "/*".
        if (prev == '/') {
          result.append("&#42;");
        } else {
          result.push_back(c);
        }
        break;
      case '/':
        // avoid "*/".
        if (prev == '*') {
          result.append("&#47;");
        } else {
          result.push_back(c);
        }
        break;
      case '@':
        // "{@" starts javadoc markup.
        if (prev == '{') {
          result.append("&#64;");
        } else {
          result.push_back(c);
        }
        break;
      case '<':
        // avoid interpretation as html.
        result.append("&lt;");
        break;
      case '>':
        // avoid interpretation as html.
        result.append("&gt;");
        break;
      case '&':
        // avoid interpretation as html.
        result.append("&amp;");
        break;
      case '\\':
        // java interprets unicode escape sequences anywhere!
        result.append("&#92;");
        break;
      default:
        result.push_back(c);
        break;
    }

    prev = c;
  }

  return result;
}

static void writedoccommentbodyforlocation(
    io::printer* printer, const sourcelocation& location) {
  string comments = location.leading_comments.empty() ?
      location.trailing_comments : location.leading_comments;
  if (!comments.empty()) {
    // todo(kenton):  ideally we should parse the comment text as markdown and
    //   write it back as html, but this requires a markdown parser.  for now
    //   we just use <pre> to get fixed-width text formatting.

    // if the comment itself contains block comment start or end markers,
    // html-escape them so that they don't accidentally close the doc comment.
    comments = escapejavadoc(comments);

    vector<string> lines;
    splitstringallowempty(comments, "\n", &lines);
    while (!lines.empty() && lines.back().empty()) {
      lines.pop_back();
    }

    printer->print(
        " *\n"
        " * <pre>\n");
    for (int i = 0; i < lines.size(); i++) {
      // most lines should start with a space.  watch out for lines that start
      // with a /, since putting that right after the leading asterisk will
      // close the comment.
      if (!lines[i].empty() && lines[i][0] == '/') {
        printer->print(" * $line$\n", "line", lines[i]);
      } else {
        printer->print(" *$line$\n", "line", lines[i]);
      }
    }
    printer->print(" * </pre>\n");
  }
}

template <typename descriptortype>
static void writedoccommentbody(
    io::printer* printer, const descriptortype* descriptor) {
  sourcelocation location;
  if (descriptor->getsourcelocation(&location)) {
    writedoccommentbodyforlocation(printer, location);
  }
}

static string firstlineof(const string& value) {
  string result = value;

  string::size_type pos = result.find_first_of('\n');
  if (pos != string::npos) {
    result.erase(pos);
  }

  // if line ends in an opening brace, make it "{ ... }" so it looks nice.
  if (!result.empty() && result[result.size() - 1] == '{') {
    result.append(" ... }");
  }

  return result;
}

void writemessagedoccomment(io::printer* printer, const descriptor* message) {
  printer->print(
    "/**\n"
    " * protobuf type {@code $fullname$}\n",
    "fullname", escapejavadoc(message->full_name()));
  writedoccommentbody(printer, message);
  printer->print(" */\n");
}

void writefielddoccomment(io::printer* printer, const fielddescriptor* field) {
  // in theory we should have slightly different comments for setters, getters,
  // etc., but in practice everyone already knows the difference between these
  // so it's redundant information.

  // we use the field declaration as the first line of the comment, e.g.:
  //   optional string foo = 5;
  // this communicates a lot of information about the field in a small space.
  // if the field is a group, the debug string might end with {.
  printer->print(
    "/**\n"
    " * <code>$def$</code>\n",
    "def", escapejavadoc(firstlineof(field->debugstring())));
  writedoccommentbody(printer, field);
  printer->print(" */\n");
}

void writeenumdoccomment(io::printer* printer, const enumdescriptor* enum_) {
  printer->print(
    "/**\n"
    " * protobuf enum {@code $fullname$}\n",
    "fullname", escapejavadoc(enum_->full_name()));
  writedoccommentbody(printer, enum_);
  printer->print(" */\n");
}

void writeenumvaluedoccomment(io::printer* printer,
                              const enumvaluedescriptor* value) {
  printer->print(
    "/**\n"
    " * <code>$def$</code>\n",
    "def", escapejavadoc(firstlineof(value->debugstring())));
  writedoccommentbody(printer, value);
  printer->print(" */\n");
}

void writeservicedoccomment(io::printer* printer,
                            const servicedescriptor* service) {
  printer->print(
    "/**\n"
    " * protobuf service {@code $fullname$}\n",
    "fullname", escapejavadoc(service->full_name()));
  writedoccommentbody(printer, service);
  printer->print(" */\n");
}

void writemethoddoccomment(io::printer* printer,
                           const methoddescriptor* method) {
  printer->print(
    "/**\n"
    " * <code>$def$</code>\n",
    "def", escapejavadoc(firstlineof(method->debugstring())));
  writedoccommentbody(printer, method);
  printer->print(" */\n");
}

}  // namespace java
}  // namespace compiler
}  // namespace protobuf
}  // namespace google
