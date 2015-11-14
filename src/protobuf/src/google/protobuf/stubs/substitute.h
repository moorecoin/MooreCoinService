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
// from google3/strings/substitute.h

#include <string>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>

#ifndef google_protobuf_stubs_substitute_h_
#define google_protobuf_stubs_substitute_h_

namespace google {
namespace protobuf {
namespace strings {

// ----------------------------------------------------------------------
// strings::substitute()
// strings::substituteandappend()
//   kind of like stringprintf, but different.
//
//   example:
//     string getmessage(string first_name, string last_name, int age) {
//       return strings::substitute("my name is $0 $1 and i am $2 years old.",
//                                  first_name, last_name, age);
//     }
//
//   differences from stringprintf:
//   * the format string does not identify the types of arguments.
//     instead, the magic of c++ deals with this for us.  see below
//     for a list of accepted types.
//   * substitutions in the format string are identified by a '$'
//     followed by a digit.  so, you can use arguments out-of-order and
//     use the same argument multiple times.
//   * it's much faster than stringprintf.
//
//   supported types:
//   * strings (const char*, const string&)
//     * note that this means you do not have to add .c_str() to all of
//       your strings.  in fact, you shouldn't; it will be slower.
//   * int32, int64, uint32, uint64:  formatted using simpleitoa().
//   * float, double:  formatted using simpleftoa() and simpledtoa().
//   * bool:  printed as "true" or "false".
//
//   substituteandappend() is like substitute() but appends the result to
//   *output.  example:
//
//     string str;
//     strings::substituteandappend(&str,
//                                  "my name is $0 $1 and i am $2 years old.",
//                                  first_name, last_name, age);
//
//   substitute() is significantly faster than stringprintf().  for very
//   large strings, it may be orders of magnitude faster.
// ----------------------------------------------------------------------

namespace internal {  // implementation details.

class substitutearg {
 public:
  inline substitutearg(const char* value)
    : text_(value), size_(strlen(text_)) {}
  inline substitutearg(const string& value)
    : text_(value.data()), size_(value.size()) {}

  // indicates that no argument was given.
  inline explicit substitutearg()
    : text_(null), size_(-1) {}

  // primitives
  // we don't overload for signed and unsigned char because if people are
  // explicitly declaring their chars as signed or unsigned then they are
  // probably actually using them as 8-bit integers and would probably
  // prefer an integer representation.  but, we don't really know.  so, we
  // make the caller decide what to do.
  inline substitutearg(char value)
    : text_(scratch_), size_(1) { scratch_[0] = value; }
  inline substitutearg(short value)
    : text_(fastint32tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(unsigned short value)
    : text_(fastuint32tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(int value)
    : text_(fastint32tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(unsigned int value)
    : text_(fastuint32tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(long value)
    : text_(fastlongtobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(unsigned long value)
    : text_(fastulongtobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(long long value)
    : text_(fastint64tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(unsigned long long value)
    : text_(fastuint64tobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(float value)
    : text_(floattobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(double value)
    : text_(doubletobuffer(value, scratch_)), size_(strlen(text_)) {}
  inline substitutearg(bool value)
    : text_(value ? "true" : "false"), size_(strlen(text_)) {}

  inline const char* data() const { return text_; }
  inline int size() const { return size_; }

 private:
  const char* text_;
  int size_;
  char scratch_[kfasttobuffersize];
};

}  // namespace internal

libprotobuf_export string substitute(
  const char* format,
  const internal::substitutearg& arg0 = internal::substitutearg(),
  const internal::substitutearg& arg1 = internal::substitutearg(),
  const internal::substitutearg& arg2 = internal::substitutearg(),
  const internal::substitutearg& arg3 = internal::substitutearg(),
  const internal::substitutearg& arg4 = internal::substitutearg(),
  const internal::substitutearg& arg5 = internal::substitutearg(),
  const internal::substitutearg& arg6 = internal::substitutearg(),
  const internal::substitutearg& arg7 = internal::substitutearg(),
  const internal::substitutearg& arg8 = internal::substitutearg(),
  const internal::substitutearg& arg9 = internal::substitutearg());

libprotobuf_export void substituteandappend(
  string* output, const char* format,
  const internal::substitutearg& arg0 = internal::substitutearg(),
  const internal::substitutearg& arg1 = internal::substitutearg(),
  const internal::substitutearg& arg2 = internal::substitutearg(),
  const internal::substitutearg& arg3 = internal::substitutearg(),
  const internal::substitutearg& arg4 = internal::substitutearg(),
  const internal::substitutearg& arg5 = internal::substitutearg(),
  const internal::substitutearg& arg6 = internal::substitutearg(),
  const internal::substitutearg& arg7 = internal::substitutearg(),
  const internal::substitutearg& arg8 = internal::substitutearg(),
  const internal::substitutearg& arg9 = internal::substitutearg());

}  // namespace strings
}  // namespace protobuf
}  // namespace google

#endif // google_protobuf_stubs_substitute_h_
