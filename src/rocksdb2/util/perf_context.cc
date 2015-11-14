//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//

#include <sstream>
#include "util/perf_context_imp.h"

namespace rocksdb {

#if defined(nperf_context) || defined(ios_cross_compile)
perflevel perf_level = kenablecount;
// this is a dummy variable since some place references it
perfcontext perf_context;
#else
__thread perflevel perf_level = kenablecount;
__thread perfcontext perf_context;
#endif

void setperflevel(perflevel level) {
  perf_level = level;
}

perflevel getperflevel() {
  return perf_level;
}

void perfcontext::reset() {
#if !defined(nperf_context) && !defined(ios_cross_compile)
  user_key_comparison_count = 0;
  block_cache_hit_count = 0;
  block_read_count = 0;
  block_read_byte = 0;
  block_read_time = 0;
  block_checksum_time = 0;
  block_decompress_time = 0;
  internal_key_skipped_count = 0;
  internal_delete_skipped_count = 0;
  write_wal_time = 0;

  get_snapshot_time = 0;
  get_from_memtable_time = 0;
  get_from_memtable_count = 0;
  get_post_process_time = 0;
  get_from_output_files_time = 0;
  seek_child_seek_time = 0;
  seek_child_seek_count = 0;
  seek_min_heap_time = 0;
  seek_internal_seek_time = 0;
  find_next_user_entry_time = 0;
  write_pre_and_post_process_time = 0;
  write_memtable_time = 0;
#endif
}

#define output(counter) #counter << " = " << counter << ", "

std::string perfcontext::tostring() const {
#if defined(nperf_context) || defined(ios_cross_compile)
  return "";
#else
  std::ostringstream ss;
  ss << output(user_key_comparison_count)
     << output(block_cache_hit_count)
     << output(block_read_count)
     << output(block_read_byte)
     << output(block_read_time)
     << output(block_checksum_time)
     << output(block_decompress_time)
     << output(internal_key_skipped_count)
     << output(internal_delete_skipped_count)
     << output(write_wal_time)
     << output(get_snapshot_time)
     << output(get_from_memtable_time)
     << output(get_from_memtable_count)
     << output(get_post_process_time)
     << output(get_from_output_files_time)
     << output(seek_child_seek_time)
     << output(seek_child_seek_count)
     << output(seek_min_heap_time)
     << output(seek_internal_seek_time)
     << output(find_next_user_entry_time)
     << output(write_pre_and_post_process_time)
     << output(write_memtable_time);
  return ss.str();
#endif
}

}
