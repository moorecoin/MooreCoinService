// copyright (c) 2013, facebook, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#ifndef storage_rocksdb_include_options_h_
#define storage_rocksdb_include_options_h_

#include <stddef.h>
#include <string>
#include <memory>
#include <vector>
#include <stdint.h>

#include "rocksdb/version.h"
#include "rocksdb/universal_compaction.h"

namespace rocksdb {

class cache;
class compactionfilter;
class compactionfilterfactory;
class compactionfilterfactoryv2;
class comparator;
class env;
enum infologlevel : unsigned char;
class filterpolicy;
class logger;
class mergeoperator;
class snapshot;
class tablefactory;
class memtablerepfactory;
class tablepropertiescollectorfactory;
class ratelimiter;
class slice;
class slicetransform;
class statistics;
class internalkeycomparator;

// db contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  each block may be compressed before
// being stored in a file.  the following enum describes which
// compression method (if any) is used to compress a block.
enum compressiontype : char {
  // note: do not change the values of existing entries, as these are
  // part of the persistent format on disk.
  knocompression = 0x0, ksnappycompression = 0x1, kzlibcompression = 0x2,
  kbzip2compression = 0x3, klz4compression = 0x4, klz4hccompression = 0x5
};

enum compactionstyle : char {
  kcompactionstylelevel = 0x0,      // level based compaction style
  kcompactionstyleuniversal = 0x1,  // universal compaction style
  kcompactionstylefifo = 0x2,       // fifo compaction style
};

struct compactionoptionsfifo {
  // once the total sum of table files reaches this, we will delete the oldest
  // table file
  // default: 1gb
  uint64_t max_table_files_size;

  compactionoptionsfifo() : max_table_files_size(1 * 1024 * 1024 * 1024) {}
};

// compression options for different compression algorithms like zlib
struct compressionoptions {
  int window_bits;
  int level;
  int strategy;
  compressionoptions() : window_bits(-14), level(-1), strategy(0) {}
  compressionoptions(int wbits, int _lev, int _strategy)
      : window_bits(wbits), level(_lev), strategy(_strategy) {}
};

enum updatestatus {    // return status for inplace update callback
  update_failed   = 0, // nothing to update
  updated_inplace = 1, // value updated inplace
  updated         = 2, // no inplace update. merged value set
};

struct dbpath {
  std::string path;
  uint64_t target_size;  // target size of total files under the path, in byte.

  dbpath() : target_size(0) {}
  dbpath(const std::string& p, uint64_t t) : path(p), target_size(t) {}
};

struct options;

struct columnfamilyoptions {
  // some functions that make it easier to optimize rocksdb

  // use this if you don't need to keep the data sorted, i.e. you'll never use
  // an iterator, only put() and get() api calls
  columnfamilyoptions* optimizeforpointlookup(
      uint64_t block_cache_size_mb);

  // default values for some parameters in columnfamilyoptions are not
  // optimized for heavy workloads and big datasets, which means you might
  // observe write stalls under some conditions. as a starting point for tuning
  // rocksdb options, use the following two functions:
  // * optimizelevelstylecompaction -- optimizes level style compaction
  // * optimizeuniversalstylecompaction -- optimizes universal style compaction
  // universal style compaction is focused on reducing write amplification
  // factor for big data sets, but increases space amplification. you can learn
  // more about the different styles here:
  // https://github.com/facebook/rocksdb/wiki/rocksdb-architecture-guide
  // make sure to also call increaseparallelism(), which will provide the
  // biggest performance gains.
  // note: we might use more memory than memtable_memory_budget during high
  // write rate period
  columnfamilyoptions* optimizelevelstylecompaction(
      uint64_t memtable_memory_budget = 512 * 1024 * 1024);
  columnfamilyoptions* optimizeuniversalstylecompaction(
      uint64_t memtable_memory_budget = 512 * 1024 * 1024);

  // -------------------
  // parameters that affect behavior

  // comparator used to define the order of keys in the table.
  // default: a comparator that uses lexicographic byte-wise ordering
  //
  // requires: the client must ensure that the comparator supplied
  // here has the same name and orders keys *exactly* the same as the
  // comparator provided to previous open calls on the same db.
  const comparator* comparator;

  // requires: the client must provide a merge operator if merge operation
  // needs to be accessed. calling merge on a db without a merge operator
  // would result in status::notsupported. the client must ensure that the
  // merge operator supplied here has the same name and *exactly* the same
  // semantics as the merge operator provided to previous open calls on
  // the same db. the only exception is reserved for upgrade, where a db
  // previously without a merge operator is introduced to merge operation
  // for the first time. it's necessary to specify a merge operator when
  // openning the db in this case.
  // default: nullptr
  std::shared_ptr<mergeoperator> merge_operator;

