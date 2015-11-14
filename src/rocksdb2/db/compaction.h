//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "util/arena.h"
#include "util/autovector.h"
#include "db/version_set.h"

namespace rocksdb {

// the structure that manages compaction input files associated
// with the same physical level.
struct compactioninputfiles {
  int level;
  std::vector<filemetadata*> files;
  inline bool empty() const { return files.empty(); }
  inline size_t size() const { return files.size(); }
  inline void clear() { files.clear(); }
  inline filemetadata* operator[](int i) const { return files[i]; }
};

class version;
class columnfamilydata;

// a compaction encapsulates information about a compaction.
class compaction {
 public:
  // no copying allowed
  compaction(const compaction&) = delete;
  void operator=(const compaction&) = delete;

  ~compaction();

  // returns the level associated to the specified compaction input level.
  // if compaction_input_level is not specified, then input_level is set to 0.
  int level(int compaction_input_level = 0) const {
    return inputs_[compaction_input_level].level;
  }

  // outputs will go to this level
  int output_level() const { return output_level_; }

  // returns the number of input levels in this compaction.
  int num_input_levels() const { return inputs_.size(); }

  // return the object that holds the edits to the descriptor done
  // by this compaction.
  versionedit* edit() const { return edit_; }

  // returns the number of input files associated to the specified
  // compaction input level.
  // the function will return 0 if when "compaction_input_level" < 0
  // or "compaction_input_level" >= "num_input_levels()".
  int num_input_files(size_t compaction_input_level) const {
    if (compaction_input_level < inputs_.size()) {
      return inputs_[compaction_input_level].size();
    }
    return 0;
  }

  // returns input version of the compaction
  version* input_version() const { return input_version_; }

  // returns the columnfamilydata associated with the compaction.
  columnfamilydata* column_family_data() const { return cfd_; }

  // returns the file meta data of the 'i'th input file at the
  // specified compaction input level.
  // requirement: "compaction_input_level" must be >= 0 and
  //              < "input_levels()"
  filemetadata* input(size_t compaction_input_level, int i) const {
    assert(compaction_input_level < inputs_.size());
    return inputs_[compaction_input_level][i];
  }

  // returns the list of file meta data of the specified compaction
  // input level.
  // requirement: "compaction_input_level" must be >= 0 and
  //              < "input_levels()"
  std::vector<filemetadata*>* const inputs(size_t compaction_input_level) {
    assert(compaction_input_level < inputs_.size());
    return &inputs_[compaction_input_level].files;
  }

  // returns the filelevel of the specified compaction input level.
  filelevel* input_levels(int compaction_input_level) {
    return &input_levels_[compaction_input_level];
  }

  // maximum size of files to build during this compaction.
  uint64_t maxoutputfilesize() const { return max_output_file_size_; }

  // what compression for output
  compressiontype outputcompressiontype() const { return output_compression_; }

  // whether need to write output file to second db path.
  uint32_t getoutputpathid() const { return output_path_id_; }

  // generate input_levels_ from inputs_
  // should be called when inputs_ is stable
  void generatefilelevels();

  // is this a trivial compaction that can be implemented by just
  // moving a single input file to the next level (no merging or splitting)
  bool istrivialmove() const;

  // if true, then the comaction can be done by simply deleting input files.
  bool isdeletioncompaction() const {
    return deletion_compaction_;
  }

  // add all inputs to this compaction as delete operations to *edit.
  void addinputdeletions(versionedit* edit);

  // returns true if the available information we have guarantees that
  // the input "user_key" does not exist in any level beyond "output_level()".
  bool keynotexistsbeyondoutputlevel(const slice& user_key);

  // returns true iff we should stop building the current output
  // before processing "internal_key".
  bool shouldstopbefore(const slice& internal_key);

  // release the input version for the compaction, once the compaction
  // is successful.
  void releaseinputs();

  // clear all files to indicate that they are not being compacted
  // delete this compaction from the list of running compactions.
  void releasecompactionfiles(status status);

  // returns the summary of the compaction in "output" with maximum "len"
  // in bytes.  the caller is responsible for the memory management of
  // "output".
  void summary(char* output, int len);

  // return the score that was used to pick this compaction run.
  double score() const { return score_; }

  // is this compaction creating a file in the bottom most level?
  bool bottommostlevel() { return bottommost_level_; }

  // does this compaction include all sst files?
  bool isfullcompaction() { return is_full_compaction_; }

  // was this compaction triggered manually by the client?
  bool ismanualcompaction() { return is_manual_compaction_; }

  // returns the size in bytes that the output file should be preallocated to.
  // in level compaction, that is max_file_size_. in universal compaction, that
  // is the sum of all input file sizes.
  uint64_t outputfilepreallocationsize();

 private:
  friend class compactionpicker;
  friend class universalcompactionpicker;
  friend class fifocompactionpicker;
  friend class levelcompactionpicker;

  compaction(version* input_version, int start_level, int out_level,
             uint64_t target_file_size, uint64_t max_grandparent_overlap_bytes,
             uint32_t output_path_id, compressiontype output_compression,
             bool seek_compaction = false, bool deletion_compaction = false);

  const int start_level_;    // the lowest level to be compacted
  const int output_level_;  // levels to which output files are stored
  uint64_t max_output_file_size_;
  uint64_t max_grandparent_overlap_bytes_;
  version* input_version_;
  versionedit* edit_;
  int number_levels_;
  columnfamilydata* cfd_;
  arena arena_;          // arena used to allocate space for file_levels_

  uint32_t output_path_id_;
  compressiontype output_compression_;
  bool seek_compaction_;
  // if true, then the comaction can be done by simply deleting input files.
  bool deletion_compaction_;

  // compaction input files organized by level.
  autovector<compactioninputfiles> inputs_;

  // a copy of inputs_, organized more closely in memory
  autovector<filelevel, 2> input_levels_;

  // state used to check for number of of overlapping grandparent files
  // (grandparent == "output_level_ + 1")
  // this vector is updated by version::getoverlappinginputs().
  std::vector<filemetadata*> grandparents_;
  size_t grandparent_index_;   // index in grandparent_starts_
  bool seen_key_;              // some output key has been seen
  uint64_t overlapped_bytes_;  // bytes of overlap between current output
                               // and grandparent files
  int base_index_;    // index of the file in files_[start_level_]
  int parent_index_;  // index of some file with same range in
                      // files_[start_level_+1]
  double score_;      // score that was used to pick this compaction.

  // is this compaction creating a file in the bottom most level?
  bool bottommost_level_;
  // does this compaction include all sst files?
  bool is_full_compaction_;

  // is this compaction requested by the client?
  bool is_manual_compaction_;

  // "level_ptrs_" holds indices into "input_version_->levels_", where each
  // index remembers which file of an associated level we are currently used
  // to check keynotexistsbeyondoutputlevel() for deletion operation.
  // as it is for checking keynotexistsbeyondoutputlevel(), it only
  // records indices for all levels beyond "output_level_".
  std::vector<size_t> level_ptrs_;

  // mark (or clear) all files that are being compacted
  void markfilesbeingcompacted(bool mark_as_compacted);

  // initialize whether the compaction is producing files at the
  // bottommost level.
  //
  // @see bottommostlevel()
  void setupbottommostlevel(bool is_manual);

  // in case of compaction error, reset the nextindex that is used
  // to pick up the next file to be compacted from files_by_size_
  void resetnextcompactionindex();
};

// utility function
extern uint64_t totalfilesize(const std::vector<filemetadata*>& files);

}  // namespace rocksdb
