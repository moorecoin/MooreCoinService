//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "port/stack_trace.h"
#include <assert.h>

namespace {
void f0() {
  char *p = nullptr;
  *p = 10;  /* sigsegv here!! */
}

void f1() {
  f0();
}

void f2() {
  f1();
}

void f3() {
  f2();
}
}  // namespace

int main() {
  rocksdb::port::installstacktracehandler();

  f3();

  return 0;
}