  // a single compactionfilter instance to call into during compaction.
  // allows an application to modify/delete a key-value during background
  // compaction.
  //
  // if the client requires a new compaction filter to be used for different
  // compaction runs, it can specify compaction_filter_factory instead of this
  // option.  the client should specify only one of the two.
  // compaction_filter takes precedence over compaction_filter_factory if
  // client specifies both.
  //
  // if multithreaded compaction is being used, the supplied compactionfilter
  // instance may be used from different threads concurrently and so should be
  // thread-safe.
  //
  // default: nullptr
  const compactionfilter* compaction_filter;

  // this is a factory that provides compaction filter objects which allow
  // an application to modify/delete a key-value during background compaction.
  //
  // a new filter will be created on each compaction run.  if multithreaded
  // compaction is being used, each created compactionfilter will only be used
  // from a single thread and so does not need to be thread-safe.
  //
  // default: a factory that doesn't provide any object
  std::shared_ptr<compactionfilterfactory> compaction_filter_factory;

  // version two of the compaction_filter_factory
  // it supports rolling compaction
  //
  // default: a factory that doesn't provide any object
  std::shared_ptr<compactionfilterfactoryv2> compaction_filter_factory_v2;

  // -------------------
  // parameters that affect performance

  // amount of data to build up in memory (backed by an unsorted log
  // on disk) before converting to a sorted on-disk file.
  //
  // larger values increase performance, especially during bulk loads.
  // up to max_write_buffer_number write buffers may be held in memory
  // at the same time,
  // so you may wish to adjust this parameter to control memory usage.
  // also, a larger write buffer will result in a longer recovery time
  // the next time the database is opened.
  //
  // default: 4mb
  size_t write_buffer_size;

  // the maximum number of write buffers that are built up in memory.
  // the default and the minimum number is 2, so that when 1 write buffer
  // is being flushed to storage, new writes can continue to the other
  // write buffer.
  // default: 2
  int max_write_buffer_number;

  // the minimum number of write buffers that will be merged together
  // before writing to storage.  if set to 1, then
  // all write buffers are fushed to l0 as individual files and this increases
  // read amplification because a get request has to check in all of these
  // files. also, an in-memory merge may result in writing lesser
  // data to storage if there are duplicate records in each of these
  // individual write buffers.  default: 1
  int min_write_buffer_number_to_merge;

  // compress blocks using the specified compression algorithm.  this
  // parameter can be changed dynamically.
  //
  // default: ksnappycompression, which gives lightweight but fast
  // compression.
  //
  // typical speeds of ksnappycompression on an intel(r) core(tm)2 2.4ghz:
  //    ~200-500mb/s compression
  //    ~400-800mb/s decompression
  // note that these speeds are significantly faster than most
  // persistent storage speeds, and therefore it is typically never
  // worth switching to knocompression.  even if the input data is
  // incompressible, the ksnappycompression implementation will
  // efficiently detect that and will switch to uncompressed mode.
  compressiontype compression;

  // different levels can have different compression policies. there
  // are cases where most lower levels would like to quick compression
  // algorithm while the higher levels (which have more data) use
  // compression algorithms that have better compression but could
  // be slower. this array, if non nullptr, should have an entry for
  // each level of the database. this array, if non nullptr, overides the
  // value specified in the previous field 'compression'. the caller is
  // reponsible for allocating memory and initializing the values in it
  // before invoking open(). the caller is responsible for freeing this
  // array and it could be freed anytime after the return from open().
  // this could have been a std::vector but that makes the equivalent
  // java/c api hard to construct.
  std::vector<compressiontype> compression_per_level;

  // different options for compression algorithms
  compressionoptions compression_opts;

  // if non-nullptr, use the specified function to determine the
  // prefixes for keys.  these prefixes will be placed in the filter.
  // depending on the workload, this can reduce the number of read-iop
  // cost for scans when a prefix is passed via readoptions to
  // db.newiterator().  for prefix filtering to work properly,
  // "prefix_extractor" and "comparator" must be such that the following
  // properties hold:
  //
  // 1) key.starts_with(prefix(key))
  // 2) compare(prefix(key), key) <= 0.
  // 3) if compare(k1, k2) <= 0, then compare(prefix(k1), prefix(k2)) <= 0
  // 4) prefix(prefix(key)) == prefix(key)
  //
  // default: nullptr
  std::shared_ptr<const slicetransform> prefix_extractor;

