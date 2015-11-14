// copyright 2005 google inc.
// all rights reserved.
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

// ----
// author: lar@google.com (laramie leavitt)
//
// these tests are really compile time tests.
// if you try to step through this in a debugger
// you will not see any evaluations, merely that
// value is assigned true or false sequentially.

#include <google/protobuf/stubs/template_util.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

namespace google_namespace = google::protobuf::internal;

namespace google {
namespace protobuf {
namespace internal {
namespace {

test(templateutiltest, testsize) {
  expect_gt(sizeof(google_namespace::big_), sizeof(google_namespace::small_));
}

test(templateutiltest, testintegralconstants) {
  // test the built-in types.
  expect_true(true_type::value);
  expect_false(false_type::value);

  typedef integral_constant<int, 1> one_type;
  expect_eq(1, one_type::value);
}

test(templateutiltest, testtemplateif) {
  typedef if_<true, true_type, false_type>::type if_true;
  expect_true(if_true::value);

  typedef if_<false, true_type, false_type>::type if_false;
  expect_false(if_false::value);
}

test(templateutiltest, testtemplatetypeequals) {
  // check that the templatetypeequals works correctly.
  bool value = false;

  // test the same type is true.
  value = type_equals_<int, int>::value;
  expect_true(value);

  // test different types are false.
  value = type_equals_<float, int>::value;
  expect_false(value);

  // test type aliasing.
  typedef const int foo;
  value = type_equals_<const foo, const int>::value;
  expect_true(value);
}

test(templateutiltest, testtemplateandor) {
  // check that the templatetypeequals works correctly.
  bool value = false;

  // yes && yes == true.
  value = and_<true_, true_>::value;
  expect_true(value);
  // yes && no == false.
  value = and_<true_, false_>::value;
  expect_false(value);
  // no && yes == false.
  value = and_<false_, true_>::value;
  expect_false(value);
  // no && no == false.
  value = and_<false_, false_>::value;
  expect_false(value);

  // yes || yes == true.
  value = or_<true_, true_>::value;
  expect_true(value);
  // yes || no == true.
  value = or_<true_, false_>::value;
  expect_true(value);
  // no || yes == true.
  value = or_<false_, true_>::value;
  expect_true(value);
  // no || no == false.
  value = or_<false_, false_>::value;
  expect_false(value);
}

test(templateutiltest, testidentity) {
  expect_true(
      (type_equals_<google_namespace::identity_<int>::type, int>::value));
  expect_true(
      (type_equals_<google_namespace::identity_<void>::type, void>::value));
}

}  // anonymous namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
