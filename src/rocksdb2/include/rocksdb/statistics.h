// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

#ifndef storage_rocksdb_include_statistics_h_
#define storage_rocksdb_include_statistics_h_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>

namespace rocksdb {

/**
 * keep adding ticker's here.
 *  1. any ticker should be added before ticker_enum_max.
 *  2. add a readable string in tickersnamemap below for the newly added ticker.
 */
enum tickers : uint32_t {
  // total block cache misses
  // requires: block_cache_miss == block_cache_index_miss +
  //                               block_cache_filter_miss +
  //                               block_cache_data_miss;
  block_cache_miss = 0,
  // total block cache hit
  // requires: block_cache_hit == block_cache_index_hit +
  //                              block_cache_filter_hit +
  //                              block_cache_data_hit;
  block_cache_hit,
  // # of blocks added to block cache.
  block_cache_add,
  // # of times cache miss when accessing index block from block cache.
  block_cache_index_miss,
  // # of times cache hit when accessing index block from block cache.
  block_cache_index_hit,
  // # of times cache miss when accessing filter block from block cache.
  block_cache_filter_miss,
  // # of times cache hit when accessing filter block from block cache.
  block_cache_filter_hit,
  // # of times cache miss when accessing data block from block cache.
  block_cache_data_miss,
  // # of times cache hit when accessing data block from block cache.
  block_cache_data_hit,
  // # of times bloom filter has avoided file reads.
  bloom_filter_useful,

  // # of memtable hits.
  memtable_hit,
  // # of memtable misses.
  memtable_miss,

  /**
   * compaction_key_drop_* count the reasons for key drop during compaction
   * there are 3 reasons currently.
   */
  compaction_key_drop_newer_entry,  // key was written with a newer value.
  compaction_key_drop_obsolete,     // the key is obsolete.
  compaction_key_drop_user,  // user compaction function has dropped the key.

  // number of keys written to the database via the put and write call's
  number_keys_written,
  // number of keys read,
  number_keys_read,
  // number keys updated, if inplace update is enabled
  number_keys_updated,
  // bytes written / read
  bytes_written,
  bytes_read,
  no_file_closes,
  no_file_opens,
  no_file_errors,
  // time system had to wait to do lo-l1 compactions
  stall_l0_slowdown_micros,
  // time system had to wait to move memtable to l1.
  stall_memtable_compaction_micros,
  // write throttle because of too many files in l0
  stall_l0_num_files_micros,
  rate_limit_delay_millis,
  no_iterators,  // number of iterators currently open

  // number of multiget calls, keys read, and bytes read
  number_multiget_calls,
  number_multiget_keys_read,
  number_multiget_bytes_read,

  // number of deletes records that were not required to be
  // written to storage because key does not exist
  number_filtered_deletes,
  number_merge_failures,
  sequence_number,

  // number of times bloom was checked before creating iterator on a
  // file, and the number of times the check was useful in avoiding
  // iterator creation (and thus likely iops).
  bloom_filter_prefix_checked,
  bloom_filter_prefix_useful,

  // number of times we had to reseek inside an iteration to skip
  // over large number of keys with same userkey.
  number_of_reseeks_in_iteration,

  // record the number of calls to getupadtessince. useful to keep track of
  // transaction log iterator refreshes
  get_updates_since_calls,
  block_cache_compressed_miss,  // miss in the compressed block cache
  block_cache_compressed_hit,   // hit in the compressed block cache
  wal_file_synced,              // number of times wal sync is done
  wal_file_bytes,               // number of bytes written to wal

  // writes can be processed by requesting thread or by the thread at the
  // head of the writers queue.
  write_done_by_self,
  write_done_by_other,
  write_timedout,        // number of writes ending up with timed-out.
  write_with_wal,       // number of write calls that request wal
  compact_read_bytes,   // bytes read during compaction
  compact_write_bytes,  // bytes written during compaction
  flush_write_bytes,    // bytes written during flush

