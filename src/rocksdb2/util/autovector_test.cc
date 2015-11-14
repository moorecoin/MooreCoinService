//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <atomic>
#include <iostream>

#include "rocksdb/env.h"
#include "util/autovector.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

using namespace std;

class autovectortest { };

const unsigned long ksize = 8;
test(autovectortest, pushbackandpopback) {
  autovector<size_t, ksize> vec;
  assert_true(vec.empty());
  assert_eq(0ul, vec.size());

  for (size_t i = 0; i < 1000 * ksize; ++i) {
    vec.push_back(i);
    assert_true(!vec.empty());
    if (i < ksize) {
      assert_true(vec.only_in_stack());
    } else {
      assert_true(!vec.only_in_stack());
    }
    assert_eq(i + 1, vec.size());
    assert_eq(i, vec[i]);
    assert_eq(i, vec.at(i));
  }

  size_t size = vec.size();
  while (size != 0) {
    vec.pop_back();
    // will always be in heap
    assert_true(!vec.only_in_stack());
    assert_eq(--size, vec.size());
  }

  assert_true(vec.empty());
}

test(autovectortest, emplaceback) {
  typedef std::pair<size_t, std::string> valuetype;
  autovector<valuetype, ksize> vec;

  for (size_t i = 0; i < 1000 * ksize; ++i) {
    vec.emplace_back(i, std::to_string(i + 123));
    assert_true(!vec.empty());
    if (i < ksize) {
      assert_true(vec.only_in_stack());
    } else {
      assert_true(!vec.only_in_stack());
    }

    assert_eq(i + 1, vec.size());
    assert_eq(i, vec[i].first);
    assert_eq(std::to_string(i + 123), vec[i].second);
  }

  vec.clear();
  assert_true(vec.empty());
  assert_true(!vec.only_in_stack());
}

test(autovectortest, resize) {
  autovector<size_t, ksize> vec;

  vec.resize(ksize);
  assert_true(vec.only_in_stack());
  for (size_t i = 0; i < ksize; ++i) {
    vec[i] = i;
  }

  vec.resize(ksize * 2);
  assert_true(!vec.only_in_stack());
  for (size_t i = 0; i < ksize; ++i) {
    assert_eq(vec[i], i);
  }
  for (size_t i = 0; i < ksize; ++i) {
    vec[i + ksize] = i;
  }

  vec.resize(1);
  assert_eq(1u, vec.size());
}

namespace {
void assertequal(
    const autovector<size_t, ksize>& a, const autovector<size_t, ksize>& b) {
  assert_eq(a.size(), b.size());
  assert_eq(a.empty(), b.empty());
  assert_eq(a.only_in_stack(), b.only_in_stack());
  for (size_t i = 0; i < a.size(); ++i) {
    assert_eq(a[i], b[i]);
  }
}
}  // namespace

test(autovectortest, copyandassignment) {
  // test both heap-allocated and stack-allocated cases.
  for (auto size : { ksize / 2, ksize * 1000 }) {
    autovector<size_t, ksize> vec;
    for (size_t i = 0; i < size; ++i) {
      vec.push_back(i);
    }

    {
      autovector<size_t, ksize> other;
      other = vec;
      assertequal(other, vec);
    }

    {
      autovector<size_t, ksize> other(vec);
      assertequal(other, vec);
    }
  }
}

test(autovectortest, iterators) {
  autovector<std::string, ksize> vec;
  for (size_t i = 0; i < ksize * 1000; ++i) {
    vec.push_back(std::to_string(i));
  }

  // basic operator test
  assert_eq(vec.front(), *vec.begin());
  assert_eq(vec.back(), *(vec.end() - 1));
  assert_true(vec.begin() < vec.end());

  // non-const iterator
  size_t index = 0;
  for (const auto& item : vec) {
    assert_eq(vec[index++], item);
  }

  index = vec.size() - 1;
  for (auto pos = vec.rbegin(); pos != vec.rend(); ++pos) {
    assert_eq(vec[index--], *pos);
  }

  // const iterator
  const auto& cvec = vec;
  index = 0;
  for (const auto& item : cvec) {
    assert_eq(cvec[index++], item);
  }

  index = vec.size() - 1;
  for (auto pos = cvec.rbegin(); pos != cvec.rend(); ++pos) {
    assert_eq(cvec[index--], *pos);
  }

  // forward and backward
  auto pos = vec.begin();
  while (pos != vec.end()) {
    auto old_val = *pos;
    auto old = pos++;
    // hack: make sure -> works
    assert_true(!old->empty());
    assert_eq(old_val, *old);
    assert_true(pos == vec.end() || old_val != *pos);
  }

  pos = vec.begin();
  for (size_t i = 0; i < vec.size(); i += 2) {
    // cannot use assert_eq since that macro depends on iostream serialization
    assert_true(pos + 2 - 2 == pos);
    pos += 2;
    assert_true(pos >= vec.begin());
    assert_true(pos <= vec.end());

    size_t diff = static_cast<size_t>(pos - vec.begin());
    assert_eq(i + 2, diff);
  }
}

