//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/cache.h"

#include <vector>
#include <string>
#include <iostream>
#include "util/coding.h"
#include "util/testharness.h"

namespace rocksdb {

// conversions between numeric keys/values and the types expected by cache.
static std::string encodekey(int k) {
  std::string result;
  putfixed32(&result, k);
  return result;
}
static int decodekey(const slice& k) {
  assert(k.size() == 4);
  return decodefixed32(k.data());
}
static void* encodevalue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int decodevalue(void* v) { return reinterpret_cast<uintptr_t>(v); }

class cachetest {
 public:
  static cachetest* current_;

  static void deleter(const slice& key, void* v) {
    current_->deleted_keys_.push_back(decodekey(key));
    current_->deleted_values_.push_back(decodevalue(v));
  }

  static const int kcachesize = 1000;
  static const int knumshardbits = 4;
  static const int kremovescancountlimit = 16;

  static const int kcachesize2 = 100;
  static const int knumshardbits2 = 2;
  static const int kremovescancountlimit2 = 200;

  std::vector<int> deleted_keys_;
  std::vector<int> deleted_values_;
  shared_ptr<cache> cache_;
  shared_ptr<cache> cache2_;

  cachetest() :
      cache_(newlrucache(kcachesize, knumshardbits, kremovescancountlimit)),
      cache2_(newlrucache(kcachesize2, knumshardbits2,
                          kremovescancountlimit2)) {
    current_ = this;
  }

  ~cachetest() {
  }

  int lookup(shared_ptr<cache> cache, int key) {
    cache::handle* handle = cache->lookup(encodekey(key));
    const int r = (handle == nullptr) ? -1 : decodevalue(cache->value(handle));
    if (handle != nullptr) {
      cache->release(handle);
    }
    return r;
  }

  void insert(shared_ptr<cache> cache, int key, int value, int charge = 1) {
    cache->release(cache->insert(encodekey(key), encodevalue(value), charge,
                                  &cachetest::deleter));
  }

  void erase(shared_ptr<cache> cache, int key) {
    cache->erase(encodekey(key));
  }


  int lookup(int key) {
    return lookup(cache_, key);
  }

  void insert(int key, int value, int charge = 1) {
    insert(cache_, key, value, charge);
  }

  void erase(int key) {
    erase(cache_, key);
  }

  int lookup2(int key) {
    return lookup(cache2_, key);
  }

  void insert2(int key, int value, int charge = 1) {
    insert(cache2_, key, value, charge);
  }

