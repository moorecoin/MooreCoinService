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

#include <vector>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/substitute.h>

#include <google/protobuf/testing/googletest.h>
#include <gtest/gtest.h>

#include "config.h"

namespace google {
namespace protobuf {
namespace {

// todo(kenton):  more tests.

#ifdef package_version  // only defined when using automake, not msvc

test(versiontest, versionmatchesconfig) {
  // verify that the version string specified in config.h matches the one
  // in common.h.  the config.h version is a string which may have a suffix
  // like "beta" or "rc1", so we remove that.
  string version = package_version;
  int pos = 0;
  while (pos < version.size() &&
         (ascii_isdigit(version[pos]) || version[pos] == '.')) {
    ++pos;
  }
  version.erase(pos);

  expect_eq(version, internal::versionstring(google_protobuf_version));
}

#endif  // package_version

test(commontest, intminmaxconstants) {
  // kint32min was declared incorrectly in the first release of protobufs.
  // ugh.
  expect_lt(kint32min, kint32max);
  expect_eq(static_cast<uint32>(kint32min), static_cast<uint32>(kint32max) + 1);
  expect_lt(kint64min, kint64max);
  expect_eq(static_cast<uint64>(kint64min), static_cast<uint64>(kint64max) + 1);
  expect_eq(0, kuint32max + 1);
  expect_eq(0, kuint64max + 1);
}

vector<string> captured_messages_;

void capturelog(loglevel level, const char* filename, int line,
                const string& message) {
  captured_messages_.push_back(
    strings::substitute("$0 $1:$2: $3",
      implicit_cast<int>(level), filename, line, message));
}

test(loggingtest, defaultlogging) {
  captureteststderr();
  int line = __line__;
  google_log(info   ) << "a message.";
  google_log(warning) << "a warning.";
  google_log(error  ) << "an error.";

  string text = getcapturedteststderr();
  expect_eq(
    "[libprotobuf info "__file__":" + simpleitoa(line + 1) + "] a message.\n"
    "[libprotobuf warning "__file__":" + simpleitoa(line + 2) + "] a warning.\n"
    "[libprotobuf error "__file__":" + simpleitoa(line + 3) + "] an error.\n",
    text);
}

test(loggingtest, nulllogging) {
  loghandler* old_handler = setloghandler(null);

  captureteststderr();
  google_log(info   ) << "a message.";
  google_log(warning) << "a warning.";
  google_log(error  ) << "an error.";

  expect_true(setloghandler(old_handler) == null);

  string text = getcapturedteststderr();
  expect_eq("", text);
}

test(loggingtest, capturelogging) {
  captured_messages_.clear();

  loghandler* old_handler = setloghandler(&capturelog);

  int start_line = __line__;
  google_log(error) << "an error.";
  google_log(warning) << "a warning.";

  expect_true(setloghandler(old_handler) == &capturelog);

  assert_eq(2, captured_messages_.size());
  expect_eq(
    "2 "__file__":" + simpleitoa(start_line + 1) + ": an error.",
    captured_messages_[0]);
  expect_eq(
    "1 "__file__":" + simpleitoa(start_line + 2) + ": a warning.",
    captured_messages_[1]);
}

test(loggingtest, silencelogging) {
  captured_messages_.clear();

  loghandler* old_handler = setloghandler(&capturelog);

  int line1 = __line__; google_log(info) << "visible1";
  logsilencer* silencer1 = new logsilencer;
  google_log(info) << "not visible.";
  logsilencer* silencer2 = new logsilencer;
  google_log(info) << "not visible.";
  delete silencer1;
  google_log(info) << "not visible.";
  delete silencer2;
  int line2 = __line__; google_log(info) << "visible2";

  expect_true(setloghandler(old_handler) == &capturelog);

  assert_eq(2, captured_messages_.size());
  expect_eq(
    "0 "__file__":" + simpleitoa(line1) + ": visible1",
    captured_messages_[0]);
  expect_eq(
    "0 "__file__":" + simpleitoa(line2) + ": visible2",
    captured_messages_[1]);
}

class closuretest : public testing::test {
 public:
  void seta123method()   { a_ = 123; }
  static void seta123function() { current_instance_->a_ = 123; }

  void setamethod(int a)         { a_ = a; }
  void setcmethod(string c)      { c_ = c; }

  static void setafunction(int a)         { current_instance_->a_ = a; }
  static void setcfunction(string c)      { current_instance_->c_ = c; }

  void setabmethod(int a, const char* b)  { a_ = a; b_ = b; }
  static void setabfunction(int a, const char* b) {
    current_instance_->a_ = a;
    current_instance_->b_ = b;
  }

