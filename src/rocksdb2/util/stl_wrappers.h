//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once

#include "util/murmurhash.h"
#include "util/coding.h"

#include "rocksdb/memtablerep.h"
#include "rocksdb/slice.h"

namespace rocksdb {
namespace stl_wrappers {
  class base {
   protected:
    const memtablerep::keycomparator& compare_;
    explicit base(const memtablerep::keycomparator& compare)
      : compare_(compare) { }
  };

  struct compare : private base {
    explicit compare(const memtablerep::keycomparator& compare)
      : base(compare) { }
    inline bool operator()(const char* a, const char* b) const {
      return compare_(a, b) < 0;
    }
  };

}
}
