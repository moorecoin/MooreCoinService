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

// from google3/base/stringprintf.cc

#include <google/protobuf/stubs/stringprintf.h>

#include <errno.h>
#include <stdarg.h> // for va_list and related operations
#include <stdio.h> // msvc requires this for _vsnprintf
#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/testing/googletest.h>

namespace google {
namespace protobuf {

#ifdef _msc_ver
enum { is_compiler_msvc = 1 };
#ifndef va_copy
// define va_copy for msvc. this is a hack, assuming va_list is simply a
// pointer into the stack and is safe to copy.
#define va_copy(dest, src) ((dest) = (src))
#endif
#else
enum { is_compiler_msvc = 0 };
#endif

void stringappendv(string* dst, const char* format, va_list ap) {
  // first try with a small fixed size buffer
  static const int kspacelength = 1024;
  char space[kspacelength];

  // it's possible for methods that use a va_list to invalidate
  // the data in it upon use.  the fix is to make a copy
  // of the structure before using it and use that copy instead.
  va_list backup_ap;
  va_copy(backup_ap, ap);
  int result = vsnprintf(space, kspacelength, format, backup_ap);
  va_end(backup_ap);

  if (result < kspacelength) {
    if (result >= 0) {
      // normal case -- everything fit.
      dst->append(space, result);
      return;
    }

    if (is_compiler_msvc) {
      // error or msvc running out of space.  msvc 8.0 and higher
      // can be asked about space needed with the special idiom below:
      va_copy(backup_ap, ap);
      result = vsnprintf(null, 0, format, backup_ap);
      va_end(backup_ap);
    }

    if (result < 0) {
      // just an error.
      return;
    }
  }

  // increase the buffer size to the size requested by vsnprintf,
  // plus one for the closing \0.
  int length = result+1;
  char* buf = new char[length];

  // restore the va_list before we use it again
  va_copy(backup_ap, ap);
  result = vsnprintf(buf, length, format, backup_ap);
  va_end(backup_ap);

  if (result >= 0 && result < length) {
    // it fit
    dst->append(buf, result);
  }
  delete[] buf;
}


string stringprintf(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  string result;
  stringappendv(&result, format, ap);
  va_end(ap);
  return result;
}

const string& sstringprintf(string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  dst->clear();
  stringappendv(dst, format, ap);
  va_end(ap);
  return *dst;
}

void stringappendf(string* dst, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  stringappendv(dst, format, ap);
  va_end(ap);
}

// max arguments supported by stringprintvector
const int kstringprintfvectormaxargs = 32;

// an empty block of zero for filler arguments.  this is const so that if
// printf tries to write to it (via %n) then the program gets a sigsegv
// and we can fix the problem or protect against an attack.
static const char string_printf_empty_block[256] = { '\0' };

string stringprintfvector(const char* format, const vector<string>& v) {
  google_check_le(v.size(), kstringprintfvectormaxargs)
      << "stringprintfvector currently only supports up to "
      << kstringprintfvectormaxargs << " arguments. "
      << "feel free to add support for more if you need it.";

  // add filler arguments so that bogus format+args have a harder time
  // crashing the program, corrupting the program (%n),
  // or displaying random chunks of memory to users.

  const char* cstr[kstringprintfvectormaxargs];
  for (int i = 0; i < v.size(); ++i) {
    cstr[i] = v[i].c_str();
  }
  for (int i = v.size(); i < google_arraysize(cstr); ++i) {
    cstr[i] = &string_printf_empty_block[0];
  }

  // i do not know any way to pass kstringprintfvectormaxargs arguments,
  // or any way to build a va_list by hand, or any api for printf
  // that accepts an array of arguments.  the best i can do is stick
  // this compile_assert right next to the actual statement.

  google_compile_assert(kstringprintfvectormaxargs == 32, arg_count_mismatch);
  return stringprintf(format,
                      cstr[0], cstr[1], cstr[2], cstr[3], cstr[4],
                      cstr[5], cstr[6], cstr[7], cstr[8], cstr[9],
                      cstr[10], cstr[11], cstr[12], cstr[13], cstr[14],
                      cstr[15], cstr[16], cstr[17], cstr[18], cstr[19],
                      cstr[20], cstr[21], cstr[22], cstr[23], cstr[24],
                      cstr[25], cstr[26], cstr[27], cstr[28], cstr[29],
                      cstr[30], cstr[31]);
}
}  // namespace protobuf
}  // namespace google
