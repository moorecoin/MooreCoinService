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

#include <google/protobuf/stubs/substitute.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/stl_util.h>

namespace google {
namespace protobuf {
namespace strings {

using internal::substitutearg;

// returns the number of args in arg_array which were passed explicitly
// to substitute().
static int countsubstituteargs(const substitutearg* const* args_array) {
  int count = 0;
  while (args_array[count] != null && args_array[count]->size() != -1) {
    ++count;
  }
  return count;
}

string substitute(
    const char* format,
    const substitutearg& arg0, const substitutearg& arg1,
    const substitutearg& arg2, const substitutearg& arg3,
    const substitutearg& arg4, const substitutearg& arg5,
    const substitutearg& arg6, const substitutearg& arg7,
    const substitutearg& arg8, const substitutearg& arg9) {
  string result;
  substituteandappend(&result, format, arg0, arg1, arg2, arg3, arg4,
                                       arg5, arg6, arg7, arg8, arg9);
  return result;
}

void substituteandappend(
    string* output, const char* format,
    const substitutearg& arg0, const substitutearg& arg1,
    const substitutearg& arg2, const substitutearg& arg3,
    const substitutearg& arg4, const substitutearg& arg5,
    const substitutearg& arg6, const substitutearg& arg7,
    const substitutearg& arg8, const substitutearg& arg9) {
  const substitutearg* const args_array[] = {
    &arg0, &arg1, &arg2, &arg3, &arg4, &arg5, &arg6, &arg7, &arg8, &arg9, null
  };

  // determine total size needed.
  int size = 0;
  for (int i = 0; format[i] != '\0'; i++) {
    if (format[i] == '$') {
      if (ascii_isdigit(format[i+1])) {
        int index = format[i+1] - '0';
        if (args_array[index]->size() == -1) {
          google_log(dfatal)
            << "strings::substitute format string invalid: asked for \"$"
            << index << "\", but only " << countsubstituteargs(args_array)
            << " args were given.  full format string was: \""
            << cescape(format) << "\".";
          return;
        }
        size += args_array[index]->size();
        ++i;  // skip next char.
      } else if (format[i+1] == '$') {
        ++size;
        ++i;  // skip next char.
      } else {
        google_log(dfatal)
          << "invalid strings::substitute() format string: \""
          << cescape(format) << "\".";
        return;
      }
    } else {
      ++size;
    }
  }

  if (size == 0) return;

  // build the string.
  int original_size = output->size();
  stlstringresizeuninitialized(output, original_size + size);
  char* target = string_as_array(output) + original_size;
  for (int i = 0; format[i] != '\0'; i++) {
    if (format[i] == '$') {
      if (ascii_isdigit(format[i+1])) {
        const substitutearg* src = args_array[format[i+1] - '0'];
        memcpy(target, src->data(), src->size());
        target += src->size();
        ++i;  // skip next char.
      } else if (format[i+1] == '$') {
        *target++ = '$';
        ++i;  // skip next char.
      }
    } else {
      *target++ = format[i];
    }
  }

  google_dcheck_eq(target - output->data(), output->size());
}

}  // namespace strings
}  // namespace protobuf
}  // namespace google