  // number of levels for this database
  int num_levels;

  // number of files to trigger level-0 compaction. a value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  //
  // default: 4
  int level0_file_num_compaction_trigger;

  // soft limit on number of level-0 files. we start slowing down writes at this
  // point. a value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  int level0_slowdown_writes_trigger;

  // maximum number of level-0 files.  we stop writes at this point.
  int level0_stop_writes_trigger;

  // maximum level to which a new compacted memtable is pushed if it
  // does not create overlap.  we try to push to level 2 to avoid the
  // relatively expensive level 0=>1 compactions and to avoid some
  // expensive manifest file operations.  we do not push all the way to
  // the largest level since that can generate a lot of wasted disk
  // space if the same key space is being repeatedly overwritten.
  int max_mem_compaction_level;

  // target file size for compaction.
  // target_file_size_base is per-file size for level-1.
  // target file size for level l can be calculated by
  // target_file_size_base * (target_file_size_multiplier ^ (l-1))
  // for example, if target_file_size_base is 2mb and
  // target_file_size_multiplier is 10, then each file on level-1 will
  // be 2mb, and each file on level 2 will be 20mb,
  // and each file on level-3 will be 200mb.

  // by default target_file_size_base is 2mb.
  int target_file_size_base;
  // by default target_file_size_multiplier is 1, which means
  // by default files in different levels will have similar size.
  int target_file_size_multiplier;

  // control maximum total data size for a level.
  // max_bytes_for_level_base is the max total for level-1.
  // maximum number of bytes for level l can be calculated as
  // (max_bytes_for_level_base) * (max_bytes_for_level_multiplier ^ (l-1))
  // for example, if max_bytes_for_level_base is 20mb, and if
  // max_bytes_for_level_multiplier is 10, total data size for level-1
  // will be 20mb, total file size for level-2 will be 200mb,
  // and total file size for level-3 will be 2gb.

  // by default 'max_bytes_for_level_base' is 10mb.
  uint64_t max_bytes_for_level_base;
  // by default 'max_bytes_for_level_base' is 10.
  int max_bytes_for_level_multiplier;

  // different max-size multipliers for different levels.
  // these are multiplied by max_bytes_for_level_multiplier to arrive
  // at the max-size of each level.
  // default: 1
  std::vector<int> max_bytes_for_level_multiplier_additional;

  // maximum number of bytes in all compacted files.  we avoid expanding
  // the lower level file set of a compaction if it would make the
  // total compaction cover more than
  // (expanded_compaction_factor * targetfilesizelevel()) many bytes.
  int expanded_compaction_factor;

  // maximum number of bytes in all source files to be compacted in a
  // single compaction run. we avoid picking too many files in the
  // source level so that we do not exceed the total source bytes
  // for compaction to exceed
  // (source_compaction_factor * targetfilesizelevel()) many bytes.
  // default:1, i.e. pick maxfilesize amount of data as the source of
  // a compaction.
  int source_compaction_factor;

  // control maximum bytes of overlaps in grandparent (i.e., level+2) before we
  // stop building a single file in a level->level+1 compaction.
  int max_grandparent_overlap_factor;

  // puts are delayed 0-1 ms when any level has a compaction score that exceeds
  // soft_rate_limit. this is ignored when == 0.0.
  // constraint: soft_rate_limit <= hard_rate_limit. if this constraint does not
  // hold, rocksdb will set soft_rate_limit = hard_rate_limit
  // default: 0 (disabled)
  double soft_rate_limit;

  // puts are delayed 1ms at a time when any level has a compaction score that
  // exceeds hard_rate_limit. this is ignored when <= 1.0.
  // default: 0 (disabled)
  double hard_rate_limit;

  // max time a put will be stalled when hard_rate_limit is enforced. if 0, then
  // there is no limit.
  // default: 1000
  unsigned int rate_limit_delay_max_milliseconds;

  // size of one block in arena memory allocation.
  // if <= 0, a proper value is automatically calculated (usually 1/10 of
  // writer_buffer_size).
  //
  // there are two additonal restriction of the the specified size:
  // (1) size should be in the range of [4096, 2 << 30] and
  // (2) be the multiple of the cpu word (which helps with the memory
  // alignment).
  //
  // we'll automatically check and adjust the size number to make sure it
  // conforms to the restrictions.
  //
  // default: 0
  size_t arena_block_size;

  // disable automatic compactions. manual compactions can still
  // be issued on this column family
  bool disable_auto_compactions;

  // purge duplicate/deleted keys when a memtable is flushed to storage.
  // default: true
  bool purge_redundant_kvs_while_flush;

