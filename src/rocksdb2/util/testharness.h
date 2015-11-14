//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include "port/stack_trace.h"
#include "rocksdb/env.h"
#include "rocksdb/slice.h"
#include "util/random.h"

namespace rocksdb {
namespace test {

// run some of the tests registered by the test() macro.  if the
// environment variable "rocksdb_tests" is not set, runs all tests.
// otherwise, runs only the tests whose name contains the value of
// "rocksdb_tests" as a substring.  e.g., suppose the tests are:
//    test(foo, hello) { ... }
//    test(foo, world) { ... }
// rocksdb_tests=hello will run the first test
// rocksdb_tests=o     will run both tests
// rocksdb_tests=junk  will run no tests
//
// returns 0 if all tests pass.
// dies or returns a non-zero value if some test fails.
extern int runalltests();

// return the directory to use for temporary storage.
extern std::string tmpdir();

// return a randomization seed for this run.  typically returns the
// same number on repeated invocations of this binary, but automated
// runs may be able to vary the seed.
extern int randomseed();

// an instance of tester is allocated to hold temporary state during
// the execution of an assertion.
class tester {
 private:
  bool ok_;
  const char* fname_;
  int line_;
  std::stringstream ss_;

 public:
  tester(const char* f, int l)
      : ok_(true), fname_(f), line_(l) {
  }

  ~tester() {
    if (!ok_) {
      fprintf(stderr, "%s:%d:%s\n", fname_, line_, ss_.str().c_str());
      port::printstack(2);
      exit(1);
    }
  }

  tester& is(bool b, const char* msg) {
    if (!b) {
      ss_ << " assertion failure " << msg;
      ok_ = false;
    }
    return *this;
  }

  tester& isok(const status& s) {
    if (!s.ok()) {
      ss_ << " " << s.tostring();
      ok_ = false;
    }
    return *this;
  }

#define binary_op(name,op)                              \
  template <class x, class y>                           \
  tester& name(const x& x, const y& y) {                \
    if (! (x op y)) {                                   \
      ss_ << " failed: " << x << (" " #op " ") << y;    \
      ok_ = false;                                      \
    }                                                   \
    return *this;                                       \
  }

  binary_op(iseq, ==)
  binary_op(isne, !=)
  binary_op(isge, >=)
  binary_op(isgt, >)
  binary_op(isle, <=)
  binary_op(islt, <)
#undef binary_op

  // attach the specified value to the error message if an error has occurred
  template <class v>
  tester& operator<<(const v& value) {
    if (!ok_) {
      ss_ << " " << value;
    }
    return *this;
  }
};

#define assert_true(c) ::rocksdb::test::tester(__file__, __line__).is((c), #c)
#define assert_ok(s) ::rocksdb::test::tester(__file__, __line__).isok((s))
#define assert_eq(a,b) ::rocksdb::test::tester(__file__, __line__).iseq((a),(b))
#define assert_ne(a,b) ::rocksdb::test::tester(__file__, __line__).isne((a),(b))
#define assert_ge(a,b) ::rocksdb::test::tester(__file__, __line__).isge((a),(b))
#define assert_gt(a,b) ::rocksdb::test::tester(__file__, __line__).isgt((a),(b))
#define assert_le(a,b) ::rocksdb::test::tester(__file__, __line__).isle((a),(b))
#define assert_lt(a,b) ::rocksdb::test::tester(__file__, __line__).islt((a),(b))

#define tconcat(a,b) tconcat1(a,b)
#define tconcat1(a,b) a##b

#define test(base,name)                                                 \
class tconcat(_test_,name) : public base {                              \
 public:                                                                \
  void _run();                                                          \
  static void _runit() {                                                \
    tconcat(_test_,name) t;                                             \
    t._run();                                                           \
  }                                                                     \
};                                                                      \
bool tconcat(_test_ignored_,name) =                                     \
  ::rocksdb::test::registertest(#base, #name, &tconcat(_test_,name)::_runit); \
void tconcat(_test_,name)::_run()

// register the specified test.  typically not used directly, but
// invoked via the macro expansion of test.
extern bool registertest(const char* base, const char* name, void (*func)());


}  // namespace test
}  // namespace rocksdb
