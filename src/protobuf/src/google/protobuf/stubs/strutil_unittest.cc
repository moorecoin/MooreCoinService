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

#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>
#include <locale.h>

namespace google {
namespace protobuf {
namespace {

// todo(kenton):  copy strutil tests from google3?

test(stringutilitytest, immunetolocales) {
  // remember the old locale.
  char* old_locale_cstr = setlocale(lc_numeric, null);
  assert_true(old_locale_cstr != null);
  string old_locale = old_locale_cstr;

  // set the locale to "c".
  assert_true(setlocale(lc_numeric, "c") != null);

  expect_eq(1.5, nolocalestrtod("1.5", null));
  expect_eq("1.5", simpledtoa(1.5));
  expect_eq("1.5", simpleftoa(1.5));

  // verify that the endptr is set correctly even if not all text was parsed.
  const char* text = "1.5f";
  char* endptr;
  expect_eq(1.5, nolocalestrtod(text, &endptr));
  expect_eq(3, endptr - text);

  if (setlocale(lc_numeric, "es_es") == null &&
      setlocale(lc_numeric, "es_es.utf8") == null) {
    // some systems may not have the desired locale available.
    google_log(warning)
      << "couldn't set locale to es_es.  skipping this test.";
  } else {
    expect_eq(1.5, nolocalestrtod("1.5", null));
    expect_eq("1.5", simpledtoa(1.5));
    expect_eq("1.5", simpleftoa(1.5));
    expect_eq(1.5, nolocalestrtod(text, &endptr));
    expect_eq(3, endptr - text);
  }

  // return to original locale.
  setlocale(lc_numeric, old_locale.c_str());
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google
