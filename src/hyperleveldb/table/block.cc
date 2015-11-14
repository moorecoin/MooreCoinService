// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// decodes the blocks generated by block_builder.cc.

#include "block.h"

#include <vector>
#include <algorithm>
#include "../hyperleveldb/comparator.h"
#include "format.h"
#include "../util/coding.h"
#include "../util/logging.h"

namespace hyperleveldb {

inline uint32_t block::numrestarts() const {
  assert(size_ >= sizeof(uint32_t));
  return decodefixed32(data_ + size_ - sizeof(uint32_t));
}

block::block(const blockcontents& contents)
    : data_(contents.data.data()),
      size_(contents.data.size()),
      owned_(contents.heap_allocated) {
  if (size_ < sizeof(uint32_t)) {
    size_ = 0;  // error marker
  } else {
    size_t max_restarts_allowed = (size_-sizeof(uint32_t)) / sizeof(uint32_t);
    if (numrestarts() > max_restarts_allowed) {
      // the size is too small for numrestarts()
      size_ = 0;
    } else {
      restart_offset_ = size_ - (1 + numrestarts()) * sizeof(uint32_t);
    }
  }
}

block::~block() {
  if (owned_) {
    delete[] data_;
  }
}

// helper routine: decode the next block entry starting at "p",
// storing the number of shared key bytes, non_shared key bytes,
// and the length of the value in "*shared", "*non_shared", and
// "*value_length", respectively.  will not derefence past "limit".
//
// if any errors are detected, returns null.  otherwise, returns a
// pointer to the key delta (just past the three decoded values).
static inline const char* decodeentry(const char* p, const char* limit,
                                      uint32_t* shared,
                                      uint32_t* non_shared,
                                      uint32_t* value_length) {
  if (limit - p < 3) return null;
  *shared = reinterpret_cast<const unsigned char*>(p)[0];
  *non_shared = reinterpret_cast<const unsigned char*>(p)[1];
  *value_length = reinterpret_cast<const unsigned char*>(p)[2];
  if ((*shared | *non_shared | *value_length) < 128) {
    // fast path: all three values are encoded in one byte each
    p += 3;
  } else {
    if ((p = getvarint32ptr(p, limit, shared)) == null) return null;
    if ((p = getvarint32ptr(p, limit, non_shared)) == null) return null;
    if ((p = getvarint32ptr(p, limit, value_length)) == null) return null;
  }

  if (static_cast<uint32_t>(limit - p) < (*non_shared + *value_length)) {
    return null;
  }
  return p;
}

class block::iter : public iterator {
 private:
  const comparator* const comparator_;
  const char* const data_;      // underlying block contents
  uint32_t const restarts_;     // offset of restart array (list of fixed32)
  uint32_t const num_restarts_; // number of uint32_t entries in restart array

  // current_ is offset in data_ of current entry.  >= restarts_ if !valid
  uint32_t current_;
  uint32_t restart_index_;  // index of restart block in which current_ falls
  std::string key_;
  slice value_;
  status status_;

  inline int compare(const slice& a, const slice& b) const {
    return comparator_->compare(a, b);
  }

  // return the offset in data_ just past the end of the current entry.
  inline uint32_t nextentryoffset() const {
    return (value_.data() + value_.size()) - data_;
  }

  uint32_t getrestartpoint(uint32_t index) {
    assert(index < num_restarts_);
    return decodefixed32(data_ + restarts_ + index * sizeof(uint32_t));
  }

  void seektorestartpoint(uint32_t index) {
    key_.clear();
    restart_index_ = index;
    // current_ will be fixed by parsenextkey();

    // parsenextkey() starts at the end of value_, so set value_ accordingly
    uint32_t offset = getrestartpoint(index);
    value_ = slice(data_ + offset, 0);
  }

 public:
  iter(const comparator* comparator,
       const char* data,
       uint32_t restarts,
       uint32_t num_restarts)
      : comparator_(comparator),
        data_(data),
        restarts_(restarts),
        num_restarts_(num_restarts),
        current_(restarts_),
        restart_index_(num_restarts_) {
    assert(num_restarts_ > 0);
  }

