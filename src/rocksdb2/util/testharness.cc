//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/testharness.h"
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "port/stack_trace.h"

namespace rocksdb {
namespace test {

namespace {
struct test {
  const char* base;
  const char* name;
  void (*func)();
};
std::vector<test>* tests;
}

bool registertest(const char* base, const char* name, void (*func)()) {
  if (tests == nullptr) {
    tests = new std::vector<test>;
  }
  test t;
  t.base = base;
  t.name = name;
  t.func = func;
  tests->push_back(t);
  return true;
}

int runalltests() {
  port::installstacktracehandler();

  const char* matcher = getenv("rocksdb_tests");

  int num = 0;
  if (tests != nullptr) {
    for (unsigned int i = 0; i < tests->size(); i++) {
      const test& t = (*tests)[i];
      if (matcher != nullptr) {
        std::string name = t.base;
        name.push_back('.');
        name.append(t.name);
        if (strstr(name.c_str(), matcher) == nullptr) {
          continue;
        }
      }
      fprintf(stderr, "==== test %s.%s\n", t.base, t.name);
      (*t.func)();
      ++num;
    }
  }
  fprintf(stderr, "==== passed %d tests\n", num);
  return 0;
}

std::string tmpdir() {
  std::string dir;
  status s = env::default()->gettestdirectory(&dir);
  assert_true(s.ok()) << s.tostring();
  return dir;
}

int randomseed() {
  const char* env = getenv("test_random_seed");
  int result = (env != nullptr ? atoi(env) : 301);
  if (result <= 0) {
    result = 301;
  }
  return result;
}

}  // namespace test
}  // namespace rocksdb