  // the compaction style. default: kcompactionstylelevel
  compactionstyle compaction_style;

  // if true, compaction will verify checksum on every read that happens
  // as part of compaction
  // default: true
  bool verify_checksums_in_compaction;

  // the options needed to support universal style compactions
  compactionoptionsuniversal compaction_options_universal;

  // the options for fifo compaction style
  compactionoptionsfifo compaction_options_fifo;

  // use keymayexist api to filter deletes when this is true.
  // if keymayexist returns false, i.e. the key definitely does not exist, then
  // the delete is a noop. keymayexist only incurs in-memory look up.
  // this optimization avoids writing the delete to storage when appropriate.
  // default: false
  bool filter_deletes;

  // an iteration->next() sequentially skips over keys with the same
  // user-key unless this option is set. this number specifies the number
  // of keys (with the same userkey) that will be sequentially
  // skipped before a reseek is issued.
  // default: 8
  uint64_t max_sequential_skip_in_iterations;

  // this is a factory that provides memtablerep objects.
  // default: a factory that provides a skip-list-based implementation of
  // memtablerep.
  std::shared_ptr<memtablerepfactory> memtable_factory;

  // this is a factory that provides tablefactory objects.
  // default: a block-based table factory that provides a default
  // implementation of tablebuilder and tablereader with default
  // blockbasedtableoptions.
  std::shared_ptr<tablefactory> table_factory;

  // block-based table related options are moved to blockbasedtableoptions.
  // related options that were originally here but now moved include:
  //   no_block_cache
  //   block_cache
  //   block_cache_compressed
  //   block_size
  //   block_size_deviation
  //   block_restart_interval
  //   filter_policy
  //   whole_key_filtering
  // if you'd like to customize some of these options, you will need to
  // use newblockbasedtablefactory() to construct a new table factory.

  // this option allows user to to collect their own interested statistics of
  // the tables.
  // default: empty vector -- no user-defined statistics collection will be
  // performed.
  typedef std::vector<std::shared_ptr<tablepropertiescollectorfactory>>
      tablepropertiescollectorfactories;
  tablepropertiescollectorfactories table_properties_collector_factories;

  // allows thread-safe inplace updates. if this is true, there is no way to
  // achieve point-in-time consistency using snapshot or iterator (assuming
  // concurrent updates).
  // if inplace_callback function is not set,
  //   put(key, new_value) will update inplace the existing_value iff
  //   * key exists in current memtable
  //   * new sizeof(new_value) <= sizeof(existing_value)
  //   * existing_value for that key is a put i.e. ktypevalue
  // if inplace_callback function is set, check doc for inplace_callback.
  // default: false.
  bool inplace_update_support;

  // number of locks used for inplace update
  // default: 10000, if inplace_update_support = true, else 0.
  size_t inplace_update_num_locks;

  // existing_value - pointer to previous value (from both memtable and sst).
  //                  nullptr if key doesn't exist
  // existing_value_size - pointer to size of existing_value).
  //                       nullptr if key doesn't exist
  // delta_value - delta value to be merged with the existing_value.
  //               stored in transaction logs.
  // merged_value - set when delta is applied on the previous value.

  // applicable only when inplace_update_support is true,
  // this callback function is called at the time of updating the memtable
  // as part of a put operation, lets say put(key, delta_value). it allows the
  // 'delta_value' specified as part of the put operation to be merged with
  // an 'existing_value' of the key in the database.

  // if the merged value is smaller in size that the 'existing_value',
  // then this function can update the 'existing_value' buffer inplace and
  // the corresponding 'existing_value'_size pointer, if it wishes to.
  // the callback should return updatestatus::updated_inplace.
  // in this case. (in this case, the snapshot-semantics of the rocksdb
  // iterator is not atomic anymore).

  // if the merged value is larger in size than the 'existing_value' or the
  // application does not wish to modify the 'existing_value' buffer inplace,
  // then the merged value should be returned via *merge_value. it is set by
  // merging the 'existing_value' and the put 'delta_value'. the callback should
  // return updatestatus::updated in this case. this merged value will be added
  // to the memtable.

  // if merging fails or the application does not wish to take any action,
  // then the callback should return updatestatus::update_failed.

  // please remember that the original call from the application is put(key,
  // delta_value). so the transaction log (if enabled) will still contain (key,
  // delta_value). the 'merged_value' is not stored in the transaction log.
  // hence the inplace_callback function should be consistent across db reopens.