  virtual bool valid() const { return current_ < restarts_; }
  virtual status status() const { return status_; }
  virtual slice key() const {
    assert(valid());
    return key_;
  }
  virtual slice value() const {
    assert(valid());
    return value_;
  }

  virtual void next() {
    assert(valid());
    parsenextkey();
  }

  virtual void prev() {
    assert(valid());

    // scan backwards to a restart point before current_
    const uint32_t original = current_;
    while (getrestartpoint(restart_index_) >= original) {
      if (restart_index_ == 0) {
        // no more entries
        current_ = restarts_;
        restart_index_ = num_restarts_;
        return;
      }
      restart_index_--;
    }

    seektorestartpoint(restart_index_);
    do {
      // loop until end of current entry hits the start of original entry
    } while (parsenextkey() && nextentryoffset() < original);
  }

  virtual void seek(const slice& target) {
    // binary search in restart array to find the last restart point
    // with a key < target
    uint32_t left = 0;
    uint32_t right = num_restarts_ - 1;
    while (left < right) {
      uint32_t mid = (left + right + 1) / 2;
      uint32_t region_offset = getrestartpoint(mid);
      uint32_t shared, non_shared, value_length;
      const char* key_ptr = decodeentry(data_ + region_offset,
                                        data_ + restarts_,
                                        &shared, &non_shared, &value_length);
      if (key_ptr == null || (shared != 0)) {
        corruptionerror();
        return;
      }
      slice mid_key(key_ptr, non_shared);
      if (compare(mid_key, target) < 0) {
        // key at "mid" is smaller than "target".  therefore all
        // blocks before "mid" are uninteresting.
        left = mid;
      } else {
        // key at "mid" is >= "target".  therefore all blocks at or
        // after "mid" are uninteresting.
        right = mid - 1;
      }
    }

    // linear search (within restart block) for first key >= target
    seektorestartpoint(left);
    while (true) {
      if (!parsenextkey()) {
        return;
      }
      if (compare(key_, target) >= 0) {
        return;
      }
    }
  }

  virtual void seektofirst() {
    seektorestartpoint(0);
    parsenextkey();
  }

  virtual void seektolast() {
    seektorestartpoint(num_restarts_ - 1);
    while (parsenextkey() && nextentryoffset() < restarts_) {
      // keep skipping
    }
  }

 private:
  void corruptionerror() {
    current_ = restarts_;
    restart_index_ = num_restarts_;
    status_ = status::corruption("bad entry in block");
    key_.clear();
    value_.clear();
  }

  bool parsenextkey() {
    current_ = nextentryoffset();
    const char* p = data_ + current_;
    const char* limit = data_ + restarts_;  // restarts come right after data
    if (p >= limit) {
      // no more entries to return.  mark as invalid.
      current_ = restarts_;
      restart_index_ = num_restarts_;
      return false;
    }

    // decode next entry
    uint32_t shared, non_shared, value_length;
    p = decodeentry(p, limit, &shared, &non_shared, &value_length);
    if (p == null || key_.size() < shared) {
      corruptionerror();
      return false;
    } else {
      key_.resize(shared);
      key_.append(p, non_shared);
      value_ = slice(p + non_shared, value_length);
      while (restart_index_ + 1 < num_restarts_ &&
             getrestartpoint(restart_index_ + 1) < current_) {
        ++restart_index_;
      }
      return true;
    }
  }
};

iterator* block::newiterator(const comparator* cmp) {
  if (size_ < sizeof(uint32_t)) {
    return newerroriterator(status::corruption("bad block contents"));
  }
  const uint32_t num_restarts = numrestarts();
  if (num_restarts == 0) {
    return newemptyiterator();
  } else {
    return new iter(cmp, data_, restart_offset_, num_restarts);
  }
}

}  // namespace hyperleveldb
