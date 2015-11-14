//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/compaction_picker.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <limits>
#include "db/filename.h"
#include "util/log_buffer.h"
#include "util/statistics.h"

namespace rocksdb {

uint64_t totalcompensatedfilesize(const std::vector<filemetadata*>& files) {
  uint64_t sum = 0;
  for (size_t i = 0; i < files.size() && files[i]; i++) {
    sum += files[i]->compensated_file_size;
  }
  return sum;
}

namespace {
// determine compression type, based on user options, level of the output
// file and whether compression is disabled.
// if enable_compression is false, then compression is always disabled no
// matter what the values of the other two parameters are.
// otherwise, the compression type is determined based on options and level.
compressiontype getcompressiontype(const options& options, int level,
                                   const bool enable_compression = true) {
  if (!enable_compression) {
    // disable compression
    return knocompression;
  }
  // if the use has specified a different compression level for each level,
  // then pick the compresison for that level.
  if (!options.compression_per_level.empty()) {
    const int n = options.compression_per_level.size() - 1;
    // it is possible for level_ to be -1; in that case, we use level
    // 0's compression.  this occurs mostly in backwards compatibility
    // situations when the builder doesn't know what level the file
    // belongs to.  likewise, if level_ is beyond the end of the
    // specified compression levels, use the last value.
    return options.compression_per_level[std::max(0, std::min(level, n))];
  } else {
    return options.compression;
  }
}

// multiple two operands. if they overflow, return op1.
uint64_t multiplycheckoverflow(uint64_t op1, int op2) {
  if (op1 == 0) {
    return 0;
  }
  if (op2 <= 0) {
    return op1;
  }
  uint64_t casted_op2 = (uint64_t) op2;
  if (std::numeric_limits<uint64_t>::max() / op1 < casted_op2) {
    return op1;
  }
  return op1 * casted_op2;
}

}  // anonymous namespace

compactionpicker::compactionpicker(const options* options,
                                   const internalkeycomparator* icmp)
    : compactions_in_progress_(options->num_levels),
      options_(options),
      num_levels_(options->num_levels),
      icmp_(icmp) {

  max_file_size_.reset(new uint64_t[numberlevels()]);
  level_max_bytes_.reset(new uint64_t[numberlevels()]);
  int target_file_size_multiplier = options_->target_file_size_multiplier;
  int max_bytes_multiplier = options_->max_bytes_for_level_multiplier;
  for (int i = 0; i < numberlevels(); i++) {
    if (i == 0 && options_->compaction_style == kcompactionstyleuniversal) {
      max_file_size_[i] = ullong_max;
      level_max_bytes_[i] = options_->max_bytes_for_level_base;
    } else if (i > 1) {
      max_file_size_[i] = multiplycheckoverflow(max_file_size_[i - 1],
                                                target_file_size_multiplier);
      level_max_bytes_[i] = multiplycheckoverflow(
          multiplycheckoverflow(level_max_bytes_[i - 1], max_bytes_multiplier),
          options_->max_bytes_for_level_multiplier_additional[i - 1]);
    } else {
      max_file_size_[i] = options_->target_file_size_base;
      level_max_bytes_[i] = options_->max_bytes_for_level_base;
    }
  }
}

compactionpicker::~compactionpicker() {}

void compactionpicker::sizebeingcompacted(std::vector<uint64_t>& sizes) {
  for (int level = 0; level < numberlevels() - 1; level++) {
    uint64_t total = 0;
    for (auto c : compactions_in_progress_[level]) {
      assert(c->level() == level);
      for (int i = 0; i < c->num_input_files(0); i++) {
        total += c->input(0, i)->compensated_file_size;
      }
    }
    sizes[level] = total;
  }
}

// clear all files to indicate that they are not being compacted
// delete this compaction from the list of running compactions.
void compactionpicker::releasecompactionfiles(compaction* c, status status) {
  c->markfilesbeingcompacted(false);
  compactions_in_progress_[c->level()].erase(c);
  if (!status.ok()) {
    c->resetnextcompactionindex();
  }
}

uint64_t compactionpicker::maxfilesizeforlevel(int level) const {
  assert(level >= 0);
  assert(level < numberlevels());
  return max_file_size_[level];
}

uint64_t compactionpicker::maxgrandparentoverlapbytes(int level) {
  uint64_t result = maxfilesizeforlevel(level);
  result *= options_->max_grandparent_overlap_factor;
  return result;
}

double compactionpicker::maxbytesforlevel(int level) {
  // note: the result for level zero is not really used since we set
  // the level-0 compaction threshold based on number of files.
  assert(level >= 0);
  assert(level < numberlevels());
  return level_max_bytes_[level];
}

void compactionpicker::getrange(const std::vector<filemetadata*>& inputs,
                                internalkey* smallest, internalkey* largest) {
  assert(!inputs.empty());
  smallest->clear();
  largest->clear();
  for (size_t i = 0; i < inputs.size(); i++) {
    filemetadata* f = inputs[i];
    if (i == 0) {
      *smallest = f->smallest;
      *largest = f->largest;
    } else {
      if (icmp_->compare(f->smallest, *smallest) < 0) {
        *smallest = f->smallest;
      }
      if (icmp_->compare(f->largest, *largest) > 0) {
        *largest = f->largest;
      }
    }
  }
}

void compactionpicker::getrange(const std::vector<filemetadata*>& inputs1,
                                const std::vector<filemetadata*>& inputs2,
                                internalkey* smallest, internalkey* largest) {
  std::vector<filemetadata*> all = inputs1;
  all.insert(all.end(), inputs2.begin(), inputs2.end());
  getrange(all, smallest, largest);
}

bool compactionpicker::expandwhileoverlapping(compaction* c) {
  assert(c != nullptr);
  // if inputs are empty then there is nothing to expand.
  if (c->inputs_[0].empty()) {
    assert(c->inputs_[1].empty());
    // this isn't good compaction
    return false;
  }

  // getoverlappinginputs will always do the right thing for level-0.
  // so we don't need to do any expansion if level == 0.
  if (c->level() == 0) {
    return true;
  }

  const int level = c->level();
  internalkey smallest, largest;

  // keep expanding c->inputs_[0] until we are sure that there is a
  // "clean cut" boundary between the files in input and the surrounding files.
  // this will ensure that no parts of a key are lost during compaction.
  int hint_index = -1;
  size_t old_size;
  do {
    old_size = c->inputs_[0].size();
    getrange(c->inputs_[0].files, &smallest, &largest);
    c->inputs_[0].clear();
    c->input_version_->getoverlappinginputs(
        level, &smallest, &largest, &c->inputs_[0].files,
        hint_index, &hint_index);
  } while(c->inputs_[0].size() > old_size);

  // get the new range
  getrange(c->inputs_[0].files, &smallest, &largest);

  // if, after the expansion, there are files that are already under
  // compaction, then we must drop/cancel this compaction.
  int parent_index = -1;
  if (c->inputs_[0].empty()) {
    log(options_->info_log,
        "[%s] expandwhileoverlapping() failure because zero input files",
        c->column_family_data()->getname().c_str());
  }
  if (c->inputs_[0].empty() || filesincompaction(c->inputs_[0].files) ||
      (c->level() != c->output_level() &&
       parentrangeincompaction(c->input_version_, &smallest, &largest, level,
                               &parent_index))) {
    c->inputs_[0].clear();
    c->inputs_[1].clear();
    return false;
  }
  return true;
}

uint64_t compactionpicker::expandedcompactionbytesizelimit(int level) {
  uint64_t result = maxfilesizeforlevel(level);
  result *= options_->expanded_compaction_factor;
  return result;
}

// returns true if any one of specified files are being compacted
bool compactionpicker::filesincompaction(std::vector<filemetadata*>& files) {
  for (unsigned int i = 0; i < files.size(); i++) {
    if (files[i]->being_compacted) {
      return true;
    }
  }
  return false;
}

// returns true if any one of the parent files are being compacted
bool compactionpicker::parentrangeincompaction(version* version,
                                               const internalkey* smallest,
                                               const internalkey* largest,
                                               int level, int* parent_index) {
  std::vector<filemetadata*> inputs;
  assert(level + 1 < numberlevels());

  version->getoverlappinginputs(level + 1, smallest, largest, &inputs,
                                *parent_index, parent_index);
  return filesincompaction(inputs);
}

// populates the set of inputs from "level+1" that overlap with "level".
// will also attempt to expand "level" if that doesn't expand "level+1"
// or cause "level" to include a file for compaction that has an overlapping
// user-key with another file.
void compactionpicker::setupotherinputs(compaction* c) {
  // if inputs are empty, then there is nothing to expand.
  // if both input and output levels are the same, no need to consider
  // files at level "level+1"
  if (c->inputs_[0].empty() || c->level() == c->output_level()) {
    return;
  }

  const int level = c->level();
  internalkey smallest, largest;

  // get the range one last time.
  getrange(c->inputs_[0].files, &smallest, &largest);

  // populate the set of next-level files (inputs_[1]) to include in compaction
  c->input_version_->getoverlappinginputs(
      level + 1, &smallest, &largest,
      &c->inputs_[1].files, c->parent_index_,
      &c->parent_index_);

  // get entire range covered by compaction
  internalkey all_start, all_limit;
  getrange(c->inputs_[0].files, c->inputs_[1].files, &all_start, &all_limit);

  // see if we can further grow the number of inputs in "level" without
  // changing the number of "level+1" files we pick up. we also choose not
  // to expand if this would cause "level" to include some entries for some
  // user key, while excluding other entries for the same user key. this
  // can happen when one user key spans multiple files.
  if (!c->inputs_[1].empty()) {
    std::vector<filemetadata*> expanded0;
    c->input_version_->getoverlappinginputs(
        level, &all_start, &all_limit, &expanded0, c->base_index_, nullptr);
    const uint64_t inputs0_size = totalcompensatedfilesize(c->inputs_[0].files);
    const uint64_t inputs1_size = totalcompensatedfilesize(c->inputs_[1].files);
    const uint64_t expanded0_size = totalcompensatedfilesize(expanded0);
    uint64_t limit = expandedcompactionbytesizelimit(level);
    if (expanded0.size() > c->inputs_[0].size() &&
        inputs1_size + expanded0_size < limit &&
        !filesincompaction(expanded0) &&
        !c->input_version_->hasoverlappinguserkey(&expanded0, level)) {
      internalkey new_start, new_limit;
      getrange(expanded0, &new_start, &new_limit);
      std::vector<filemetadata*> expanded1;
      c->input_version_->getoverlappinginputs(level + 1, &new_start, &new_limit,
                                              &expanded1, c->parent_index_,
                                              &c->parent_index_);
      if (expanded1.size() == c->inputs_[1].size() &&
          !filesincompaction(expanded1)) {
        log(options_->info_log,
            "[%s] expanding@%d %zu+%zu (%" priu64 "+%" priu64
            " bytes) to %zu+%zu (%" priu64 "+%" priu64 "bytes)\n",
            c->column_family_data()->getname().c_str(), level,
            c->inputs_[0].size(), c->inputs_[1].size(), inputs0_size,
            inputs1_size, expanded0.size(), expanded1.size(), expanded0_size,
            inputs1_size);
        smallest = new_start;
        largest = new_limit;
        c->inputs_[0].files = expanded0;
        c->inputs_[1].files = expanded1;
        getrange(c->inputs_[0].files, c->inputs_[1].files,
                 &all_start, &all_limit);
      }
    }
  }

  // compute the set of grandparent files that overlap this compaction
  // (parent == level+1; grandparent == level+2)
  if (level + 2 < numberlevels()) {
    c->input_version_->getoverlappinginputs(level + 2, &all_start, &all_limit,
                                            &c->grandparents_);
  }
}

compaction* compactionpicker::compactrange(version* version, int input_level,
                                           int output_level,
                                           uint32_t output_path_id,
                                           const internalkey* begin,
                                           const internalkey* end,
                                           internalkey** compaction_end) {
  // compactionpickerfifo has its own implementation of compact range
  assert(options_->compaction_style != kcompactionstylefifo);

  std::vector<filemetadata*> inputs;
  bool covering_the_whole_range = true;

  // all files are 'overlapping' in universal style compaction.
  // we have to compact the entire range in one shot.
  if (options_->compaction_style == kcompactionstyleuniversal) {
    begin = nullptr;
    end = nullptr;
  }
  version->getoverlappinginputs(input_level, begin, end, &inputs);
  if (inputs.empty()) {
    return nullptr;
  }

  // avoid compacting too much in one shot in case the range is large.
  // but we cannot do this for level-0 since level-0 files can overlap
  // and we must not pick one file and drop another older file if the
  // two files overlap.
  if (input_level > 0) {
    const uint64_t limit =
        maxfilesizeforlevel(input_level) * options_->source_compaction_factor;
    uint64_t total = 0;
    for (size_t i = 0; i + 1 < inputs.size(); ++i) {
      uint64_t s = inputs[i]->compensated_file_size;
      total += s;
      if (total >= limit) {
        **compaction_end = inputs[i + 1]->smallest;
        covering_the_whole_range = false;
        inputs.resize(i + 1);
        break;
      }
    }
  }
  assert(output_path_id < static_cast<uint32_t>(options_->db_paths.size()));
  compaction* c = new compaction(
      version, input_level, output_level, maxfilesizeforlevel(output_level),
      maxgrandparentoverlapbytes(input_level), output_path_id,
      getcompressiontype(*options_, output_level));

  c->inputs_[0].files = inputs;
  if (expandwhileoverlapping(c) == false) {
    delete c;
    log(options_->info_log,
        "[%s] could not compact due to expansion failure.\n",
        version->cfd_->getname().c_str());
    return nullptr;
  }

  setupotherinputs(c);

  if (covering_the_whole_range) {
    *compaction_end = nullptr;
  }

  // these files that are to be manaully compacted do not trample
  // upon other files because manual compactions are processed when
  // the system has a max of 1 background compaction thread.
  c->markfilesbeingcompacted(true);

  // is this compaction creating a file at the bottommost level
  c->setupbottommostlevel(true);

  c->is_manual_compaction_ = true;

  return c;
}

compaction* levelcompactionpicker::pickcompaction(version* version,
                                                  logbuffer* log_buffer) {
  compaction* c = nullptr;
  int level = -1;

  // compute the compactions needed. it is better to do it here
  // and also in logandapply(), otherwise the values could be stale.
  std::vector<uint64_t> size_being_compacted(numberlevels() - 1);
  sizebeingcompacted(size_being_compacted);
  version->computecompactionscore(size_being_compacted);

  // we prefer compactions triggered by too much data in a level over
  // the compactions triggered by seeks.
  //
  // find the compactions by size on all levels.
  for (int i = 0; i < numberlevels() - 1; i++) {
    assert(i == 0 ||
           version->compaction_score_[i] <= version->compaction_score_[i - 1]);
    level = version->compaction_level_[i];
    if ((version->compaction_score_[i] >= 1)) {
      c = pickcompactionbysize(version, level, version->compaction_score_[i]);
      if (c == nullptr || expandwhileoverlapping(c) == false) {
        delete c;
        c = nullptr;
      } else {
        break;
      }
    }
  }

  if (c == nullptr) {
    return nullptr;
  }

  // two level 0 compaction won't run at the same time, so don't need to worry
  // about files on level 0 being compacted.
  if (level == 0) {
    assert(compactions_in_progress_[0].empty());
    internalkey smallest, largest;
    getrange(c->inputs_[0].files, &smallest, &largest);
    // note that the next call will discard the file we placed in
    // c->inputs_[0] earlier and replace it with an overlapping set
    // which will include the picked file.
    c->inputs_[0].clear();
    c->input_version_->getoverlappinginputs(0, &smallest, &largest,
                                            &c->inputs_[0].files);

    // if we include more l0 files in the same compaction run it can
    // cause the 'smallest' and 'largest' key to get extended to a
    // larger range. so, re-invoke getrange to get the new key range
    getrange(c->inputs_[0].files, &smallest, &largest);
    if (parentrangeincompaction(c->input_version_, &smallest, &largest, level,
                                &c->parent_index_)) {
      delete c;
      return nullptr;
    }
    assert(!c->inputs_[0].empty());
  }

  // setup "level+1" files (inputs_[1])
  setupotherinputs(c);

  // mark all the files that are being compacted
  c->markfilesbeingcompacted(true);

  // is this compaction creating a file at the bottommost level
  c->setupbottommostlevel(false);

  // remember this currently undergoing compaction
  compactions_in_progress_[level].insert(c);

  return c;
}

compaction* levelcompactionpicker::pickcompactionbysize(version* version,
                                                        int level,
                                                        double score) {
  compaction* c = nullptr;

  // level 0 files are overlapping. so we cannot pick more
  // than one concurrent compactions at this level. this
  // could be made better by looking at key-ranges that are
  // being compacted at level 0.
  if (level == 0 && compactions_in_progress_[level].size() == 1) {
    return nullptr;
  }

  assert(level >= 0);
  assert(level + 1 < numberlevels());
  c = new compaction(version, level, level + 1, maxfilesizeforlevel(level + 1),
                     maxgrandparentoverlapbytes(level), 0,
                     getcompressiontype(*options_, level + 1));
  c->score_ = score;

  // pick the largest file in this level that is not already
  // being compacted
  std::vector<int>& file_size = c->input_version_->files_by_size_[level];

  // record the first file that is not yet compacted
  int nextindex = -1;

  for (unsigned int i = c->input_version_->next_file_to_compact_by_size_[level];
       i < file_size.size(); i++) {
    int index = file_size[i];
    filemetadata* f = c->input_version_->files_[level][index];

    // check to verify files are arranged in descending compensated size.
    assert((i == file_size.size() - 1) ||
           (i >= version::number_of_files_to_sort_ - 1) ||
           (f->compensated_file_size >=
            c->input_version_->files_[level][file_size[i + 1]]->
                compensated_file_size));

    // do not pick a file to compact if it is being compacted
    // from n-1 level.
    if (f->being_compacted) {
      continue;
    }

    // remember the startindex for the next call to pickcompaction
    if (nextindex == -1) {
      nextindex = i;
    }

    // do not pick this file if its parents at level+1 are being compacted.
    // maybe we can avoid redoing this work in setupotherinputs
    int parent_index = -1;
    if (parentrangeincompaction(c->input_version_, &f->smallest, &f->largest,
                                level, &parent_index)) {
      continue;
    }
    c->inputs_[0].files.push_back(f);
    c->base_index_ = index;
    c->parent_index_ = parent_index;
    break;
  }

  if (c->inputs_[0].empty()) {
    delete c;
    c = nullptr;
  }

  // store where to start the iteration in the next call to pickcompaction
  version->next_file_to_compact_by_size_[level] = nextindex;

  return c;
}

// universal style of compaction. pick files that are contiguous in
// time-range to compact.
//
compaction* universalcompactionpicker::pickcompaction(version* version,
                                                      logbuffer* log_buffer) {
  int level = 0;
  double score = version->compaction_score_[0];

  if ((version->files_[level].size() <
       (unsigned int)options_->level0_file_num_compaction_trigger)) {
    logtobuffer(log_buffer, "[%s] universal: nothing to do\n",
                version->cfd_->getname().c_str());
    return nullptr;
  }
  version::filesummarystorage tmp;
  logtobuffer(log_buffer, "[%s] universal: candidate files(%zu): %s\n",
              version->cfd_->getname().c_str(), version->files_[level].size(),
              version->levelfilesummary(&tmp, 0));

  // check for size amplification first.
  compaction* c;
  if ((c = pickcompactionuniversalsizeamp(version, score, log_buffer)) !=
      nullptr) {
    logtobuffer(log_buffer, "[%s] universal: compacting for size amp\n",
                version->cfd_->getname().c_str());
  } else {
    // size amplification is within limits. try reducing read
    // amplification while maintaining file size ratios.
    unsigned int ratio = options_->compaction_options_universal.size_ratio;

    if ((c = pickcompactionuniversalreadamp(version, score, ratio, uint_max,
                                            log_buffer)) != nullptr) {
      logtobuffer(log_buffer, "[%s] universal: compacting for size ratio\n",
                  version->cfd_->getname().c_str());
    } else {
      // size amplification and file size ratios are within configured limits.
      // if max read amplification is exceeding configured limits, then force
      // compaction without looking at filesize ratios and try to reduce
      // the number of files to fewer than level0_file_num_compaction_trigger.
      unsigned int num_files = version->files_[level].size() -
                               options_->level0_file_num_compaction_trigger;
      if ((c = pickcompactionuniversalreadamp(
               version, score, uint_max, num_files, log_buffer)) != nullptr) {
        logtobuffer(log_buffer, "[%s] universal: compacting for file num\n",
                    version->cfd_->getname().c_str());
      }
    }
  }
  if (c == nullptr) {
    return nullptr;
  }
  assert(c->inputs_[0].size() > 1);

  // validate that all the chosen files are non overlapping in time
  filemetadata* newerfile __attribute__((unused)) = nullptr;
  for (unsigned int i = 0; i < c->inputs_[0].size(); i++) {
    filemetadata* f = c->inputs_[0][i];
    assert (f->smallest_seqno <= f->largest_seqno);
    assert(newerfile == nullptr ||
           newerfile->smallest_seqno > f->largest_seqno);
    newerfile = f;
  }

  // is the earliest file part of this compaction?
  filemetadata* last_file = c->input_version_->files_[level].back();
  c->bottommost_level_ = c->inputs_[0].files.back() == last_file;

  // update statistics
  measuretime(options_->statistics.get(),
              num_files_in_single_compaction, c->inputs_[0].size());

  // mark all the files that are being compacted
  c->markfilesbeingcompacted(true);

  // remember this currently undergoing compaction
  compactions_in_progress_[level].insert(c);

  // record whether this compaction includes all sst files.
  // for now, it is only relevant in universal compaction mode.
  c->is_full_compaction_ =
      (c->inputs_[0].size() == c->input_version_->files_[0].size());

  return c;
}

uint32_t universalcompactionpicker::getpathid(const options& options,
                                              uint64_t file_size) {
  // two conditions need to be satisfied:
  // (1) the target path needs to be able to hold the file's size
  // (2) total size left in this and previous paths need to be not
  //     smaller than expected future file size before this new file is
  //     compacted, which is estimated based on size_ratio.
  // for example, if now we are compacting files of size (1, 1, 2, 4, 8),
  // we will make sure the target file, probably with size of 16, will be
  // placed in a path so that eventually when new files are generated and
  // compacted to (1, 1, 2, 4, 8, 16), all those files can be stored in or
  // before the path we chose.
  //
  // todo(sdong): now the case of multiple column families is not
  // considered in this algorithm. so the target size can be violated in
  // that case. we need to improve it.
  uint64_t accumulated_size = 0;
  uint64_t future_size =
      file_size * (100 - options.compaction_options_universal.size_ratio) / 100;
  uint32_t p = 0;
  for (; p < options.db_paths.size() - 1; p++) {
    uint64_t target_size = options.db_paths[p].target_size;
    if (target_size > file_size &&
        accumulated_size + (target_size - file_size) > future_size) {
      return p;
    }
    accumulated_size += target_size;
  }
  return p;
}

//
// consider compaction files based on their size differences with
// the next file in time order.
//
compaction* universalcompactionpicker::pickcompactionuniversalreadamp(
    version* version, double score, unsigned int ratio,
    unsigned int max_number_of_files_to_compact, logbuffer* log_buffer) {
  int level = 0;

  unsigned int min_merge_width =
    options_->compaction_options_universal.min_merge_width;
  unsigned int max_merge_width =
    options_->compaction_options_universal.max_merge_width;

  // the files are sorted from newest first to oldest last.
  const auto& files = version->files_[level];

  filemetadata* f = nullptr;
  bool done = false;
  int start_index = 0;
  unsigned int candidate_count = 0;

  unsigned int max_files_to_compact = std::min(max_merge_width,
                                       max_number_of_files_to_compact);
  min_merge_width = std::max(min_merge_width, 2u);

  // considers a candidate file only if it is smaller than the
  // total size accumulated so far.
  for (unsigned int loop = 0; loop < files.size(); loop++) {

    candidate_count = 0;

    // skip files that are already being compacted
    for (f = nullptr; loop < files.size(); loop++) {
      f = files[loop];

      if (!f->being_compacted) {
        candidate_count = 1;
        break;
      }
      logtobuffer(log_buffer, "[%s] universal: file %" priu64
                              "[%d] being compacted, skipping",
                  version->cfd_->getname().c_str(), f->fd.getnumber(), loop);
      f = nullptr;
    }

    // this file is not being compacted. consider it as the
    // first candidate to be compacted.
    uint64_t candidate_size =  f != nullptr? f->compensated_file_size : 0;
    if (f != nullptr) {
      char file_num_buf[kformatfilenumberbufsize];
      formatfilenumber(f->fd.getnumber(), f->fd.getpathid(), file_num_buf,
                       sizeof(file_num_buf));
      logtobuffer(log_buffer, "[%s] universal: possible candidate file %s[%d].",
                  version->cfd_->getname().c_str(), file_num_buf, loop);
    }

    // check if the suceeding files need compaction.
    for (unsigned int i = loop + 1;
         candidate_count < max_files_to_compact && i < files.size(); i++) {
      filemetadata* f = files[i];
      if (f->being_compacted) {
        break;
      }
      // pick files if the total/last candidate file size (increased by the
      // specified ratio) is still larger than the next candidate file.
      // candidate_size is the total size of files picked so far with the
      // default kcompactionstopstyletotalsize; with
      // kcompactionstopstylesimilarsize, it's simply the size of the last
      // picked file.
      uint64_t sz = (candidate_size * (100l + ratio)) /100;
      if (sz < f->fd.getfilesize()) {
        break;
      }
      if (options_->compaction_options_universal.stop_style == kcompactionstopstylesimilarsize) {
        // similar-size stopping rule: also check the last picked file isn't
        // far larger than the next candidate file.
        sz = (f->fd.getfilesize() * (100l + ratio)) / 100;
        if (sz < candidate_size) {
          // if the small file we've encountered begins a run of similar-size
          // files, we'll pick them up on a future iteration of the outer
          // loop. if it's some lonely straggler, it'll eventually get picked
          // by the last-resort read amp strategy which disregards size ratios.
          break;
        }
        candidate_size = f->compensated_file_size;
      } else { // default kcompactionstopstyletotalsize
        candidate_size += f->compensated_file_size;
      }
      candidate_count++;
    }

    // found a series of consecutive files that need compaction.
    if (candidate_count >= (unsigned int)min_merge_width) {
      start_index = loop;
      done = true;
      break;
    } else {
      for (unsigned int i = loop;
           i < loop + candidate_count && i < files.size(); i++) {
        filemetadata* f = files[i];
        logtobuffer(log_buffer, "[%s] universal: skipping file %" priu64
                                "[%d] with size %" priu64
                                " (compensated size %" priu64 ") %d\n",
                    version->cfd_->getname().c_str(), f->fd.getnumber(), i,
                    f->fd.getfilesize(), f->compensated_file_size,
                    f->being_compacted);
      }
    }
  }
  if (!done || candidate_count <= 1) {
    return nullptr;
  }
  unsigned int first_index_after = start_index + candidate_count;
  // compression is enabled if files compacted earlier already reached
  // size ratio of compression.
  bool enable_compression = true;
  int ratio_to_compress =
      options_->compaction_options_universal.compression_size_percent;
  if (ratio_to_compress >= 0) {
    uint64_t total_size = version->numlevelbytes(level);
    uint64_t older_file_size = 0;
    for (unsigned int i = files.size() - 1;
         i >= first_index_after; i--) {
      older_file_size += files[i]->fd.getfilesize();
      if (older_file_size * 100l >= total_size * (long) ratio_to_compress) {
        enable_compression = false;
        break;
      }
    }
  }

  uint64_t estimated_total_size = 0;
  for (unsigned int i = 0; i < first_index_after; i++) {
    estimated_total_size += files[i]->fd.getfilesize();
  }
  uint32_t path_id = getpathid(*options_, estimated_total_size);

  compaction* c = new compaction(
      version, level, level, maxfilesizeforlevel(level), llong_max, path_id,
      getcompressiontype(*options_, level, enable_compression));
  c->score_ = score;

  for (unsigned int i = start_index; i < first_index_after; i++) {
    filemetadata* f = c->input_version_->files_[level][i];
    c->inputs_[0].files.push_back(f);
    char file_num_buf[kformatfilenumberbufsize];
    formatfilenumber(f->fd.getnumber(), f->fd.getpathid(), file_num_buf,
                     sizeof(file_num_buf));
    logtobuffer(log_buffer,
                "[%s] universal: picking file %s[%d] "
                "with size %" priu64 " (compensated size %" priu64 ")\n",
                version->cfd_->getname().c_str(), file_num_buf, i,
                f->fd.getfilesize(), f->compensated_file_size);
  }
  return c;
}

// look at overall size amplification. if size amplification
// exceeeds the configured value, then do a compaction
// of the candidate files all the way upto the earliest
// base file (overrides configured values of file-size ratios,
// min_merge_width and max_merge_width).
//
compaction* universalcompactionpicker::pickcompactionuniversalsizeamp(
    version* version, double score, logbuffer* log_buffer) {
  int level = 0;

  // percentage flexibilty while reducing size amplification
  uint64_t ratio = options_->compaction_options_universal.
                     max_size_amplification_percent;

  // the files are sorted from newest first to oldest last.
  const auto& files = version->files_[level];

  unsigned int candidate_count = 0;
  uint64_t candidate_size = 0;
  unsigned int start_index = 0;
  filemetadata* f = nullptr;

  // skip files that are already being compacted
  for (unsigned int loop = 0; loop < files.size() - 1; loop++) {
    f = files[loop];
    if (!f->being_compacted) {
      start_index = loop;         // consider this as the first candidate.
      break;
    }
    char file_num_buf[kformatfilenumberbufsize];
    formatfilenumber(f->fd.getnumber(), f->fd.getpathid(), file_num_buf,
                     sizeof(file_num_buf));
    logtobuffer(log_buffer, "[%s] universal: skipping file %s[%d] compacted %s",
                version->cfd_->getname().c_str(), file_num_buf, loop,
                " cannot be a candidate to reduce size amp.\n");
    f = nullptr;
  }
  if (f == nullptr) {
    return nullptr;             // no candidate files
  }

  char file_num_buf[kformatfilenumberbufsize];
  formatfilenumber(f->fd.getnumber(), f->fd.getpathid(), file_num_buf,
                   sizeof(file_num_buf));
  logtobuffer(log_buffer, "[%s] universal: first candidate file %s[%d] %s",
              version->cfd_->getname().c_str(), file_num_buf, start_index,
              " to reduce size amp.\n");

  // keep adding up all the remaining files
  for (unsigned int loop = start_index; loop < files.size() - 1; loop++) {
    f = files[loop];
    if (f->being_compacted) {
      char file_num_buf[kformatfilenumberbufsize];
      formatfilenumber(f->fd.getnumber(), f->fd.getpathid(), file_num_buf,
                       sizeof(file_num_buf));
      logtobuffer(
          log_buffer, "[%s] universal: possible candidate file %s[%d] %s.",
          version->cfd_->getname().c_str(), file_num_buf, loop,
          " is already being compacted. no size amp reduction possible.\n");
      return nullptr;
    }
    candidate_size += f->compensated_file_size;
    candidate_count++;
  }
  if (candidate_count == 0) {
    return nullptr;
  }

  // size of earliest file
  uint64_t earliest_file_size = files.back()->fd.getfilesize();

  // size amplification = percentage of additional size
  if (candidate_size * 100 < ratio * earliest_file_size) {
    logtobuffer(
        log_buffer,
        "[%s] universal: size amp not needed. newer-files-total-size %" priu64
        "earliest-file-size %" priu64,
        version->cfd_->getname().c_str(), candidate_size, earliest_file_size);
    return nullptr;
  } else {
    logtobuffer(
        log_buffer,
        "[%s] universal: size amp needed. newer-files-total-size %" priu64
        "earliest-file-size %" priu64,
        version->cfd_->getname().c_str(), candidate_size, earliest_file_size);
  }
  assert(start_index >= 0 && start_index < files.size() - 1);

  // estimate total file size
  uint64_t estimated_total_size = 0;
  for (unsigned int loop = start_index; loop < files.size(); loop++) {
    estimated_total_size += files[loop]->fd.getfilesize();
  }
  uint32_t path_id = getpathid(*options_, estimated_total_size);

  // create a compaction request
  // we always compact all the files, so always compress.
  compaction* c =
      new compaction(version, level, level, maxfilesizeforlevel(level),
                     llong_max, path_id, getcompressiontype(*options_, level));
  c->score_ = score;
  for (unsigned int loop = start_index; loop < files.size(); loop++) {
    f = c->input_version_->files_[level][loop];
    c->inputs_[0].files.push_back(f);
    logtobuffer(log_buffer,
        "[%s] universal: size amp picking file %" priu64 "[%d] "
        "with size %" priu64 " (compensated size %" priu64 ")",
        version->cfd_->getname().c_str(),
        f->fd.getnumber(), loop,
        f->fd.getfilesize(), f->compensated_file_size);
  }
  return c;
}

compaction* fifocompactionpicker::pickcompaction(version* version,
                                                 logbuffer* log_buffer) {
  assert(version->numberlevels() == 1);
  uint64_t total_size = 0;
  for (const auto& file : version->files_[0]) {
    total_size += file->compensated_file_size;
  }

  if (total_size <= options_->compaction_options_fifo.max_table_files_size ||
      version->files_[0].size() == 0) {
    // total size not exceeded
    logtobuffer(log_buffer,
                "[%s] fifo compaction: nothing to do. total size %" priu64
                ", max size %" priu64 "\n",
                version->cfd_->getname().c_str(), total_size,
                options_->compaction_options_fifo.max_table_files_size);
    return nullptr;
  }

  if (compactions_in_progress_[0].size() > 0) {
    logtobuffer(log_buffer,
                "[%s] fifo compaction: already executing compaction. no need "
                "to run parallel compactions since compactions are very fast",
                version->cfd_->getname().c_str());
    return nullptr;
  }

  compaction* c = new compaction(version, 0, 0, 0, 0, 0, knocompression, false,
                                 true /* is deletion compaction */);
  // delete old files (fifo)
  for (auto ritr = version->files_[0].rbegin();
       ritr != version->files_[0].rend(); ++ritr) {
    auto f = *ritr;
    total_size -= f->compensated_file_size;
    c->inputs_[0].files.push_back(f);
    char tmp_fsize[16];
    appendhumanbytes(f->fd.getfilesize(), tmp_fsize, sizeof(tmp_fsize));
    logtobuffer(log_buffer, "[%s] fifo compaction: picking file %" priu64
                            " with size %s for deletion",
                version->cfd_->getname().c_str(), f->fd.getnumber(), tmp_fsize);
    if (total_size <= options_->compaction_options_fifo.max_table_files_size) {
      break;
    }
  }

  c->markfilesbeingcompacted(true);
  compactions_in_progress_[0].insert(c);

  return c;
}

compaction* fifocompactionpicker::compactrange(
    version* version, int input_level, int output_level,
    uint32_t output_path_id, const internalkey* begin, const internalkey* end,
    internalkey** compaction_end) {
  assert(input_level == 0);
  assert(output_level == 0);
  *compaction_end = nullptr;
  logbuffer log_buffer(infologlevel::info_level, options_->info_log.get());
  compaction* c = pickcompaction(version, &log_buffer);
  if (c != nullptr) {
    assert(output_path_id < static_cast<uint32_t>(options_->db_paths.size()));
    c->output_path_id_ = output_path_id;
  }
  log_buffer.flushbuffertolog();
  return c;
}

}  // namespace rocksdb