  // default: nullptr
  updatestatus (*inplace_callback)(char* existing_value,
                                   uint32_t* existing_value_size,
                                   slice delta_value,
                                   std::string* merged_value);

  // if prefix_extractor is set and bloom_bits is not 0, create prefix bloom
  // for memtable
  uint32_t memtable_prefix_bloom_bits;

  // number of hash probes per key
  uint32_t memtable_prefix_bloom_probes;

  // page size for huge page tlb for bloom in memtable. if <=0, not allocate
  // from huge page tlb but from malloc.
  // need to reserve huge pages for it to be allocated. for example:
  //      sysctl -w vm.nr_hugepages=20
  // see linux doc documentation/vm/hugetlbpage.txt

  size_t memtable_prefix_bloom_huge_page_tlb_size;

  // control locality of bloom filter probes to improve cache miss rate.
  // this option only applies to memtable prefix bloom and plaintable
  // prefix bloom. it essentially limits every bloom checking to one cache line.
  // this optimization is turned off when set to 0, and positive number to turn
  // it on.
  // default: 0
  uint32_t bloom_locality;

  // maximum number of successive merge operations on a key in the memtable.
  //
  // when a merge operation is added to the memtable and the maximum number of
  // successive merges is reached, the value of the key will be calculated and
  // inserted into the memtable instead of the merge operation. this will
  // ensure that there are never more than max_successive_merges merge
  // operations in the memtable.
  //
  // default: 0 (disabled)
  size_t max_successive_merges;

  // the number of partial merge operands to accumulate before partial
  // merge will be performed. partial merge will not be called
  // if the list of values to merge is less than min_partial_merge_operands.
  //
  // if min_partial_merge_operands < 2, then it will be treated as 2.
  //
  // default: 2
  uint32_t min_partial_merge_operands;

  // create columnfamilyoptions with default values for all fields
  columnfamilyoptions();
  // create columnfamilyoptions from options
  explicit columnfamilyoptions(const options& options);

  void dump(logger* log) const;
};

struct dboptions {
  // some functions that make it easier to optimize rocksdb

  // by default, rocksdb uses only one background thread for flush and
  // compaction. calling this function will set it up such that total of
  // `total_threads` is used. good value for `total_threads` is the number of
  // cores. you almost definitely want to call this function if your system is
  // bottlenecked by rocksdb.
  dboptions* increaseparallelism(int total_threads = 16);

  // if true, the database will be created if it is missing.
  // default: false
  bool create_if_missing;

  // if true, missing column families will be automatically created.
  // default: false
  bool create_missing_column_families;

  // if true, an error is raised if the database already exists.
  // default: false
  bool error_if_exists;

  // if true, the implementation will do aggressive checking of the
  // data it is processing and will stop early if it detects any
  // errors.  this may have unforeseen ramifications: for example, a
  // corruption of one db entry may cause a large number of entries to
  // become unreadable or for the entire db to become unopenable.
  // if any of the  writes to the database fails (put, delete, merge, write),
  // the database will switch to read-only mode and fail all other
  // write operations.
  // default: true
  bool paranoid_checks;

  // use the specified object to interact with the environment,
  // e.g. to read/write files, schedule background work, etc.
  // default: env::default()
  env* env;

  // use to control write rate of flush and compaction. flush has higher
  // priority than compaction. rate limiting is disabled if nullptr.
  // if rate limiter is enabled, bytes_per_sync is set to 1mb by default.
  // default: nullptr
  std::shared_ptr<ratelimiter> rate_limiter;

  // any internal progress/error information generated by the db will
  // be written to info_log if it is non-nullptr, or to a file stored
  // in the same directory as the db contents if info_log is nullptr.
  // default: nullptr
  std::shared_ptr<logger> info_log;

  infologlevel info_log_level;

  // number of open files that can be used by the db.  you may need to
  // increase this if your database has a large working set. value -1 means
  // files opened are always kept open. you can estimate number of files based
  // on target_file_size_base and target_file_size_multiplier for level-based
  // compaction. for universal-style compaction, you can usually set it to -1.
  // default: 5000
  int max_open_files;

  // once write-ahead logs exceed this size, we will start forcing the flush of
  // column families whose memtables are backed by the oldest live wal file
  // (i.e. the ones that are causing all the space amplification). if set to 0
  // (default), we will dynamically choose the wal size limit to be
  // [sum of all write_buffer_size * max_write_buffer_number] * 2
  // default: 0
  uint64_t max_total_wal_size;

  // if non-null, then we should collect metrics about database operations
  // statistics objects should not be shared between db instances as
  // it does not use any locks to prevent concurrent updates.
  std::shared_ptr<statistics> statistics;