  // number of table's properties loaded directly from file, without creating
  // table reader object.
  number_direct_load_table_properties,
  number_superversion_acquires,
  number_superversion_releases,
  number_superversion_cleanups,
  number_block_not_compressed,
  ticker_enum_max
};

// the order of items listed in  tickers should be the same as
// the order listed in tickersnamemap
const std::vector<std::pair<tickers, std::string>> tickersnamemap = {
    {block_cache_miss, "rocksdb.block.cache.miss"},
    {block_cache_hit, "rocksdb.block.cache.hit"},
    {block_cache_add, "rocksdb.block.cache.add"},
    {block_cache_index_miss, "rocksdb.block.cache.index.miss"},
    {block_cache_index_hit, "rocksdb.block.cache.index.hit"},
    {block_cache_filter_miss, "rocksdb.block.cache.filter.miss"},
    {block_cache_filter_hit, "rocksdb.block.cache.filter.hit"},
    {block_cache_data_miss, "rocksdb.block.cache.data.miss"},
    {block_cache_data_hit, "rocksdb.block.cache.data.hit"},
    {bloom_filter_useful, "rocksdb.bloom.filter.useful"},
    {memtable_hit, "rocksdb.memtable.hit"},
    {memtable_miss, "rocksdb.memtable.miss"},
    {compaction_key_drop_newer_entry, "rocksdb.compaction.key.drop.new"},
    {compaction_key_drop_obsolete, "rocksdb.compaction.key.drop.obsolete"},
    {compaction_key_drop_user, "rocksdb.compaction.key.drop.user"},
    {number_keys_written, "rocksdb.number.keys.written"},
    {number_keys_read, "rocksdb.number.keys.read"},
    {number_keys_updated, "rocksdb.number.keys.updated"},
    {bytes_written, "rocksdb.bytes.written"},
    {bytes_read, "rocksdb.bytes.read"},
    {no_file_closes, "rocksdb.no.file.closes"},
    {no_file_opens, "rocksdb.no.file.opens"},
    {no_file_errors, "rocksdb.no.file.errors"},
    {stall_l0_slowdown_micros, "rocksdb.l0.slowdown.micros"},
    {stall_memtable_compaction_micros, "rocksdb.memtable.compaction.micros"},
    {stall_l0_num_files_micros, "rocksdb.l0.num.files.stall.micros"},
    {rate_limit_delay_millis, "rocksdb.rate.limit.delay.millis"},
    {no_iterators, "rocksdb.num.iterators"},
    {number_multiget_calls, "rocksdb.number.multiget.get"},
    {number_multiget_keys_read, "rocksdb.number.multiget.keys.read"},
    {number_multiget_bytes_read, "rocksdb.number.multiget.bytes.read"},
    {number_filtered_deletes, "rocksdb.number.deletes.filtered"},
    {number_merge_failures, "rocksdb.number.merge.failures"},
    {sequence_number, "rocksdb.sequence.number"},
    {bloom_filter_prefix_checked, "rocksdb.bloom.filter.prefix.checked"},
    {bloom_filter_prefix_useful, "rocksdb.bloom.filter.prefix.useful"},
    {number_of_reseeks_in_iteration, "rocksdb.number.reseeks.iteration"},
    {get_updates_since_calls, "rocksdb.getupdatessince.calls"},
    {block_cache_compressed_miss, "rocksdb.block.cachecompressed.miss"},
    {block_cache_compressed_hit, "rocksdb.block.cachecompressed.hit"},
    {wal_file_synced, "rocksdb.wal.synced"},
    {wal_file_bytes, "rocksdb.wal.bytes"},
    {write_done_by_self, "rocksdb.write.self"},
    {write_done_by_other, "rocksdb.write.other"},
    {write_timedout, "rocksdb.write.timedout"},
    {write_with_wal, "rocksdb.write.wal"},
    {flush_write_bytes, "rocksdb.flush.write.bytes"},
    {compact_read_bytes, "rocksdb.compact.read.bytes"},
    {compact_write_bytes, "rocksdb.compact.write.bytes"},
    {number_direct_load_table_properties,
     "rocksdb.number.direct.load.table.properties"},
    {number_superversion_acquires, "rocksdb.number.superversion_acquires"},
    {number_superversion_releases, "rocksdb.number.superversion_releases"},
    {number_superversion_cleanups, "rocksdb.number.superversion_cleanups"},
    {number_block_not_compressed, "rocksdb.number.block.not_compressed"},
};

/**
 * keep adding histogram's here.
 * any histogram whould have value less than histogram_enum_max
 * add a new histogram by assigning it the current value of histogram_enum_max
 * add a string representation in histogramsnamemap below
 * and increment histogram_enum_max
 */
enum histograms : uint32_t {
  db_get = 0,
  db_write,
  compaction_time,
  table_sync_micros,
  compaction_outfile_sync_micros,
  wal_file_sync_micros,
  manifest_file_sync_micros,
  // time spent in io during table open
  table_open_io_micros,
  db_multiget,
  read_block_compaction_micros,
  read_block_get_micros,
  write_raw_block_micros,