  virtual void setup() {
    current_instance_ = this;
    a_ = 0;
    b_ = null;
    c_.clear();
    permanent_closure_ = null;
  }

  void deleteclosureincallback() {
    delete permanent_closure_;
  }

  int a_;
  const char* b_;
  string c_;
  closure* permanent_closure_;

  static closuretest* current_instance_;
};

closuretest* closuretest::current_instance_ = null;

test_f(closuretest, testclosurefunction0) {
  closure* closure = newcallback(&seta123function);
  expect_ne(123, a_);
  closure->run();
  expect_eq(123, a_);
}

test_f(closuretest, testclosuremethod0) {
  closure* closure = newcallback(current_instance_,
                                 &closuretest::seta123method);
  expect_ne(123, a_);
  closure->run();
  expect_eq(123, a_);
}

test_f(closuretest, testclosurefunction1) {
  closure* closure = newcallback(&setafunction, 456);
  expect_ne(456, a_);
  closure->run();
  expect_eq(456, a_);
}

test_f(closuretest, testclosuremethod1) {
  closure* closure = newcallback(current_instance_,
                                 &closuretest::setamethod, 456);
  expect_ne(456, a_);
  closure->run();
  expect_eq(456, a_);
}

test_f(closuretest, testclosurefunction1string) {
  closure* closure = newcallback(&setcfunction, string("test"));
  expect_ne("test", c_);
  closure->run();
  expect_eq("test", c_);
}

test_f(closuretest, testclosuremethod1string) {
  closure* closure = newcallback(current_instance_,
                                 &closuretest::setcmethod, string("test"));
  expect_ne("test", c_);
  closure->run();
  expect_eq("test", c_);
}

test_f(closuretest, testclosurefunction2) {
  const char* cstr = "hello";
  closure* closure = newcallback(&setabfunction, 789, cstr);
  expect_ne(789, a_);
  expect_ne(cstr, b_);
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
}

test_f(closuretest, testclosuremethod2) {
  const char* cstr = "hello";
  closure* closure = newcallback(current_instance_,
                                 &closuretest::setabmethod, 789, cstr);
  expect_ne(789, a_);
  expect_ne(cstr, b_);
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
}

// repeat all of the above with newpermanentcallback()

test_f(closuretest, testpermanentclosurefunction0) {
  closure* closure = newpermanentcallback(&seta123function);
  expect_ne(123, a_);
  closure->run();
  expect_eq(123, a_);
  a_ = 0;
  closure->run();
  expect_eq(123, a_);
  delete closure;
}

test_f(closuretest, testpermanentclosuremethod0) {
  closure* closure = newpermanentcallback(current_instance_,
                                          &closuretest::seta123method);
  expect_ne(123, a_);
  closure->run();
  expect_eq(123, a_);
  a_ = 0;
  closure->run();
  expect_eq(123, a_);
  delete closure;
}

test_f(closuretest, testpermanentclosurefunction1) {
  closure* closure = newpermanentcallback(&setafunction, 456);
  expect_ne(456, a_);
  closure->run();
  expect_eq(456, a_);
  a_ = 0;
  closure->run();
  expect_eq(456, a_);
  delete closure;
}

test_f(closuretest, testpermanentclosuremethod1) {
  closure* closure = newpermanentcallback(current_instance_,
                                          &closuretest::setamethod, 456);
  expect_ne(456, a_);
  closure->run();
  expect_eq(456, a_);
  a_ = 0;
  closure->run();
  expect_eq(456, a_);
  delete closure;
}

test_f(closuretest, testpermanentclosurefunction2) {
  const char* cstr = "hello";
  closure* closure = newpermanentcallback(&setabfunction, 789, cstr);
  expect_ne(789, a_);
  expect_ne(cstr, b_);
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
  a_ = 0;
  b_ = null;
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
  delete closure;
}

test_f(closuretest, testpermanentclosuremethod2) {
  const char* cstr = "hello";
  closure* closure = newpermanentcallback(current_instance_,
                                          &closuretest::setabmethod, 789, cstr);
  expect_ne(789, a_);
  expect_ne(cstr, b_);
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
  a_ = 0;
  b_ = null;
  closure->run();
  expect_eq(789, a_);
  expect_eq(cstr, b_);
  delete closure;
}

test_f(closuretest, testpermanentclosuredeleteincallback) {
  permanent_closure_ = newpermanentcallback((closuretest*) this,
      &closuretest::deleteclosureincallback);
  permanent_closure_->run();
}

}  // anonymous namespace
}  // namespace protobuf
}  // namespace google
