//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2012 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/filter_policy.h"

#include "rocksdb/slice.h"
#include "util/hash.h"

namespace rocksdb {

namespace {

class bloomfilterpolicy : public filterpolicy {
 private:
  size_t bits_per_key_;
  size_t k_;
  uint32_t (*hash_func_)(const slice& key);

  void initialize() {
    // we intentionally round down to reduce probing cost a little bit
    k_ = static_cast<size_t>(bits_per_key_ * 0.69);  // 0.69 =~ ln(2)
    if (k_ < 1) k_ = 1;
    if (k_ > 30) k_ = 30;
  }

 public:
  explicit bloomfilterpolicy(int bits_per_key,
                             uint32_t (*hash_func)(const slice& key))
      : bits_per_key_(bits_per_key), hash_func_(hash_func) {
    initialize();
  }
  explicit bloomfilterpolicy(int bits_per_key)
      : bits_per_key_(bits_per_key) {
    hash_func_ = bloomhash;
    initialize();
  }

  virtual const char* name() const {
    return "rocksdb.builtinbloomfilter";
  }

  virtual void createfilter(const slice* keys, int n, std::string* dst) const {
    // compute bloom filter size (in both bits and bytes)
    size_t bits = n * bits_per_key_;

    // for small n, we can see a very high false positive rate.  fix it
    // by enforcing a minimum bloom filter length.
    if (bits < 64) bits = 64;

    size_t bytes = (bits + 7) / 8;
    bits = bytes * 8;

    const size_t init_size = dst->size();
    dst->resize(init_size + bytes, 0);
    dst->push_back(static_cast<char>(k_));  // remember # of probes in filter
    char* array = &(*dst)[init_size];
    for (size_t i = 0; i < (size_t)n; i++) {
      // use double-hashing to generate a sequence of hash values.
      // see analysis in [kirsch,mitzenmacher 2006].
      uint32_t h = hash_func_(keys[i]);
      const uint32_t delta = (h >> 17) | (h << 15);  // rotate right 17 bits
      for (size_t j = 0; j < k_; j++) {
        const uint32_t bitpos = h % bits;
        array[bitpos/8] |= (1 << (bitpos % 8));
        h += delta;
      }
    }
  }

  virtual bool keymaymatch(const slice& key, const slice& bloom_filter) const {
    const size_t len = bloom_filter.size();
    if (len < 2) return false;

    const char* array = bloom_filter.data();
    const size_t bits = (len - 1) * 8;

    // use the encoded k so that we can read filters generated by
    // bloom filters created using different parameters.
    const size_t k = array[len-1];
    if (k > 30) {
      // reserved for potentially new encodings for short bloom filters.
      // consider it a match.
      return true;
    }

    uint32_t h = hash_func_(key);
    const uint32_t delta = (h >> 17) | (h << 15);  // rotate right 17 bits
    for (size_t j = 0; j < k; j++) {
      const uint32_t bitpos = h % bits;
      if ((array[bitpos/8] & (1 << (bitpos % 8))) == 0) return false;
      h += delta;
    }
    return true;
  }
};
}

const filterpolicy* newbloomfilterpolicy(int bits_per_key) {
  return new bloomfilterpolicy(bits_per_key);
}

}  // namespace rocksdb