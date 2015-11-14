//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/compaction.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <vector>

#include "db/column_family.h"
#include "util/logging.h"

namespace rocksdb {

uint64_t totalfilesize(const std::vector<filemetadata*>& files) {
  uint64_t sum = 0;
  for (size_t i = 0; i < files.size() && files[i]; i++) {
    sum += files[i]->fd.getfilesize();
  }
  return sum;
}

compaction::compaction(version* input_version, int start_level, int out_level,
                       uint64_t target_file_size,
                       uint64_t max_grandparent_overlap_bytes,
                       uint32_t output_path_id,
                       compressiontype output_compression, bool seek_compaction,
                       bool deletion_compaction)
    : start_level_(start_level),
      output_level_(out_level),
      max_output_file_size_(target_file_size),
      max_grandparent_overlap_bytes_(max_grandparent_overlap_bytes),
      input_version_(input_version),
      number_levels_(input_version_->numberlevels()),
      cfd_(input_version_->cfd_),
      output_path_id_(output_path_id),
      output_compression_(output_compression),
      seek_compaction_(seek_compaction),
      deletion_compaction_(deletion_compaction),
      grandparent_index_(0),
      seen_key_(false),
      overlapped_bytes_(0),
      base_index_(-1),
      parent_index_(-1),
      score_(0),
      bottommost_level_(false),
      is_full_compaction_(false),
      is_manual_compaction_(false),
      level_ptrs_(std::vector<size_t>(number_levels_)) {

  cfd_->ref();
  input_version_->ref();
  edit_ = new versionedit();
  edit_->setcolumnfamily(cfd_->getid());
  for (int i = 0; i < number_levels_; i++) {
    level_ptrs_[i] = 0;
  }
  int num_levels = output_level_ - start_level_ + 1;
  input_levels_.resize(num_levels);
  inputs_.resize(num_levels);
  for (int i = 0; i < num_levels; ++i) {
    inputs_[i].level = start_level_ + i;
  }
}

compaction::~compaction() {
  delete edit_;
  if (input_version_ != nullptr) {
    input_version_->unref();
  }
  if (cfd_ != nullptr) {
    if (cfd_->unref()) {
      delete cfd_;
    }
  }
}

void compaction::generatefilelevels() {
  input_levels_.resize(num_input_levels());
  for (int which = 0; which < num_input_levels(); which++) {
    dogeneratefilelevel(&input_levels_[which], inputs_[which].files, &arena_);
  }
}

bool compaction::istrivialmove() const {
  // avoid a move if there is lots of overlapping grandparent data.
  // otherwise, the move could create a parent file that will require
  // a very expensive merge later on.
  // if start_level_== output_level_, the purpose is to force compaction
  // filter to be applied to that level, and thus cannot be a trivia move.
  return (start_level_ != output_level_ &&
          num_input_levels() == 2 &&
          num_input_files(0) == 1 &&
          num_input_files(1) == 0 &&
          totalfilesize(grandparents_) <= max_grandparent_overlap_bytes_);
}

void compaction::addinputdeletions(versionedit* edit) {
  for (int which = 0; which < num_input_levels(); which++) {
    for (size_t i = 0; i < inputs_[which].size(); i++) {
      edit->deletefile(level(which), inputs_[which][i]->fd.getnumber());
    }
  }
}

bool compaction::keynotexistsbeyondoutputlevel(const slice& user_key) {
  assert(cfd_->options()->compaction_style != kcompactionstylefifo);
  if (cfd_->options()->compaction_style == kcompactionstyleuniversal) {
    return bottommost_level_;
  }
  // maybe use binary search to find right entry instead of linear search?
  const comparator* user_cmp = cfd_->user_comparator();
  for (int lvl = output_level_ + 1; lvl < number_levels_; lvl++) {
    const std::vector<filemetadata*>& files = input_version_->files_[lvl];
    for (; level_ptrs_[lvl] < files.size(); ) {
      filemetadata* f = files[level_ptrs_[lvl]];
      if (user_cmp->compare(user_key, f->largest.user_key()) <= 0) {
        // we've advanced far enough
        if (user_cmp->compare(user_key, f->smallest.user_key()) >= 0) {
          // key falls in this file's range, so definitely
          // exists beyond output level
          return false;
        }
        break;
      }
      level_ptrs_[lvl]++;
    }
  }
  return true;
}

bool compaction::shouldstopbefore(const slice& internal_key) {
  // scan to find earliest grandparent file that contains key.
  const internalkeycomparator* icmp = &cfd_->internal_comparator();
  while (grandparent_index_ < grandparents_.size() &&
      icmp->compare(internal_key,
                    grandparents_[grandparent_index_]->largest.encode()) > 0) {
    if (seen_key_) {
      overlapped_bytes_ += grandparents_[grandparent_index_]->fd.getfilesize();
    }
    assert(grandparent_index_ + 1 >= grandparents_.size() ||
           icmp->compare(grandparents_[grandparent_index_]->largest.encode(),
                         grandparents_[grandparent_index_+1]->smallest.encode())
                         < 0);
    grandparent_index_++;
  }
  seen_key_ = true;

  if (overlapped_bytes_ > max_grandparent_overlap_bytes_) {
    // too much overlap for current output; start new output
    overlapped_bytes_ = 0;
    return true;
  } else {
    return false;
  }
}

// mark (or clear) each file that is being compacted
void compaction::markfilesbeingcompacted(bool mark_as_compacted) {
  for (int i = 0; i < num_input_levels(); i++) {
    for (unsigned int j = 0; j < inputs_[i].size(); j++) {
      assert(mark_as_compacted ? !inputs_[i][j]->being_compacted :
                                  inputs_[i][j]->being_compacted);
      inputs_[i][j]->being_compacted = mark_as_compacted;
    }
  }
}

// is this compaction producing files at the bottommost level?
void compaction::setupbottommostlevel(bool is_manual) {
  assert(cfd_->options()->compaction_style != kcompactionstylefifo);
  if (cfd_->options()->compaction_style == kcompactionstyleuniversal) {
    // if universal compaction style is used and manual
    // compaction is occuring, then we are guaranteed that
    // all files will be picked in a single compaction
    // run. we can safely set bottommost_level_ = true.
    // if it is not manual compaction, then bottommost_level_
    // is already set when the compaction was created.
    if (is_manual) {
      bottommost_level_ = true;
    }
    return;
  }
  bottommost_level_ = true;
  // checks whether there are files living beyond the output_level.
  for (int i = output_level_ + 1; i < number_levels_; i++) {
    if (input_version_->numlevelfiles(i) > 0) {
      bottommost_level_ = false;
      break;
    }
  }
}

void compaction::releaseinputs() {
  if (input_version_ != nullptr) {
    input_version_->unref();
    input_version_ = nullptr;
  }
  if (cfd_ != nullptr) {
    if (cfd_->unref()) {
      delete cfd_;
    }
    cfd_ = nullptr;
  }
}

void compaction::releasecompactionfiles(status status) {
  cfd_->compaction_picker()->releasecompactionfiles(this, status);
}

void compaction::resetnextcompactionindex() {
  input_version_->resetnextcompactionindex(start_level_);
}

namespace {
int inputsummary(const std::vector<filemetadata*>& files, char* output,
                 int len) {
  *output = '\0';
  int write = 0;
  for (unsigned int i = 0; i < files.size(); i++) {
    int sz = len - write;
    int ret;
    char sztxt[16];
    appendhumanbytes(files.at(i)->fd.getfilesize(), sztxt, 16);
    ret = snprintf(output + write, sz, "%" priu64 "(%s) ",
                   files.at(i)->fd.getnumber(), sztxt);
    if (ret < 0 || ret >= sz) break;
    write += ret;
  }
  // if files.size() is non-zero, overwrite the last space
  return write - !!files.size();
}
}  // namespace

void compaction::summary(char* output, int len) {
  int write =
      snprintf(output, len, "base version %" priu64
                            " base level %d, seek compaction:%d, inputs: [",
               input_version_->getversionnumber(),
               start_level_, seek_compaction_);
  if (write < 0 || write >= len) {
    return;
  }

  for (int level = 0; level < num_input_levels(); ++level) {
    if (level > 0) {
      write += snprintf(output + write, len - write, "], [");
      if (write < 0 || write >= len) {
        return;
      }
    }
    write += inputsummary(inputs_[level].files, output + write, len - write);
    if (write < 0 || write >= len) {
      return;
    }
  }

  snprintf(output + write, len - write, "]");
}

uint64_t compaction::outputfilepreallocationsize() {
  uint64_t preallocation_size = 0;

  if (cfd_->options()->compaction_style == kcompactionstylelevel) {
    preallocation_size =
        cfd_->compaction_picker()->maxfilesizeforlevel(output_level());
  } else {
    for (int level = 0; level < num_input_levels(); ++level) {
      for (const auto& f : inputs_[level].files) {
        preallocation_size += f->fd.getfilesize();
      }
    }
  }
  // over-estimate slightly so we don't end up just barely crossing
  // the threshold
  return preallocation_size * 1.1;
}

}  // namespace rocksdb
