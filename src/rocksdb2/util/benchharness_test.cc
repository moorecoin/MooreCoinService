//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#include "util/benchharness.h"
#include <vector>

namespace rocksdb {

benchmark(insertfrontvector) {
  std::vector<int> v;
  for (int i = 0; i < 100; i++) {
    v.insert(v.begin(), i);
  }
}

benchmark_relative(insertbackvector) {
  std::vector<int> v;
  for (size_t i = 0; i < 100; i++) {
    v.insert(v.end(), i);
  }
}

benchmark_n(insertfrontvector_n, n) {
  std::vector<int> v;
  for (size_t i = 0; i < n; i++) {
    v.insert(v.begin(), i);
  }
}

benchmark_relative_n(insertbackvector_n, n) {
  std::vector<int> v;
  for (size_t i = 0; i < n; i++) {
    v.insert(v.end(), i);
  }
}

benchmark_n(insertfrontend_n, n) {
  std::vector<int> v;
  for (size_t i = 0; i < n; i++) {
    v.insert(v.begin(), i);
  }
  for (size_t i = 0; i < n; i++) {
    v.insert(v.end(), i);
  }
}

benchmark_relative_n(insertfrontendsuspend_n, n) {
  std::vector<int> v;
  for (size_t i = 0; i < n; i++) {
    v.insert(v.begin(), i);
  }
  benchmark_suspend {
    for (size_t i = 0; i < n; i++) {
      v.insert(v.end(), i);
    }
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  rocksdb::benchmark::runbenchmarks();
  return 0;
}