  // if true, then the contents of data files are not synced
  // to stable storage. their contents remain in the os buffers till the
  // os decides to flush them. this option is good for bulk-loading
  // of data. once the bulk-loading is complete, please issue a
  // sync to the os to flush all dirty buffesrs to stable storage.
  // default: false
  bool disabledatasync;

  // if true, then every store to stable storage will issue a fsync.
  // if false, then every store to stable storage will issue a fdatasync.
  // this parameter should be set to true while storing data to
  // filesystem like ext3 that can lose files after a reboot.
  // default: false
  bool use_fsync;

  // a list of paths where sst files can be put into, with its target size.
  // newer data is placed into paths specified earlier in the vector while
  // older data gradually moves to paths specified later in the vector.
  //
  // for example, you have a flash device with 10gb allocated for the db,
  // as well as a hard drive of 2tb, you should config it to be:
  //   [{"/flash_path", 10gb}, {"/hard_drive", 2tb}]
  //
  // the system will try to guarantee data under each path is close to but
  // not larger than the target size. but current and future file sizes used
  // by determining where to place a file are based on best-effort estimation,
  // which means there is a chance that the actual size under the directory
  // is slightly more than target size under some workloads. user should give
  // some buffer room for those cases.
  //
  // if none of the paths has sufficient room to place a file, the file will
  // be placed to the last path anyway, despite to the target size.
  //
  // placing newer data to ealier paths is also best-efforts. user should
  // expect user files to be placed in higher levels in some extreme cases.
  //
  // if left empty, only one path will be used, which is db_name passed when
  // opening the db.
  // default: empty
  std::vector<dbpath> db_paths;

  // this specifies the info log dir.
  // if it is empty, the log files will be in the same dir as data.
  // if it is non empty, the log files will be in the specified dir,
  // and the db data dir's absolute path will be used as the log file
  // name's prefix.
  std::string db_log_dir;

  // this specifies the absolute dir path for write-ahead logs (wal).
  // if it is empty, the log files will be in the same dir as data,
  //   dbname is used as the data dir by default
  // if it is non empty, the log files will be in kept the specified dir.
  // when destroying the db,
  //   all log files in wal_dir and the dir itself is deleted
  std::string wal_dir;

  // the periodicity when obsolete files get deleted. the default
  // value is 6 hours. the files that get out of scope by compaction
  // process will still get automatically delete on every compaction,
  // regardless of this setting
  uint64_t delete_obsolete_files_period_micros;

  // maximum number of concurrent background compaction jobs, submitted to
  // the default low priority thread pool.
  // if you're increasing this, also consider increasing number of threads in
  // low priority thread pool. for more information, see
  // env::setbackgroundthreads
  // default: 1
  int max_background_compactions;

  // maximum number of concurrent background memtable flush jobs, submitted to
  // the high priority thread pool.
  //
  // by default, all background jobs (major compaction and memtable flush) go
  // to the low priority pool. if this option is set to a positive number,
  // memtable flush jobs will be submitted to the high priority pool.
  // it is important when the same env is shared by multiple db instances.
  // without a separate pool, long running major compaction jobs could
  // potentially block memtable flush jobs of other db instances, leading to
  // unnecessary put stalls.
  //
  // if you're increasing this, also consider increasing number of threads in
  // high priority thread pool. for more information, see
  // env::setbackgroundthreads
  // default: 1
  int max_background_flushes;

  // specify the maximal size of the info log file. if the log file
  // is larger than `max_log_file_size`, a new info log file will
  // be created.
  // if max_log_file_size == 0, all logs will be written to one
  // log file.
  size_t max_log_file_size;

  // time for the info log file to roll (in seconds).
  // if specified with non-zero value, log file will be rolled
  // if it has been active longer than `log_file_time_to_roll`.
  // default: 0 (disabled)
  size_t log_file_time_to_roll;

  // maximal info log files to be kept.
  // default: 1000
  size_t keep_log_file_num;

  // manifest file is rolled over on reaching this limit.
  // the older manifest file be deleted.
  // the default value is max_int so that roll-over does not take place.
  uint64_t max_manifest_file_size;

  // number of shards used for table cache.
  int table_cache_numshardbits;

  // during data eviction of table's lru cache, it would be inefficient
  // to strictly follow lru because this piece of memory will not really
  // be released unless its refcount falls to zero. instead, make two
  // passes: the first pass will release items with refcount = 1,
  // and if not enough space releases after scanning the number of
  // elements specified by this parameter, we will remove items in lru
  // order.
  int table_cache_remove_scan_count_limit;