namespace {
vector<string> gettestkeys(size_t size) {
  vector<string> keys;
  keys.resize(size);

  int index = 0;
  for (auto& key : keys) {
    key = "item-" + to_string(index++);
  }
  return keys;
}
}  // namespace

template<class tvector>
void benchmarkvectorcreationandinsertion(
    string name, size_t ops, size_t item_size,
    const std::vector<typename tvector::value_type>& items) {
  auto env = env::default();

  int index = 0;
  auto start_time = env->nownanos();
  auto ops_remaining = ops;
  while(ops_remaining--) {
    tvector v;
    for (size_t i = 0; i < item_size; ++i) {
      v.push_back(items[index++]);
    }
  }
  auto elapsed = env->nownanos() - start_time;
  cout << "created " << ops << " " << name << " instances:\n\t"
       << "each was inserted with " << item_size << " elements\n\t"
       << "total time elapsed: " << elapsed << " (ns)" << endl;
}

template <class tvector>
size_t benchmarksequenceaccess(string name, size_t ops, size_t elem_size) {
  tvector v;
  for (const auto& item : gettestkeys(elem_size)) {
    v.push_back(item);
  }
  auto env = env::default();

  auto ops_remaining = ops;
  auto start_time = env->nownanos();
  size_t total = 0;
  while (ops_remaining--) {
    auto end = v.end();
    for (auto pos = v.begin(); pos != end; ++pos) {
      total += pos->size();
    }
  }
  auto elapsed = env->nownanos() - start_time;
  cout << "performed " << ops << " sequence access against " << name << "\n\t"
       << "size: " << elem_size << "\n\t"
       << "total time elapsed: " << elapsed << " (ns)" << endl;
  // hack avoid compiler's optimization to ignore total
  return total;
}

// this test case only reports the performance between std::vector<string>
// and autovector<string>. we chose string for comparison because in most
// o our use cases we used std::vector<string>.
test(autovectortest, perfbench) {
  // we run same operations for kops times in order to get a more fair result.
  size_t kops = 100000;

  // creation and insertion test
  // test the case when there is:
  //  * no element inserted: internal array of std::vector may not really get
  //    initialize.
  //  * one element inserted: internal array of std::vector must have
  //    initialized.
  //  * ksize elements inserted. this shows the most time we'll spend if we
  //    keep everything in stack.
  //  * 2 * ksize elements inserted. the internal vector of
  //    autovector must have been initialized.
  cout << "=====================================================" << endl;
  cout << "creation and insertion test (value type: std::string)" << endl;
  cout << "=====================================================" << endl;

  // pre-generated unique keys
  auto string_keys = gettestkeys(kops * 2 * ksize);
  for (auto insertions : { 0ul, 1ul, ksize / 2, ksize, 2 * ksize }) {
    benchmarkvectorcreationandinsertion<vector<string>>(
      "vector<string>", kops, insertions, string_keys
    );
    benchmarkvectorcreationandinsertion<autovector<string, ksize>>(
      "autovector<string>", kops, insertions, string_keys
    );
    cout << "-----------------------------------" << endl;
  }

  cout << "=====================================================" << endl;
  cout << "creation and insertion test (value type: uint64_t)" << endl;
  cout << "=====================================================" << endl;

  // pre-generated unique keys
  vector<uint64_t> int_keys(kops * 2 * ksize);
  for (size_t i = 0; i < kops * 2 * ksize; ++i) {
    int_keys[i] = i;
  }
  for (auto insertions : { 0ul, 1ul, ksize / 2, ksize, 2 * ksize }) {
    benchmarkvectorcreationandinsertion<vector<uint64_t>>(
      "vector<uint64_t>", kops, insertions, int_keys
    );
    benchmarkvectorcreationandinsertion<autovector<uint64_t, ksize>>(
      "autovector<uint64_t>", kops, insertions, int_keys
    );
    cout << "-----------------------------------" << endl;
  }

  // sequence access test
  cout << "=====================================================" << endl;
  cout << "sequence access test" << endl;
  cout << "=====================================================" << endl;
  for (auto elem_size : { ksize / 2, ksize, 2 * ksize }) {
    benchmarksequenceaccess<vector<string>>(
        "vector", kops, elem_size
    );
    benchmarksequenceaccess<autovector<string, ksize>>(
        "autovector", kops, elem_size
    );
    cout << "-----------------------------------" << endl;
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
