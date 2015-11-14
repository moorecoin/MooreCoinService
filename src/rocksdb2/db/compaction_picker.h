//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "db/version_set.h"
#include "db/compaction.h"
#include "rocksdb/status.h"
#include "rocksdb/options.h"
#include "rocksdb/env.h"

#include <vector>
#include <memory>
#include <set>

namespace rocksdb {

class logbuffer;
class compaction;
class version;

class compactionpicker {
 public:
  compactionpicker(const options* options, const internalkeycomparator* icmp);
  virtual ~compactionpicker();

  // pick level and inputs for a new compaction.
  // returns nullptr if there is no compaction to be done.
  // otherwise returns a pointer to a heap-allocated object that
  // describes the compaction.  caller should delete the result.
  virtual compaction* pickcompaction(version* version,
                                     logbuffer* log_buffer) = 0;

  // return a compaction object for compacting the range [begin,end] in
  // the specified level.  returns nullptr if there is nothing in that
  // level that overlaps the specified range.  caller should delete
  // the result.
  //
  // the returned compaction might not include the whole requested range.
  // in that case, compaction_end will be set to the next key that needs
  // compacting. in case the compaction will compact the whole range,
  // compaction_end will be set to nullptr.
  // client is responsible for compaction_end storage -- when called,
  // *compaction_end should point to valid internalkey!
  virtual compaction* compactrange(version* version, int input_level,
                                   int output_level, uint32_t output_path_id,
                                   const internalkey* begin,
                                   const internalkey* end,
                                   internalkey** compaction_end);

  // given the current number of levels, returns the lowest allowed level
  // for compaction input.
  virtual int maxinputlevel(int current_num_levels) const = 0;

  // free up the files that participated in a compaction
  void releasecompactionfiles(compaction* c, status status);

  // return the total amount of data that is undergoing
  // compactions per level
  void sizebeingcompacted(std::vector<uint64_t>& sizes);

  // returns maximum total overlap bytes with grandparent
  // level (i.e., level+2) before we stop building a single
  // file in level->level+1 compaction.
  uint64_t maxgrandparentoverlapbytes(int level);

  // returns maximum total bytes of data on a given level.
  double maxbytesforlevel(int level);

  // get the max file size in a given level.
  uint64_t maxfilesizeforlevel(int level) const;

 protected:
  int numberlevels() const { return num_levels_; }

  // stores the minimal range that covers all entries in inputs in
  // *smallest, *largest.
  // requires: inputs is not empty
  void getrange(const std::vector<filemetadata*>& inputs, internalkey* smallest,
                internalkey* largest);

  // stores the minimal range that covers all entries in inputs1 and inputs2
  // in *smallest, *largest.
  // requires: inputs is not empty
  void getrange(const std::vector<filemetadata*>& inputs1,
                const std::vector<filemetadata*>& inputs2,
                internalkey* smallest, internalkey* largest);

  // add more files to the inputs on "level" to make sure that
  // no newer version of a key is compacted to "level+1" while leaving an older
  // version in a "level". otherwise, any get() will search "level" first,
  // and will likely return an old/stale value for the key, since it always
  // searches in increasing order of level to find the value. this could
  // also scramble the order of merge operands. this function should be
  // called any time a new compaction is created, and its inputs_[0] are
  // populated.
  //
  // will return false if it is impossible to apply this compaction.
  bool expandwhileoverlapping(compaction* c);

  uint64_t expandedcompactionbytesizelimit(int level);

  // returns true if any one of the specified files are being compacted
  bool filesincompaction(std::vector<filemetadata*>& files);

  // returns true if any one of the parent files are being compacted
  bool parentrangeincompaction(version* version, const internalkey* smallest,
                               const internalkey* largest, int level,
                               int* index);

  void setupotherinputs(compaction* c);

  // record all the ongoing compactions for all levels
  std::vector<std::set<compaction*>> compactions_in_progress_;

  // per-level target file size.
  std::unique_ptr<uint64_t[]> max_file_size_;

  // per-level max bytes
  std::unique_ptr<uint64_t[]> level_max_bytes_;

  const options* const options_;

 private:
  int num_levels_;

  const internalkeycomparator* const icmp_;
};

class universalcompactionpicker : public compactionpicker {
 public:
  universalcompactionpicker(const options* options,
                            const internalkeycomparator* icmp)
      : compactionpicker(options, icmp) {}
  virtual compaction* pickcompaction(version* version,
                                     logbuffer* log_buffer) override;

  // the maxinum allowed input level.  always return 0.
  virtual int maxinputlevel(int current_num_levels) const override {
    return 0;
  }

 private:
  // pick universal compaction to limit read amplification
  compaction* pickcompactionuniversalreadamp(version* version, double score,
                                             unsigned int ratio,
                                             unsigned int num_files,
                                             logbuffer* log_buffer);

  // pick universal compaction to limit space amplification.
  compaction* pickcompactionuniversalsizeamp(version* version, double score,
                                             logbuffer* log_buffer);

  // pick a path id to place a newly generated file, with its estimated file
  // size.
  static uint32_t getpathid(const options& options, uint64_t file_size);
};

class levelcompactionpicker : public compactionpicker {
 public:
  levelcompactionpicker(const options* options,
                        const internalkeycomparator* icmp)
      : compactionpicker(options, icmp) {}
  virtual compaction* pickcompaction(version* version,
                                     logbuffer* log_buffer) override;

  // returns current_num_levels - 2, meaning the last level cannot be
  // compaction input level.
  virtual int maxinputlevel(int current_num_levels) const override {
    return current_num_levels - 2;
  }

 private:
  // for the specfied level, pick a compaction.
  // returns nullptr if there is no compaction to be done.
  // if level is 0 and there is already a compaction on that level, this
  // function will return nullptr.
  compaction* pickcompactionbysize(version* version, int level, double score);
};

class fifocompactionpicker : public compactionpicker {
 public:
  fifocompactionpicker(const options* options,
                       const internalkeycomparator* icmp)
      : compactionpicker(options, icmp) {}

  virtual compaction* pickcompaction(version* version,
                                     logbuffer* log_buffer) override;

  virtual compaction* compactrange(version* version, int input_level,
                                   int output_level, uint32_t output_path_id,
                                   const internalkey* begin,
                                   const internalkey* end,
                                   internalkey** compaction_end) override;

  // the maxinum allowed input level.  always return 0.
  virtual int maxinputlevel(int current_num_levels) const override {
    return 0;
  }
};

// utility function
extern uint64_t totalcompensatedfilesize(const std::vector<filemetadata*>& files);

}  // namespace rocksdb