  // the following two fields affect how archived logs will be deleted.
  // 1. if both set to 0, logs will be deleted asap and will not get into
  //    the archive.
  // 2. if wal_ttl_seconds is 0 and wal_size_limit_mb is not 0,
  //    wal files will be checked every 10 min and if total size is greater
  //    then wal_size_limit_mb, they will be deleted starting with the
  //    earliest until size_limit is met. all empty files will be deleted.
  // 3. if wal_ttl_seconds is not 0 and wal_size_limit_mb is 0, then
  //    wal files will be checked every wal_ttl_secondsi / 2 and those that
  //    are older than wal_ttl_seconds will be deleted.
  // 4. if both are not 0, wal files will be checked every 10 min and both
  //    checks will be performed with ttl being first.
  uint64_t wal_ttl_seconds;
  uint64_t wal_size_limit_mb;

  // number of bytes to preallocate (via fallocate) the manifest
  // files.  default is 4mb, which is reasonable to reduce random io
  // as well as prevent overallocation for mounts that preallocate
  // large amounts of data (such as xfs's allocsize option).
  size_t manifest_preallocation_size;

  // data being read from file storage may be buffered in the os
  // default: true
  bool allow_os_buffer;

  // allow the os to mmap file for reading sst tables. default: false
  bool allow_mmap_reads;

  // allow the os to mmap file for writing. default: false
  bool allow_mmap_writes;

  // disable child process inherit open files. default: true
  bool is_fd_close_on_exec;

  // skip log corruption error on recovery (if client is ok with
  // losing most recent changes)
  // default: false
  bool skip_log_error_on_recovery;

  // if not zero, dump rocksdb.stats to log every stats_dump_period_sec
  // default: 3600 (1 hour)
  unsigned int stats_dump_period_sec;

  // if set true, will hint the underlying file system that the file
  // access pattern is random, when a sst file is opened.
  // default: true
  bool advise_random_on_open;

  // specify the file access pattern once a compaction is started.
  // it will be applied to all input files of a compaction.
  // default: normal
  enum {
    none,
    normal,
    sequential,
    willneed
  } access_hint_on_compaction_start;

  // use adaptive mutex, which spins in the user space before resorting
  // to kernel. this could reduce context switch when the mutex is not
  // heavily contended. however, if the mutex is hot, we could end up
  // wasting spin time.
  // default: false
  bool use_adaptive_mutex;

  // allow rocksdb to use thread local storage to optimize performance.
  // default: true
  bool allow_thread_local;

  // create dboptions with default values for all fields
  dboptions();
  // create dboptions from options
  explicit dboptions(const options& options);

  void dump(logger* log) const;

  // allows os to incrementally sync files to disk while they are being
  // written, asynchronously, in the background.
  // issue one request for every bytes_per_sync written. 0 turns it off.
  // default: 0
  //
  // you may consider using rate_limiter to regulate write rate to device.
  // when rate limiter is enabled, it automatically enables bytes_per_sync
  // to 1mb.
  uint64_t bytes_per_sync;
};

// options to control the behavior of a database (passed to db::open)
struct options : public dboptions, public columnfamilyoptions {
  // create an options object with default values for all fields.
  options() :
    dboptions(),
    columnfamilyoptions() {}

  options(const dboptions& db_options,
          const columnfamilyoptions& column_family_options)
      : dboptions(db_options), columnfamilyoptions(column_family_options) {}

  void dump(logger* log) const;

  // set appropriate parameters for bulk loading.
  // the reason that this is a function that returns "this" instead of a
  // constructor is to enable chaining of multiple similar calls in the future.
  //

  // all data will be in level 0 without any automatic compaction.
  // it's recommended to manually call compactrange(null, null) before reading
  // from the database, because otherwise the read can be very slow.
  options* prepareforbulkload();
};

//
// an application can issue a read request (via get/iterators) and specify
// if that read should process data that already resides on a specified cache
// level. for example, if an application specifies kblockcachetier then the
// get call will process data that is already processed in the memtable or
// the block cache. it will not page in data from the os cache or data that
// resides in storage.
enum readtier {
  kreadalltier = 0x0,    // data in memtable, block cache, os cache or storage
  kblockcachetier = 0x1  // data in memtable or block cache
};

// options that control read operations
struct readoptions {
  // if true, all data read from underlying storage will be
  // verified against corresponding checksums.
  // default: true
  bool verify_checksums;

  // should the "data block"/"index block"/"filter block" read for this
  // iteration be cached in memory?
  // callers may wish to set this field to false for bulk scans.
  // default: true
  bool fill_cache;