  void erase2(int key) {
    erase(cache2_, key);
  }
};
cachetest* cachetest::current_;

namespace {
void dumbdeleter(const slice& key, void* value) { }
}  // namespace

test(cachetest, usagetest) {
  // cache is shared_ptr and will be automatically cleaned up.
  const uint64_t kcapacity = 100000;
  auto cache = newlrucache(kcapacity, 8, 200);

  size_t usage = 0;
  const char* value = "abcdef";
  // make sure everything will be cached
  for (int i = 1; i < 100; ++i) {
    std::string key(i, 'a');
    auto kv_size = key.size() + 5;
    cache->release(
        cache->insert(key, (void*)value, kv_size, dumbdeleter)
    );
    usage += kv_size;
    assert_eq(usage, cache->getusage());
  }

  // make sure the cache will be overloaded
  for (uint64_t i = 1; i < kcapacity; ++i) {
    auto key = std::to_string(i);
    cache->release(
        cache->insert(key, (void*)value, key.size() + 5, dumbdeleter)
    );
  }

  // the usage should be close to the capacity
  assert_gt(kcapacity, cache->getusage());
  assert_lt(kcapacity * 0.95, cache->getusage());
}

test(cachetest, hitandmiss) {
  assert_eq(-1, lookup(100));

  insert(100, 101);
  assert_eq(101, lookup(100));
  assert_eq(-1,  lookup(200));
  assert_eq(-1,  lookup(300));

  insert(200, 201);
  assert_eq(101, lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(-1,  lookup(300));

  insert(100, 102);
  assert_eq(102, lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(-1,  lookup(300));

  assert_eq(1u, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);
}

test(cachetest, erase) {
  erase(200);
  assert_eq(0u, deleted_keys_.size());

  insert(100, 101);
  insert(200, 201);
  erase(100);
  assert_eq(-1,  lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(1u, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);

  erase(100);
  assert_eq(-1,  lookup(100));
  assert_eq(201, lookup(200));
  assert_eq(1u, deleted_keys_.size());
}

test(cachetest, entriesarepinned) {
  insert(100, 101);
  cache::handle* h1 = cache_->lookup(encodekey(100));
  assert_eq(101, decodevalue(cache_->value(h1)));

  insert(100, 102);
  cache::handle* h2 = cache_->lookup(encodekey(100));
  assert_eq(102, decodevalue(cache_->value(h2)));
  assert_eq(0u, deleted_keys_.size());

  cache_->release(h1);
  assert_eq(1u, deleted_keys_.size());
  assert_eq(100, deleted_keys_[0]);
  assert_eq(101, deleted_values_[0]);

  erase(100);
  assert_eq(-1, lookup(100));
  assert_eq(1u, deleted_keys_.size());

  cache_->release(h2);
  assert_eq(2u, deleted_keys_.size());
  assert_eq(100, deleted_keys_[1]);
  assert_eq(102, deleted_values_[1]);
}

test(cachetest, evictionpolicy) {
  insert(100, 101);
  insert(200, 201);

  // frequently used entry must be kept around
  for (int i = 0; i < kcachesize + 100; i++) {
    insert(1000+i, 2000+i);
    assert_eq(2000+i, lookup(1000+i));
    assert_eq(101, lookup(100));
  }
  assert_eq(101, lookup(100));
  assert_eq(-1, lookup(200));
}

test(cachetest, evictionpolicyref) {
  insert(100, 101);
  insert(101, 102);
  insert(102, 103);
  insert(103, 104);
  insert(200, 101);
  insert(201, 102);
  insert(202, 103);
  insert(203, 104);
  cache::handle* h201 = cache_->lookup(encodekey(200));
  cache::handle* h202 = cache_->lookup(encodekey(201));
  cache::handle* h203 = cache_->lookup(encodekey(202));
  cache::handle* h204 = cache_->lookup(encodekey(203));
  insert(300, 101);
  insert(301, 102);
  insert(302, 103);
  insert(303, 104);

  // insert entries much more than cache capacity
  for (int i = 0; i < kcachesize + 100; i++) {
    insert(1000 + i, 2000 + i);
  }

  // check whether the entries inserted in the beginning
  // are evicted. ones without extra ref are evicted and
  // those with are not.
  assert_eq(-1, lookup(100));
  assert_eq(-1, lookup(101));
  assert_eq(-1, lookup(102));
  assert_eq(-1, lookup(103));

  assert_eq(-1, lookup(300));
  assert_eq(-1, lookup(301));
  assert_eq(-1, lookup(302));
  assert_eq(-1, lookup(303));

  assert_eq(101, lookup(200));
  assert_eq(102, lookup(201));
  assert_eq(103, lookup(202));
  assert_eq(104, lookup(203));

  // cleaning up all the handles
  cache_->release(h201);
  cache_->release(h202);
  cache_->release(h203);
  cache_->release(h204);
}

test(cachetest, evictionpolicyref2) {
  std::vector<cache::handle*> handles;

  insert(100, 101);
  // insert entries much more than cache capacity
  for (int i = 0; i < kcachesize + 100; i++) {
    insert(1000 + i, 2000 + i);
    if (i < kcachesize ) {
      handles.push_back(cache_->lookup(encodekey(1000 + i)));
    }
  }

  // make sure referenced keys are also possible to be deleted
  // if there are not sufficient non-referenced keys
  for (int i = 0; i < 5; i++) {
    assert_eq(-1, lookup(1000 + i));
  }

  for (int i = kcachesize; i < kcachesize + 100; i++) {
    assert_eq(2000 + i, lookup(1000 + i));
  }
  assert_eq(-1, lookup(100));

  // cleaning up all the handles
  while (handles.size() > 0) {
    cache_->release(handles.back());
    handles.pop_back();
  }
}

test(cachetest, evictionpolicyreflargescanlimit) {
  std::vector<cache::handle*> handles2;

  // cache2 has a cache removescancountlimit higher than cache size
  // so it would trigger a boundary condition.

  // populate the cache with 10 more keys than its size.
  // reference all keys except one close to the end.
  for (int i = 0; i < kcachesize2 + 10; i++) {
    insert2(1000 + i, 2000+i);
    if (i != kcachesize2 ) {
      handles2.push_back(cache2_->lookup(encodekey(1000 + i)));
    }
  }

  // make sure referenced keys are also possible to be deleted
  // if there are not sufficient non-referenced keys
  for (int i = 0; i < 3; i++) {
    assert_eq(-1, lookup2(1000 + i));
  }
  // the non-referenced value is deleted even if it's accessed
  // recently.
  assert_eq(-1, lookup2(1000 + kcachesize2));
  // other values recently accessed are not deleted since they
  // are referenced.
  for (int i = kcachesize2 - 10; i < kcachesize2 + 10; i++) {
    if (i != kcachesize2) {
      assert_eq(2000 + i, lookup2(1000 + i));
    }
  }

  // cleaning up all the handles
  while (handles2.size() > 0) {
    cache2_->release(handles2.back());
    handles2.pop_back();
  }
}



test(cachetest, heavyentries) {
  // add a bunch of light and heavy entries and then count the combined
  // size of items still in the cache, which must be approximately the
  // same as the total capacity.
  const int klight = 1;
  const int kheavy = 10;
  int added = 0;
  int index = 0;
  while (added < 2*kcachesize) {
    const int weight = (index & 1) ? klight : kheavy;
    insert(index, 1000+index, weight);
    added += weight;
    index++;
  }

  int cached_weight = 0;
  for (int i = 0; i < index; i++) {
    const int weight = (i & 1 ? klight : kheavy);
    int r = lookup(i);
    if (r >= 0) {
      cached_weight += weight;
      assert_eq(1000+i, r);
    }
  }
  assert_le(cached_weight, kcachesize + kcachesize/10);
}

test(cachetest, newid) {
  uint64_t a = cache_->newid();
  uint64_t b = cache_->newid();
  assert_ne(a, b);
}


class value {
 private:
  int v_;
 public:
  explicit value(int v) : v_(v) { }

  ~value() { std::cout << v_ << " is destructed\n"; }
};

namespace {
void deleter(const slice& key, void* value) {
  delete (value *)value;
}
}  // namespace

test(cachetest, badeviction) {
  int n = 10;

  // a lrucache with n entries and one shard only
  std::shared_ptr<cache> cache = newlrucache(n, 0);

  std::vector<cache::handle*> handles(n+1);

  // insert n+1 entries, but not releasing.
  for (int i = 0; i < n+1; i++) {
    std::string key = std::to_string(i+1);
    handles[i] = cache->insert(key, new value(i+1), 1, &deleter);
  }

  // guess what's in the cache now?
  for (int i = 0; i < n+1; i++) {
    std::string key = std::to_string(i+1);
    auto h = cache->lookup(key);
    std::cout << key << (h?" found\n":" not found\n");
    // only the first entry should be missing
    assert_true(h || i == 0);
    if (h) cache->release(h);
  }

  for (int i = 0; i < n+1; i++) {
    cache->release(handles[i]);
  }
  std::cout << "poor entries\n";
}

namespace {
std::vector<std::pair<int, int>> callback_state;
void callback(void* entry, size_t charge) {
  callback_state.push_back({decodevalue(entry), static_cast<int>(charge)});
}
};

test(cachetest, applytoallcacheentirestest) {
  std::vector<std::pair<int, int>> inserted;
  callback_state.clear();

  for (int i = 0; i < 10; ++i) {
    insert(i, i * 2, i + 1);
    inserted.push_back({i * 2, i + 1});
  }
  cache_->applytoallcacheentries(callback, true);

  sort(inserted.begin(), inserted.end());
  sort(callback_state.begin(), callback_state.end());
  assert_true(inserted == callback_state);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
