//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include <cstdint>
#include <functional>
#include <limits>
#include <vector>
#include "util/arena.h"
#include "util/autovector.h"

namespace rocksdb {

class comparator;
struct filemetadata;
struct fdwithkeyrange;
struct filelevel;

// the file tree structure in version is prebuilt and the range of each file
// is known. on version::get(), it uses binary search to find a potential file
// and then check if a target key can be found in the file by comparing the key
// to each file's smallest and largest key. the results of these comparisions
// can be reused beyond checking if a key falls into a file's range.
// with some pre-calculated knowledge, each key comparision that has been done
// can serve as a hint to narrow down further searches: if a key compared to
// be smaller than a file's smallest or largest, that comparison can be used
// to find out the right bound of next binary search. similarly, if a key
// compared to be larger than a file's smallest or largest, it can be utilized
// to find out the left bound of next binary search.
// with these hints: it can greatly reduce the range of binary search,
// especially for bottom levels, given that one file most likely overlaps with
// only n files from level below (where n is max_bytes_for_level_multiplier).
// so on level l, we will only look at ~n files instead of n^l files on the
// naive approach.
class fileindexer {
 public:
  explicit fileindexer(const comparator* ucmp);

  uint32_t numlevelindex();

  uint32_t levelindexsize(uint32_t level);

  // return a file index range in the next level to search for a key based on
  // smallest and largest key comparision for the current file specified by
  // level and file_index. when *left_index < *right_index, both index should
  // be valid and fit in the vector size.
  void getnextlevelindex(
    const uint32_t level, const uint32_t file_index, const int cmp_smallest,
    const int cmp_largest, int32_t* left_bound, int32_t* right_bound);

  void updateindex(arena* arena, const uint32_t num_levels,
                   std::vector<filemetadata*>* const files);

  enum {
    klevelmaxindex = std::numeric_limits<int32_t>::max()
  };

 private:
  uint32_t num_levels_;
  const comparator* ucmp_;

  struct indexunit {
    indexunit()
      : smallest_lb(0), largest_lb(0), smallest_rb(-1), largest_rb(-1) {}
    // during file search, a key is compared against smallest and largest
    // from a filemetadata. it can have 3 possible outcomes:
    // (1) key is smaller than smallest, implying it is also smaller than
    //     larger. precalculated index based on "smallest < smallest" can
    //     be used to provide right bound.
    // (2) key is in between smallest and largest.
    //     precalculated index based on "smallest > greatest" can be used to
    //     provide left bound.
    //     precalculated index based on "largest < smallest" can be used to
    //     provide right bound.
    // (3) key is larger than largest, implying it is also larger than smallest.
    //     precalculated index based on "largest > largest" can be used to
    //     provide left bound.
    //
    // as a result, we will need to do:
    // compare smallest (<=) and largest keys from upper level file with
    // smallest key from lower level to get a right bound.
    // compare smallest (>=) and largest keys from upper level file with
    // largest key from lower level to get a left bound.
    //
    // example:
    //    level 1:              [50 - 60]
    //    level 2:        [1 - 40], [45 - 55], [58 - 80]
    // a key 35, compared to be less than 50, 3rd file on level 2 can be
    // skipped according to rule (1). lb = 0, rb = 1.
    // a key 53, sits in the middle 50 and 60. 1st file on level 2 can be
    // skipped according to rule (2)-a, but the 3rd file cannot be skipped
    // because 60 is greater than 58. lb = 1, rb = 2.
    // a key 70, compared to be larger than 60. 1st and 2nd file can be skipped
    // according to rule (3). lb = 2, rb = 2.
    //
    // point to a left most file in a lower level that may contain a key,
    // which compares greater than smallest of a filemetadata (upper level)
    int32_t smallest_lb;
    // point to a left most file in a lower level that may contain a key,
    // which compares greater than largest of a filemetadata (upper level)
    int32_t largest_lb;
    // point to a right most file in a lower level that may contain a key,
    // which compares smaller than smallest of a filemetadata (upper level)
    int32_t smallest_rb;
    // point to a right most file in a lower level that may contain a key,
    // which compares smaller than largest of a filemetadata (upper level)
    int32_t largest_rb;
  };

  // data structure to store indexunits in a whole level
  struct indexlevel {
    size_t num_index;
    indexunit* index_units;

    indexlevel() : num_index(0), index_units(nullptr) {}
  };

  void calculatelb(
      const std::vector<filemetadata*>& upper_files,
      const std::vector<filemetadata*>& lower_files, indexlevel* index_level,
      std::function<int(const filemetadata*, const filemetadata*)> cmp_op,
      std::function<void(indexunit*, int32_t)> set_index);

  void calculaterb(
      const std::vector<filemetadata*>& upper_files,
      const std::vector<filemetadata*>& lower_files, indexlevel* index_level,
      std::function<int(const filemetadata*, const filemetadata*)> cmp_op,
      std::function<void(indexunit*, int32_t)> set_index);

  autovector<indexlevel> next_level_index_;
  int32_t* level_rb_;
};

}  // namespace rocksdb
