//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include "rocksdb/types.h"

namespace rocksdb {

class comparator;
class iterator;
class env;
class arena;

// return an iterator that provided the union of the data in
// children[0,n-1].  takes ownership of the child iterators and
// will delete them when the result iterator is deleted.
//
// the result does no duplicate suppression.  i.e., if a particular
// key is present in k child iterators, it will be yielded k times.
//
// requires: n >= 0
extern iterator* newmergingiterator(const comparator* comparator,
                                    iterator** children, int n,
                                    arena* arena = nullptr);

class mergingiterator;

// a builder class to build a merging iterator by adding iterators one by one.
class mergeiteratorbuilder {
 public:
  // comparator: the comparator used in merging comparator
  // arena: where the merging iterator needs to be allocated from.
  explicit mergeiteratorbuilder(const comparator* comparator, arena* arena);
  ~mergeiteratorbuilder() {}

  // add iter to the merging iterator.
  void additerator(iterator* iter);

  // get arena used to build the merging iterator. it is called one a child
  // iterator needs to be allocated.
  arena* getarena() { return arena; }

  // return the result merging iterator.
  iterator* finish();

 private:
  mergingiterator* merge_iter;
  iterator* first_iter;
  bool use_merging_iter;
  arena* arena;
};

}  // namespace rocksdb
