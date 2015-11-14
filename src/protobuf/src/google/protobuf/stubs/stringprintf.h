// protocol buffers - google's data interchange format
// copyright 2012 google inc.  all rights reserved.
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

// from google3/base/stringprintf.h
//
// printf variants that place their output in a c++ string.
//
// usage:
//      string result = stringprintf("%d %s\n", 10, "hello");
//      sstringprintf(&result, "%d %s\n", 10, "hello");
//      stringappendf(&result, "%d %s\n", 20, "there");

#ifndef google_protobuf_stubs_stringprintf_h
#define google_protobuf_stubs_stringprintf_h

#include <stdarg.h>
#include <string>
#include <vector>

#include <google/protobuf/stubs/common.h>

namespace google {
namespace protobuf {

// return a c++ string
libprotobuf_export extern string stringprintf(const char* format, ...);

// store result into a supplied string and return it
libprotobuf_export extern const string& sstringprintf(string* dst, const char* format, ...);

// append result to a supplied string
libprotobuf_export extern void stringappendf(string* dst, const char* format, ...);

// lower-level routine that takes a va_list and appends to a specified
// string.  all other routines are just convenience wrappers around it.
libprotobuf_export extern void stringappendv(string* dst, const char* format, va_list ap);

// the max arguments supported by stringprintfvector
libprotobuf_export extern const int kstringprintfvectormaxargs;

// you can use this version when all your arguments are strings, but
// you don't know how many arguments you'll have at compile time.
// stringprintfvector will log(fatal) if v.size() > kstringprintfvectormaxargs
libprotobuf_export extern string stringprintfvector(const char* format, const vector<string>& v);

}  // namespace protobuf
}  // namespace google

#endif  // google_protobuf_stubs_stringprintf_h
