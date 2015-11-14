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

// from google3/base/stringprintf_unittest.cc

#include <google/protobuf/stubs/stringprintf.h>

#include <cerrno>
#include <string>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace {

test(stringprintftest, empty) {
#if 0
  // gcc 2.95.3, gcc 4.1.0, and gcc 4.2.2 all warn about this:
  // warning: zero-length printf format string.
  // so we do not allow them in google3.
  expect_eq("", stringprintf(""));
#endif
  expect_eq("", stringprintf("%s", string().c_str()));
  expect_eq("", stringprintf("%s", ""));
}

test(stringprintftest, misc) {
// msvc and mingw does not support $ format specifier.
#if !defined(_msc_ver) && !defined(__mingw32__)
  expect_eq("123hello w", stringprintf("%3$d%2$s %1$c", 'w', "hello", 123));
#endif  // !_msc_ver
}

test(stringappendftest, empty) {
  string value("hello");
  const char* empty = "";
  stringappendf(&value, "%s", empty);
  expect_eq("hello", value);
}

test(stringappendftest, emptystring) {
  string value("hello");
  stringappendf(&value, "%s", "");
  expect_eq("hello", value);
}

test(stringappendftest, string) {
  string value("hello");
  stringappendf(&value, " %s", "world");
  expect_eq("hello world", value);
}

test(stringappendftest, int) {
  string value("hello");
  stringappendf(&value, " %d", 123);
  expect_eq("hello 123", value);
}

test(stringprintftest, multibyte) {
  // if we are in multibyte mode and feed invalid multibyte sequence,
  // stringprintf should return an empty string instead of running
  // out of memory while trying to determine destination buffer size.
  // see b/4194543.

  char* old_locale = setlocale(lc_ctype, null);
  // push locale with multibyte mode
  setlocale(lc_ctype, "en_us.utf8");

  const char kinvalidcodepoint[] = "\375\067s";
  string value = stringprintf("%.*s", 3, kinvalidcodepoint);

  // in some versions of glibc (e.g. eglibc-2.11.1, aka grtev2), snprintf
  // returns error given an invalid codepoint. other versions
  // (e.g. eglibc-2.15, aka pre-grtev3) emit the codepoint verbatim.
  // we test that the output is one of the above.
  expect_true(value.empty() || value == kinvalidcodepoint);

  // repeat with longer string, to make sure that the dynamically
  // allocated path in stringappendv is handled correctly.
  int n = 2048;
  char* buf = new char[n+1];
  memset(buf, ' ', n-3);
  memcpy(buf + n - 3, kinvalidcodepoint, 4);
  value =  stringprintf("%.*s", n, buf);
  // see grtev2 vs. grtev3 comment above.
  expect_true(value.empty() || value == buf);
  delete[] buf;

  setlocale(lc_ctype, old_locale);
}

test(stringprintftest, nomultibyte) {
  // no multibyte handling, but the string contains funny chars.
  char* old_locale = setlocale(lc_ctype, null);
  setlocale(lc_ctype, "posix");
  string value = stringprintf("%.*s", 3, "\375\067s");
  setlocale(lc_ctype, old_locale);
  expect_eq("\375\067s", value);
}

test(stringprintftest, dontoverwriteerrno) {
  // check that errno isn't overwritten unless we're printing
  // something significantly larger than what people are normally
  // printing in their badly written plog() statements.
  errno = echild;
  string value = stringprintf("hello, %s!", "world");
  expect_eq(echild, errno);
}

test(stringprintftest, largebuf) {
  // check that the large buffer is handled correctly.
  int n = 2048;
  char* buf = new char[n+1];
  memset(buf, ' ', n);
  buf[n] = 0;
  string value = stringprintf("%s", buf);
  expect_eq(buf, value);
  delete[] buf;
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google
