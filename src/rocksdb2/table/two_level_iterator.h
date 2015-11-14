//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "rocksdb/iterator.h"
#include "rocksdb/env.h"
#include "table/iterator_wrapper.h"

namespace rocksdb {

struct readoptions;
class internalkeycomparator;
class arena;

struct twoleveliteratorstate {
  explicit twoleveliteratorstate(bool check_prefix_may_match)
    : check_prefix_may_match(check_prefix_may_match) {}

  virtual ~twoleveliteratorstate() {}
  virtual iterator* newsecondaryiterator(const slice& handle) = 0;
  virtual bool prefixmaymatch(const slice& internal_key) = 0;

  // if call prefixmaymatch()
  bool check_prefix_may_match;
};


// return a new two level iterator.  a two-level iterator contains an
// index iterator whose values point to a sequence of blocks where
// each block is itself a sequence of key,value pairs.  the returned
// two-level iterator yields the concatenation of all key/value pairs
// in the sequence of blocks.  takes ownership of "index_iter" and
// will delete it when no longer needed.
//
// uses a supplied function to convert an index_iter value into
// an iterator over the contents of the corresponding block.
// arena: if not null, the arena is used to allocate the iterator.
//        when destroying the iterator, the destructor will destroy
//        all the states but those allocated in arena.
extern iterator* newtwoleveliterator(twoleveliteratorstate* state,
                                     iterator* first_level_iter,
                                     arena* arena = nullptr);

}  // namespace rocksdb