  stall_l0_slowdown_count,
  stall_memtable_compaction_count,
  stall_l0_num_files_count,
  hard_rate_limit_delay_count,
  soft_rate_limit_delay_count,
  num_files_in_single_compaction,
  db_seek,
  histogram_enum_max,
};

const std::vector<std::pair<histograms, std::string>> histogramsnamemap = {
  { db_get, "rocksdb.db.get.micros" },
  { db_write, "rocksdb.db.write.micros" },
  { compaction_time, "rocksdb.compaction.times.micros" },
  { table_sync_micros, "rocksdb.table.sync.micros" },
  { compaction_outfile_sync_micros, "rocksdb.compaction.outfile.sync.micros" },
  { wal_file_sync_micros, "rocksdb.wal.file.sync.micros" },
  { manifest_file_sync_micros, "rocksdb.manifest.file.sync.micros" },
  { table_open_io_micros, "rocksdb.table.open.io.micros" },
  { db_multiget, "rocksdb.db.multiget.micros" },
  { read_block_compaction_micros, "rocksdb.read.block.compaction.micros" },
  { read_block_get_micros, "rocksdb.read.block.get.micros" },
  { write_raw_block_micros, "rocksdb.write.raw.block.micros" },
  { stall_l0_slowdown_count, "rocksdb.l0.slowdown.count"},
  { stall_memtable_compaction_count, "rocksdb.memtable.compaction.count"},
  { stall_l0_num_files_count, "rocksdb.num.files.stall.count"},
  { hard_rate_limit_delay_count, "rocksdb.hard.rate.limit.delay.count"},
  { soft_rate_limit_delay_count, "rocksdb.soft.rate.limit.delay.count"},
  { num_files_in_single_compaction, "rocksdb.numfiles.in.singlecompaction" },
  { db_seek, "rocksdb.db.seek.micros" },
};

struct histogramdata {
  double median;
  double percentile95;
  double percentile99;
  double average;
  double standard_deviation;
};

// analyze the performance of a db
class statistics {
 public:
  virtual ~statistics() {}

  virtual uint64_t gettickercount(uint32_t tickertype) const = 0;
  virtual void histogramdata(uint32_t type,
                             histogramdata* const data) const = 0;

  virtual void recordtick(uint32_t tickertype, uint64_t count = 0) = 0;
  virtual void settickercount(uint32_t tickertype, uint64_t count) = 0;
  virtual void measuretime(uint32_t histogramtype, uint64_t time) = 0;

  // string representation of the statistic object.
  virtual std::string tostring() const {
    // do nothing by default
    return std::string("tostring(): not implemented");
  }

  // override this function to disable particular histogram collection
  virtual bool histenabledfortype(uint32_t type) const {
    return type < histogram_enum_max;
  }
};

// create a concrete dbstatistics object
std::shared_ptr<statistics> createdbstatistics();

}  // namespace rocksdb

#endif  // storage_rocksdb_include_statistics_h_
