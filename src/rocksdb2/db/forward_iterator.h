//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once

#ifndef rocksdb_lite

#include <string>
#include <vector>
#include <queue>

#include "rocksdb/db.h"
#include "rocksdb/iterator.h"
#include "rocksdb/options.h"
#include "db/dbformat.h"

namespace rocksdb {

class dbimpl;
class env;
struct superversion;
class columnfamilydata;
class leveliterator;
struct filemetadata;

class minitercomparator {
 public:
  explicit minitercomparator(const comparator* comparator) :
    comparator_(comparator) {}

  bool operator()(iterator* a, iterator* b) {
    return comparator_->compare(a->key(), b->key()) > 0;
  }
 private:
  const comparator* comparator_;
};

typedef std::priority_queue<iterator*,
          std::vector<iterator*>,
          minitercomparator> miniterheap;

/**
 * forwarditerator is a special type of iterator that only supports seek()
 * and next(). it is expected to perform better than tailingiterator by
 * removing the encapsulation and making all information accessible within
 * the iterator. at the current implementation, snapshot is taken at the
 * time seek() is called. the next() followed do not see new values after.
 */
class forwarditerator : public iterator {
 public:
  forwarditerator(dbimpl* db, const readoptions& read_options,
                  columnfamilydata* cfd);
  virtual ~forwarditerator();

  void seektolast() override {
    status_ = status::notsupported("forwarditerator::seektolast()");
    valid_ = false;
  }
  void prev() {
    status_ = status::notsupported("forwarditerator::prev");
    valid_ = false;
  }

  virtual bool valid() const override;
  void seektofirst() override;
  virtual void seek(const slice& target) override;
  virtual void next() override;
  virtual slice key() const override;
  virtual slice value() const override;
  virtual status status() const override;

 private:
  void cleanup();
  void rebuilditerators();
  void resetincompleteiterators();
  void seekinternal(const slice& internal_key, bool seek_to_first);
  void updatecurrent();
  bool needtoseekimmutable(const slice& internal_key);
  uint32_t findfileinrange(
    const std::vector<filemetadata*>& files, const slice& internal_key,
    uint32_t left, uint32_t right);

  dbimpl* const db_;
  const readoptions read_options_;
  columnfamilydata* const cfd_;
  const slicetransform* const prefix_extractor_;
  const comparator* user_comparator_;
  miniterheap immutable_min_heap_;

  superversion* sv_;
  iterator* mutable_iter_;
  std::vector<iterator*> imm_iters_;
  std::vector<iterator*> l0_iters_;
  std::vector<leveliterator*> level_iters_;
  iterator* current_;
  // internal iterator status
  status status_;
  bool valid_;

  iterkey prev_key_;
  bool is_prev_set_;
};

}  // namespace rocksdb
#endif  // rocksdb_lite
