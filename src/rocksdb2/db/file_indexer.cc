//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/file_indexer.h"
#include <algorithm>
#include "rocksdb/comparator.h"
#include "db/version_edit.h"

namespace rocksdb {

fileindexer::fileindexer(const comparator* ucmp)
    : num_levels_(0), ucmp_(ucmp), level_rb_(nullptr) {}

uint32_t fileindexer::numlevelindex() {
  return next_level_index_.size();
}

uint32_t fileindexer::levelindexsize(uint32_t level) {
  return next_level_index_[level].num_index;
}

void fileindexer::getnextlevelindex(
    const uint32_t level, const uint32_t file_index, const int cmp_smallest,
    const int cmp_largest, int32_t* left_bound, int32_t* right_bound) {
  assert(level > 0);

  // last level, no hint
  if (level == num_levels_ - 1) {
    *left_bound = 0;
    *right_bound = -1;
    return;
  }

  assert(level < num_levels_ - 1);
  assert(static_cast<int32_t>(file_index) <= level_rb_[level]);

  const indexunit* index_units = next_level_index_[level].index_units;
  const auto& index = index_units[file_index];

  if (cmp_smallest < 0) {
    *left_bound = (level > 0 && file_index > 0)
                      ? index_units[file_index - 1].largest_lb
                      : 0;
    *right_bound = index.smallest_rb;
  } else if (cmp_smallest == 0) {
    *left_bound = index.smallest_lb;
    *right_bound = index.smallest_rb;
  } else if (cmp_smallest > 0 && cmp_largest < 0) {
    *left_bound = index.smallest_lb;
    *right_bound = index.largest_rb;
  } else if (cmp_largest == 0) {
    *left_bound = index.largest_lb;
    *right_bound = index.largest_rb;
  } else if (cmp_largest > 0) {
    *left_bound = index.largest_lb;
    *right_bound = level_rb_[level + 1];
  } else {
    assert(false);
  }

  assert(*left_bound >= 0);
  assert(*left_bound <= *right_bound + 1);
  assert(*right_bound <= level_rb_[level + 1]);
}

void fileindexer::updateindex(arena* arena, const uint32_t num_levels,
                              std::vector<filemetadata*>* const files) {
  if (files == nullptr) {
    return;
  }
  if (num_levels == 0) {  // uint_32 0-1 would cause bad behavior
    num_levels_ = num_levels;
    return;
  }
  assert(level_rb_ == nullptr);  // level_rb_ should be init here

  num_levels_ = num_levels;
  next_level_index_.resize(num_levels);

  char* mem = arena->allocatealigned(num_levels_ * sizeof(int32_t));
  level_rb_ = new (mem) int32_t[num_levels_];
  for (size_t i = 0; i < num_levels_; i++) {
    level_rb_[i] = -1;
  }

  // l1 - ln-1
  for (uint32_t level = 1; level < num_levels_ - 1; ++level) {
    const auto& upper_files = files[level];
    const int32_t upper_size = upper_files.size();
    const auto& lower_files = files[level + 1];
    level_rb_[level] = upper_files.size() - 1;
    if (upper_size == 0) {
      continue;
    }
    indexlevel& index_level = next_level_index_[level];
    index_level.num_index = upper_size;
    char* mem = arena->allocatealigned(upper_size * sizeof(indexunit));
    index_level.index_units = new (mem) indexunit[upper_size];

    calculatelb(
        upper_files, lower_files, &index_level,
        [this](const filemetadata * a, const filemetadata * b)->int {
          return ucmp_->compare(a->smallest.user_key(), b->largest.user_key());
        },
        [](indexunit* index, int32_t f_idx) { index->smallest_lb = f_idx; });
    calculatelb(
        upper_files, lower_files, &index_level,
        [this](const filemetadata * a, const filemetadata * b)->int {
          return ucmp_->compare(a->largest.user_key(), b->largest.user_key());
        },
        [](indexunit* index, int32_t f_idx) { index->largest_lb = f_idx; });
    calculaterb(
        upper_files, lower_files, &index_level,
        [this](const filemetadata * a, const filemetadata * b)->int {
          return ucmp_->compare(a->smallest.user_key(), b->smallest.user_key());
        },
        [](indexunit* index, int32_t f_idx) { index->smallest_rb = f_idx; });
    calculaterb(
        upper_files, lower_files, &index_level,
        [this](const filemetadata * a, const filemetadata * b)->int {
          return ucmp_->compare(a->largest.user_key(), b->smallest.user_key());
        },
        [](indexunit* index, int32_t f_idx) { index->largest_rb = f_idx; });
  }

  level_rb_[num_levels_ - 1] = files[num_levels_ - 1].size() - 1;
}

void fileindexer::calculatelb(
    const std::vector<filemetadata*>& upper_files,
    const std::vector<filemetadata*>& lower_files, indexlevel* index_level,
    std::function<int(const filemetadata*, const filemetadata*)> cmp_op,
    std::function<void(indexunit*, int32_t)> set_index) {
  const int32_t upper_size = upper_files.size();
  const int32_t lower_size = lower_files.size();
  int32_t upper_idx = 0;
  int32_t lower_idx = 0;

  indexunit* index = index_level->index_units;
  while (upper_idx < upper_size && lower_idx < lower_size) {
    int cmp = cmp_op(upper_files[upper_idx], lower_files[lower_idx]);

    if (cmp == 0) {
      set_index(&index[upper_idx], lower_idx);
      ++upper_idx;
      ++lower_idx;
    } else if (cmp > 0) {
      // lower level's file (largest) is smaller, a key won't hit in that
      // file. move to next lower file
      ++lower_idx;
    } else {
      // lower level's file becomes larger, update the index, and
      // move to the next upper file
      set_index(&index[upper_idx], lower_idx);
      ++upper_idx;
    }
  }

  while (upper_idx < upper_size) {
    // lower files are exhausted, that means the remaining upper files are
    // greater than any lower files. set the index to be the lower level size.
    set_index(&index[upper_idx], lower_size);
    ++upper_idx;
  }
}

void fileindexer::calculaterb(
    const std::vector<filemetadata*>& upper_files,
    const std::vector<filemetadata*>& lower_files, indexlevel* index_level,
    std::function<int(const filemetadata*, const filemetadata*)> cmp_op,
    std::function<void(indexunit*, int32_t)> set_index) {
  const int32_t upper_size = upper_files.size();
  const int32_t lower_size = lower_files.size();
  int32_t upper_idx = upper_size - 1;
  int32_t lower_idx = lower_size - 1;

  indexunit* index = index_level->index_units;
  while (upper_idx >= 0 && lower_idx >= 0) {
    int cmp = cmp_op(upper_files[upper_idx], lower_files[lower_idx]);

    if (cmp == 0) {
      set_index(&index[upper_idx], lower_idx);
      --upper_idx;
      --lower_idx;
    } else if (cmp < 0) {
      // lower level's file (smallest) is larger, a key won't hit in that
      // file. move to next lower file.
      --lower_idx;
    } else {
      // lower level's file becomes smaller, update the index, and move to
      // the next the upper file
      set_index(&index[upper_idx], lower_idx);
      --upper_idx;
    }
  }
  while (upper_idx >= 0) {
    // lower files are exhausted, that means the remaining upper files are
    // smaller than any lower files. set it to -1.
    set_index(&index[upper_idx], -1);
    --upper_idx;
  }
}

}  // namespace rocksdb
