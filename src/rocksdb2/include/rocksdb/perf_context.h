// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_include_perf_context_h
#define storage_rocksdb_include_perf_context_h

#include <stdint.h>
#include <string>

namespace rocksdb {

enum perflevel {
  kdisable        = 0,  // disable perf stats
  kenablecount    = 1,  // enable only count stats
  kenabletime     = 2   // enable time stats too
};

// set the perf stats level
void setperflevel(perflevel level);

// get current perf stats level
perflevel getperflevel();

// a thread local context for gathering performance counter efficiently
// and transparently.

struct perfcontext {

  void reset(); // reset all performance counters to zero

  std::string tostring() const;

  uint64_t user_key_comparison_count; // total number of user key comparisons
  uint64_t block_cache_hit_count;     // total number of block cache hits
  uint64_t block_read_count;          // total number of block reads (with io)
  uint64_t block_read_byte;           // total number of bytes from block reads
  uint64_t block_read_time;           // total time spent on block reads
  uint64_t block_checksum_time;       // total time spent on block checksum
  uint64_t block_decompress_time;     // total time spent on block decompression
  // total number of internal keys skipped over during iteration (overwritten or
  // deleted, to be more specific, hidden by a put or delete of the same key)
  uint64_t internal_key_skipped_count;
  // total number of deletes skipped over during iteration
  uint64_t internal_delete_skipped_count;

  uint64_t get_snapshot_time;          // total time spent on getting snapshot
  uint64_t get_from_memtable_time;     // total time spent on querying memtables
  uint64_t get_from_memtable_count;    // number of mem tables queried
  // total time spent after get() finds a key
  uint64_t get_post_process_time;
  uint64_t get_from_output_files_time; // total time reading from output files
  // total time spent on seeking child iters
  uint64_t seek_child_seek_time;
  // number of seek issued in child iterators
  uint64_t seek_child_seek_count;
  uint64_t seek_min_heap_time;         // total time spent on the merge heap
  // total time spent on seeking the internal entries
  uint64_t seek_internal_seek_time;
  // total time spent on iterating internal entries to find the next user entry
  uint64_t find_next_user_entry_time;
  // total time spent on pre or post processing when writing a record
  uint64_t write_pre_and_post_process_time;
  uint64_t write_wal_time;            // total time spent on writing to wal
  // total time spent on writing to mem tables
  uint64_t write_memtable_time;
};

#if defined(nperf_context) || defined(ios_cross_compile)
extern perfcontext perf_context;
#else
extern __thread perfcontext perf_context;
#endif

}

#endif
