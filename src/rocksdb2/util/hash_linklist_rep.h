// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef rocksdb_lite
#pragma once
#include "rocksdb/slice_transform.h"
#include "rocksdb/memtablerep.h"

namespace rocksdb {

class hashlinklistrepfactory : public memtablerepfactory {
 public:
  explicit hashlinklistrepfactory(size_t bucket_count,
                                  uint32_t threshold_use_skiplist,
                                  size_t huge_page_tlb_size,
                                  int bucket_entries_logging_threshold,
                                  bool if_log_bucket_dist_when_flash)
      : bucket_count_(bucket_count),
        threshold_use_skiplist_(threshold_use_skiplist),
        huge_page_tlb_size_(huge_page_tlb_size),
        bucket_entries_logging_threshold_(bucket_entries_logging_threshold),
        if_log_bucket_dist_when_flash_(if_log_bucket_dist_when_flash) {}

  virtual ~hashlinklistrepfactory() {}

  virtual memtablerep* creatememtablerep(
      const memtablerep::keycomparator& compare, arena* arena,
      const slicetransform* transform, logger* logger) override;

  virtual const char* name() const override {
    return "hashlinklistrepfactory";
  }

 private:
  const size_t bucket_count_;
  const uint32_t threshold_use_skiplist_;
  const size_t huge_page_tlb_size_;
  int bucket_entries_logging_threshold_;
  bool if_log_bucket_dist_when_flash_;
};

}
#endif  // rocksdb_lite
