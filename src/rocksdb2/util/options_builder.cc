//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include <math.h>
#include <algorithm>
#include "rocksdb/options.h"

namespace rocksdb {

namespace {

// for now, always use 1-0 as level bytes multiplier.
const int kbytesforlevelmultiplier = 10;
const size_t kbytesforonemb = 1024 * 1024;

// pick compaction style
compactionstyle pickcompactionstyle(size_t write_buffer_size,
                                    int read_amp_threshold,
                                    int write_amp_threshold,
                                    uint64_t target_db_size) {
  // estimate read amplification and write amplification of two compaction
  // styles. if there is hard limit to force a choice, make the choice.
  // otherwise, calculate a score based on threshold and expected value of
  // two styles, weighing reads 4x important than writes.
  int expected_levels = static_cast<int>(ceil(
      ::log(target_db_size / write_buffer_size) / ::log(kbytesforlevelmultiplier)));

  int expected_max_files_universal =
      static_cast<int>(ceil(log2(target_db_size / write_buffer_size)));

  const int kestimatedlevel0filesinlevelstyle = 2;
  // estimate write amplification:
  // (1) 1 for every l0 file
  // (2) 2 for l1
  // (3) kbytesforlevelmultiplier for the last level. it's really hard to
  //     predict.
  // (3) kbytesforlevelmultiplier for other levels.
  int expected_write_amp_level = kestimatedlevel0filesinlevelstyle + 2
      + (expected_levels - 2) * kbytesforlevelmultiplier
      + kbytesforlevelmultiplier;
  int expected_read_amp_level =
      kestimatedlevel0filesinlevelstyle + expected_levels;

  int max_read_amp_uni = expected_max_files_universal;
  if (read_amp_threshold <= max_read_amp_uni) {
    return kcompactionstylelevel;
  } else if (write_amp_threshold <= expected_write_amp_level) {
    return kcompactionstyleuniversal;
  }

  const double kreadwriteweight = 4;

  double level_ratio =
      static_cast<double>(read_amp_threshold) / expected_read_amp_level *
          kreadwriteweight +
      static_cast<double>(write_amp_threshold) / expected_write_amp_level;

  int expected_write_amp_uni = expected_max_files_universal / 2 + 2;
  int expected_read_amp_uni = expected_max_files_universal / 2 + 1;

  double uni_ratio =
      static_cast<double>(read_amp_threshold) / expected_read_amp_uni *
          kreadwriteweight +
      static_cast<double>(write_amp_threshold) / expected_write_amp_uni;

  if (level_ratio > uni_ratio) {
    return kcompactionstylelevel;
  } else {
    return kcompactionstyleuniversal;
  }
}

// pick mem table size
void pickwritebuffersize(size_t total_write_buffer_limit, options* options) {
  const size_t kmaxwritebuffersize = 128 * kbytesforonemb;
  const size_t kminwritebuffersize = 4 * kbytesforonemb;

  // try to pick up a buffer size between 4mb and 128mb.
  // and try to pick 4 as the total number of write buffers.
  size_t write_buffer_size = total_write_buffer_limit / 4;
  if (write_buffer_size > kmaxwritebuffersize) {
    write_buffer_size = kmaxwritebuffersize;
  } else if (write_buffer_size < kminwritebuffersize) {
    write_buffer_size = std::min(static_cast<size_t>(kminwritebuffersize),
                                 total_write_buffer_limit / 2);
  }

  // truncate to multiple of 1mb.
  if (write_buffer_size % kbytesforonemb != 0) {
    write_buffer_size =
        (write_buffer_size / kbytesforonemb + 1) * kbytesforonemb;
  }

  options->write_buffer_size = write_buffer_size;
  options->max_write_buffer_number =
      total_write_buffer_limit / write_buffer_size;
  options->min_write_buffer_number_to_merge = 1;
}

void optimizeforuniversal(options* options) {
  options->level0_file_num_compaction_trigger = 2;
  options->level0_slowdown_writes_trigger = 30;
  options->level0_stop_writes_trigger = 40;
  options->max_open_files = -1;
}

// optimize parameters for level-based compaction
void optimizeforlevel(int read_amplification_threshold,
                      int write_amplification_threshold,
                      uint64_t target_db_size, options* options) {
  int expected_levels_one_level0_file =
      static_cast<int>(ceil(::log(target_db_size / options->write_buffer_size) /
                            ::log(kbytesforlevelmultiplier)));

  int level0_stop_writes_trigger =
      read_amplification_threshold - expected_levels_one_level0_file;

  const size_t kinitiallevel0totalsize = 128 * kbytesforonemb;
  const int kmaxfilenumcompactiontrigger = 4;
  const int kminlevel0stoptrigger = 3;

  int file_num_buffer =
      kinitiallevel0totalsize / options->write_buffer_size + 1;

  if (level0_stop_writes_trigger > file_num_buffer) {
    // have sufficient room for multiple level 0 files
    // try enlarge the buffer up to 1gb

    // try to enlarge the buffer up to 1gb, if still have sufficient headroom.
    file_num_buffer *=
        1 << std::max(0, std::min(3, level0_stop_writes_trigger -
                                       file_num_buffer - 2));

    options->level0_stop_writes_trigger = level0_stop_writes_trigger;
    options->level0_slowdown_writes_trigger = level0_stop_writes_trigger - 2;
    options->level0_file_num_compaction_trigger =
        std::min(kmaxfilenumcompactiontrigger, file_num_buffer / 2);
  } else {
    options->level0_stop_writes_trigger =
        std::max(kminlevel0stoptrigger, file_num_buffer);
    options->level0_slowdown_writes_trigger =
        options->level0_stop_writes_trigger - 1;
    options->level0_file_num_compaction_trigger = 1;
  }

  // this doesn't consider compaction and overheads of mem tables. but usually
  // it is in the same order of magnitude.
  int expected_level0_compaction_size =
      options->level0_file_num_compaction_trigger * options->write_buffer_size;
  // enlarge level1 target file size if level0 compaction size is larger.
  int max_bytes_for_level_base = 10 * kbytesforonemb;
  if (expected_level0_compaction_size > max_bytes_for_level_base) {
    max_bytes_for_level_base = expected_level0_compaction_size;
  }
  options->max_bytes_for_level_base = max_bytes_for_level_base;
  // now always set level multiplier to be 10
  options->max_bytes_for_level_multiplier = kbytesforlevelmultiplier;

  const int kminfilesize = 2 * kbytesforonemb;
  // allow at least 3-way parallelism for compaction between level 1 and 2.
  int max_file_size = max_bytes_for_level_base / 3;
  if (max_file_size < kminfilesize) {
    options->target_file_size_base = kminfilesize;
  } else {
    if (max_file_size % kbytesforonemb != 0) {
      max_file_size = (max_file_size / kbytesforonemb + 1) * kbytesforonemb;
    }
    options->target_file_size_base = max_file_size;
  }

  // todo: consider to tune num_levels too.
}

}  // namespace

options getoptions(size_t total_write_buffer_limit,
                   int read_amplification_threshold,
                   int write_amplification_threshold, uint64_t target_db_size) {
  options options;
  pickwritebuffersize(total_write_buffer_limit, &options);
  size_t write_buffer_size = options.write_buffer_size;
  options.compaction_style =
      pickcompactionstyle(write_buffer_size, read_amplification_threshold,
                          write_amplification_threshold, target_db_size);
  if (options.compaction_style == kcompactionstyleuniversal) {
    optimizeforuniversal(&options);
  } else {
    optimizeforlevel(read_amplification_threshold,
                     write_amplification_threshold, target_db_size, &options);
  }
  return options;
}

}  // namespace rocksdb
