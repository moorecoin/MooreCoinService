// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

public enum histogramtype {
  db_get(0),
  db_write(1),
  compaction_time(2),
  table_sync_micros(3),
  compaction_outfile_sync_micros(4),
  wal_file_sync_micros(5),
  manifest_file_sync_micros(6),
  // time spent in io during table open
  table_open_io_micros(7),
  db_multiget(8),
  read_block_compaction_micros(9),
  read_block_get_micros(10),
  write_raw_block_micros(11),

  stall_l0_slowdown_count(12),
  stall_memtable_compaction_count(13),
  stall_l0_num_files_count(14),
  hard_rate_limit_delay_count(15),
  soft_rate_limit_delay_count(16),
  num_files_in_single_compaction(17);

  private final int value_;

  private histogramtype(int value) {
    value_ = value;
  }

  public int getvalue() {
    return value_;
  }
}
