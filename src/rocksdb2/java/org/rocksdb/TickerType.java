// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

public enum tickertype {
  // total block cache misses
  // requires: block_cache_miss == block_cache_index_miss +
  //                               block_cache_filter_miss +
  //                               block_cache_data_miss;
  block_cache_miss(0),
  // total block cache hit
  // requires: block_cache_hit == block_cache_index_hit +
  //                              block_cache_filter_hit +
  //                              block_cache_data_hit;
  block_cache_hit(1),
  // # of blocks added to block cache.
  block_cache_add(2),
  // # of times cache miss when accessing index block from block cache.
  block_cache_index_miss(3),
  // # of times cache hit when accessing index block from block cache.
  block_cache_index_hit(4),
  // # of times cache miss when accessing filter block from block cache.
  block_cache_filter_miss(5),
  // # of times cache hit when accessing filter block from block cache.
  block_cache_filter_hit(6),
  // # of times cache miss when accessing data block from block cache.
  block_cache_data_miss(7),
  // # of times cache hit when accessing data block from block cache.
  block_cache_data_hit(8),
  // # of times bloom filter has avoided file reads.
  bloom_filter_useful(9),

  // # of memtable hits.
  memtable_hit(10),
  // # of memtable misses.
  memtable_miss(11),

  /**
   * compaction_key_drop_* count the reasons for key drop during compaction
   * there are 3 reasons currently.
   */
  compaction_key_drop_newer_entry(12),  // key was written with a newer value.
  compaction_key_drop_obsolete(13),     // the key is obsolete.
  compaction_key_drop_user(14),  // user compaction function has dropped the key.

  // number of keys written to the database via the put and write call's
  number_keys_written(15),
  // number of keys read,
  number_keys_read(16),
  // number keys updated, if inplace update is enabled
  number_keys_updated(17),
  // bytes written / read
  bytes_written(18),
  bytes_read(19),
  no_file_closes(20),
  no_file_opens(21),
  no_file_errors(22),
  // time system had to wait to do lo-l1 compactions
  stall_l0_slowdown_micros(23),
  // time system had to wait to move memtable to l1.
  stall_memtable_compaction_micros(24),
  // write throttle because of too many files in l0
  stall_l0_num_files_micros(25),
  rate_limit_delay_millis(26),
  no_iterators(27),  // number of iterators currently open

  // number of multiget calls, keys read, and bytes read
  number_multiget_calls(28),
  number_multiget_keys_read(29),
  number_multiget_bytes_read(30),

  // number of deletes records that were not required to be
  // written to storage because key does not exist
  number_filtered_deletes(31),
  number_merge_failures(32),
  sequence_number(33),

  // number of times bloom was checked before creating iterator on a
  // file, and the number of times the check was useful in avoiding
  // iterator creation (and thus likely iops).
  bloom_filter_prefix_checked(34),
  bloom_filter_prefix_useful(35),

  // number of times we had to reseek inside an iteration to skip
  // over large number of keys with same userkey.
  number_of_reseeks_in_iteration(36),

  // record the number of calls to getupadtessince. useful to keep track of
  // transaction log iterator refreshes
  get_updates_since_calls(37),
  block_cache_compressed_miss(38),  // miss in the compressed block cache
  block_cache_compressed_hit(39),   // hit in the compressed block cache
  wal_file_synced(40),              // number of times wal sync is done
  wal_file_bytes(41),               // number of bytes written to wal

  // writes can be processed by requesting thread or by the thread at the
  // head of the writers queue.
  write_done_by_self(42),
  write_done_by_other(43),
  write_with_wal(44),       // number of write calls that request wal
  compact_read_bytes(45),   // bytes read during compaction
  compact_write_bytes(46),  // bytes written during compaction

  // number of table's properties loaded directly from file, without creating
  // table reader object.
  number_direct_load_table_properties(47),
  number_superversion_acquires(48),
  number_superversion_releases(49),
  number_superversion_cleanups(50);

  private final int value_;

  private tickertype(int value) {
    value_ = value;
  }

  public int getvalue() {
    return value_;
  }
}