  // if this option is set and memtable implementation allows, seek
  // might only return keys with the same prefix as the seek-key
  //
  // ! deprecated: prefix_seek is on by default when prefix_extractor
  // is configured
  // bool prefix_seek;

  // if "snapshot" is non-nullptr, read as of the supplied snapshot
  // (which must belong to the db that is being read and which must
  // not have been released).  if "snapshot" is nullptr, use an impliicit
  // snapshot of the state at the beginning of this read operation.
  // default: nullptr
  const snapshot* snapshot;

  // if "prefix" is non-nullptr, and readoptions is being passed to
  // db.newiterator, only return results when the key begins with this
  // prefix.  this field is ignored by other calls (e.g., get).
  // options.prefix_extractor must also be set, and
  // prefix_extractor.inrange(prefix) must be true.  the iterator
  // returned by newiterator when this option is set will behave just
  // as if the underlying store did not contain any non-matching keys,
  // with two exceptions.  seek() only accepts keys starting with the
  // prefix, and seektolast() is not supported.  prefix filter with this
  // option will sometimes reduce the number of read iops.
  // default: nullptr
  //
  // ! deprecated
  // const slice* prefix;

  // specify if this read request should process data that already
  // resides on a particular cache. if the required data is not
  // found at the specified cache, then status::incomplete is returned.
  // default: kreadalltier
  readtier read_tier;

  // specify to create a tailing iterator -- a special iterator that has a
  // view of the complete database (i.e. it can also be used to read newly
  // added data) and is optimized for sequential reads. it will return records
  // that were inserted into the database after the creation of the iterator.
  // default: false
  // not supported in rocksdb_lite mode!
  bool tailing;

  // enable a total order seek regardless of index format (e.g. hash index)
  // used in the table. some table format (e.g. plain table) may not support
  // this option.
  bool total_order_seek;

  readoptions()
      : verify_checksums(true),
        fill_cache(true),
        snapshot(nullptr),
        read_tier(kreadalltier),
        tailing(false),
        total_order_seek(false) {}
  readoptions(bool cksum, bool cache)
      : verify_checksums(cksum),
        fill_cache(cache),
        snapshot(nullptr),
        read_tier(kreadalltier),
        tailing(false),
        total_order_seek(false) {}
};

// options that control write operations
struct writeoptions {
  // if true, the write will be flushed from the operating system
  // buffer cache (by calling writablefile::sync()) before the write
  // is considered complete.  if this flag is true, writes will be
  // slower.
  //
  // if this flag is false, and the machine crashes, some recent
  // writes may be lost.  note that if it is just the process that
  // crashes (i.e., the machine does not reboot), no writes will be
  // lost even if sync==false.
  //
  // in other words, a db write with sync==false has similar
  // crash semantics as the "write()" system call.  a db write
  // with sync==true has similar crash semantics to a "write()"
  // system call followed by "fdatasync()".
  //
  // default: false
  bool sync;

  // if true, writes will not first go to the write ahead log,
  // and the write may got lost after a crash.
  bool disablewal;

  // if non-zero, then associated write waiting longer than the specified
  // time may be aborted and returns status::timedout. a write that takes
  // less than the specified time is guaranteed to not fail with
  // status::timedout.
  //
  // the number of times a write call encounters a timeout is recorded in
  // statistics.write_timedout
  //
  // default: 0
  uint64_t timeout_hint_us;

  // if true and if user is trying to write to column families that don't exist
  // (they were dropped),  ignore the write (don't return an error). if there
  // are multiple writes in a writebatch, other writes will succeed.
  // default: false
  bool ignore_missing_column_families;

  writeoptions()
      : sync(false),
        disablewal(false),
        timeout_hint_us(0),
        ignore_missing_column_families(false) {}
};

// options that control flush operations
struct flushoptions {
  // if true, the flush will wait until the flush is done.
  // default: true
  bool wait;

  flushoptions() : wait(true) {}
};

// get options based on some guidelines. now only tune parameter based on
// flush/compaction and fill default parameters for other parameters.
// total_write_buffer_limit: budget for memory spent for mem tables
// read_amplification_threshold: comfortable value of read amplification
// write_amplification_threshold: comfortable value of write amplification.
// target_db_size: estimated total db size.
extern options getoptions(size_t total_write_buffer_limit,
                          int read_amplification_threshold = 8,
                          int write_amplification_threshold = 32,
                          uint64_t target_db_size = 68719476736 /* 64gb */);
}  // namespace rocksdb

#endif  // storage_rocksdb_include_options_h_
