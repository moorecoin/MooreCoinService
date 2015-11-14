// copyright 2008 google inc. all rights reserved.
// author: xpeng@google.com (peter peng)

#include <google/protobuf/stubs/common.h>
#include <gtest/gtest.h>

namespace google {
namespace protobuf {
namespace internal {
namespace {

test(structurallyvalidtest, validutf8string) {
  // on gcc, this string can be written as:
  //   "abcd 1234 - \u2014\u2013\u2212"
  // msvc seems to interpret \u differently.
  string valid_str("abcd 1234 - \342\200\224\342\200\223\342\210\222 - xyz789");
  expect_true(isstructurallyvalidutf8(valid_str.data(),
                                      valid_str.size()));
  // additional check for pointer alignment
  for (int i = 1; i < 8; ++i) {
    expect_true(isstructurallyvalidutf8(valid_str.data() + i,
                                        valid_str.size() - i));
  }
}

test(structurallyvalidtest, invalidutf8string) {
  const string invalid_str("abcd\xa0\xb0\xa0\xb0\xa0\xb0 - xyz789");
  expect_false(isstructurallyvalidutf8(invalid_str.data(),
                                       invalid_str.size()));
  // additional check for pointer alignment
  for (int i = 1; i < 8; ++i) {
    expect_false(isstructurallyvalidutf8(invalid_str.data() + i,
                                         invalid_str.size() - i));
  }
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google
