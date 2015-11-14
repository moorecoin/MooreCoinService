//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#pragma once
#include <queue>

#include "rocksdb/comparator.h"
#include "table/iterator_wrapper.h"

namespace rocksdb {

// return the max of two keys.
class maxiteratorcomparator {
 public:
  maxiteratorcomparator(const comparator* comparator) :
    comparator_(comparator) {}

  bool operator()(iteratorwrapper* a, iteratorwrapper* b) {
    return comparator_->compare(a->key(), b->key()) <= 0;
  }
 private:
  const comparator* comparator_;
};

// return the max of two keys.
class miniteratorcomparator {
 public:
  // if maxheap is set comparator returns the max value.
  // else returns the min value.
  // can use to create a minheap or a maxheap.
  miniteratorcomparator(const comparator* comparator) :
    comparator_(comparator) {}

  bool operator()(iteratorwrapper* a, iteratorwrapper* b) {
    return comparator_->compare(a->key(), b->key()) > 0;
  }
 private:
  const comparator* comparator_;
};

}  // namespace rocksdb
