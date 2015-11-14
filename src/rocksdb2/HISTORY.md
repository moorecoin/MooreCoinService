# rocksdb change log

### unreleased

----- past releases -----

## 3.5.0 (9/3/2014)
### new features
* add include/utilities/write_batch_with_index.h, providing a utilitiy class to query data out of writebatch when building it.
* move blockbasedtable related options to blockbasedtableoptions from options. change corresponding jni interface. options affected include:
  no_block_cache, block_cache, block_cache_compressed, block_size, block_size_deviation, block_restart_interval, filter_policy, whole_key_filtering. filter_policy is changed to shared_ptr from a raw pointer.
* remove deprecated options: disable_seek_compaction and db_stats_log_interval
* optimizeforpointlookup() takes one parameter for block cache size. it now builds hash index, bloom filter, and block cache.

### public api changes
* the prefix extractor used with v2 compaction filters is now passed user key to slicetransform::transform instead of unparsed rocksdb key.

## 3.4.0 (8/18/2014)
### new features
* support multiple db paths in universal style compactions
* add feature of storing plain table index and bloom filter in sst file.
* compactrange() will never output compacted files to level 0. this used to be the case when all the compaction input files were at level 0.

### public api changes
* dboptions.db_paths now is a vector of a dbpath structure which indicates both of path and target size
* newplaintablefactory instead of bunch of parameters now accepts plaintableoptions, which is defined in include/rocksdb/table.h
* moved include/utilities/*.h to include/rocksdb/utilities/*.h
* statistics apis now take uint32_t as type instead of tickers. also make two access functions gettickercount and histogramdata const
* add db property rocksdb.estimate-num-keys, estimated number of live keys in db.
* add db::getintproperty(), which returns db properties that are integer as uint64_t.
* the prefix extractor used with v2 compaction filters is now passed user key to slicetransform::transform instead of unparsed rocksdb key.

## 3.3.0 (7/10/2014)
### new features
* added json api prototype.
* hashlinklist reduces performance outlier caused by skewed bucket by switching data in the bucket from linked list to skip list. add parameter threshold_use_skiplist in newhashlinklistrepfactory().
* rocksdb is now able to reclaim storage space more effectively during the compaction process.  this is done by compensating the size of each deletion entry by the 2x average value size, which makes compaction to be triggerred by deletion entries more easily.
* add timeout api to write.  now writeoptions have a variable called timeout_hint_us.  with timeout_hint_us set to non-zero, any write associated with this timeout_hint_us may be aborted when it runs longer than the specified timeout_hint_us, and it is guaranteed that any write completes earlier than the specified time-out will not be aborted due to the time-out condition.
* add a rate_limiter option, which controls total throughput of flush and compaction. the throughput is specified in bytes/sec. flush always has precedence over compaction when available bandwidth is constrained.

### public api changes
* removed newtotalorderplaintablefactory because it is not used and implemented semantically incorrect.

## 3.2.0 (06/20/2014)

### public api changes
* we removed seek compaction as a concept from rocksdb because:
1) it makes more sense for spinning disk workloads, while rocksdb is primarily designed for flash and memory,
2) it added some complexity to the important code-paths,
3) none of our internal customers were really using it.
because of that, options::disable_seek_compaction is now obsolete. it is still a parameter in options, so it does not break the build, but it does not have any effect. we plan to completely remove it at some point, so we ask users to please remove this option from your code base.
* add two paramters to newhashlinklistrepfactory() for logging on too many entries in a hash bucket when flushing.
* added new option blockbasedtableoptions::hash_index_allow_collision. when enabled, prefix hash index for block-based table will not store prefix and allow hash collision, reducing memory consumption.

### new features
* plaintable now supports a new key encoding: for keys of the same prefix, the prefix is only written once. it can be enabled through encoding_type paramter of newplaintablefactory()
* add adaptivetablefactory, which is used to convert from a db of plaintable to blockbasedtabe, or vise versa. it can be created using newadaptivetablefactory()

### performance improvements
* tailing iterator re-implemeted with forwarditerator + cascading search hint , see ~20% throughput improvement.

## 3.1.0 (05/21/2014)

### public api changes
* replaced columnfamilyoptions::table_properties_collectors with columnfamilyoptions::table_properties_collector_factories

### new features
* hash index for block-based table will be materialized and reconstructed more efficiently. previously hash index is constructed by scanning the whole table during every table open.
* fifo compaction style

## 3.0.0 (05/05/2014)

### public api changes
* added _level to all infologlevel enums
* deprecated readoptions.prefix and readoptions.prefix_seek. seek() defaults to prefix-based seek when options.prefix_extractor is supplied. more detail is documented in https://github.com/facebook/rocksdb/wiki/prefix-seek-api-changes
* memtablerepfactory::creatememtablerep() takes info logger as an extra parameter.

### new features
* column family support
* added an option to use different checksum functions in blockbasedtableoptions
* added applytoallcacheentries() function to cache

## 2.8.0 (04/04/2014)

* removed arena.h from public header files.
* by default, checksums are verified on every read from database
* change default value of several options, including: paranoid_checks=true, max_open_files=5000, level0_slowdown_writes_trigger=20, level0_stop_writes_trigger=24, disable_seek_compaction=true, max_background_flushes=1 and allow_mmap_writes=false
* added is_manual_compaction to compactionfilter::context
* added "virtual void waitforjoin()" in class env. default operation is no-op.
* removed backupengine::deletebackupsnewerthan() function
* added new option -- verify_checksums_in_compaction
* changed options.prefix_extractor from raw pointer to shared_ptr (take ownership)
  changed hashskiplistrepfactory and hashlinklistrepfactory constructor to not take slicetransform object (use options.prefix_extractor implicitly)
* added env::getthreadpoolqueuelen(), which returns the waiting queue length of thread pools
* added a command "checkconsistency" in ldb tool, which checks
  if file system state matches db state (file existence and file sizes)
* separate options related to block based table to a new struct blockbasedtableoptions.
* writebatch has a new function count() to return total size in the batch, and data() now returns a reference instead of a copy
* add more counters to perf context.
* supports several more db properties: compaction-pending, background-errors and cur-size-active-mem-table.

### new features
* if we find one truncated record at the end of the manifest or wal files,
  we will ignore it. we assume that writers of these records were interrupted
  and that we can safely ignore it.
* a new sst format "plaintable" is added, which is optimized for memory-only workloads. it can be created through newplaintablefactory() or newtotalorderplaintablefactory().
* a new mem table implementation hash linked list optimizing for the case that there are only few keys for each prefix, which can be created through newhashlinklistrepfactory().
* merge operator supports a new function partialmergemulti() to allow users to do partial merges against multiple operands.
* now compaction filter has a v2 interface. it buffers the kv-pairs sharing the same key prefix, process them in batches, and return the batched results back to db. the new interface uses a new structure compactionfiltercontext for the same purpose as compactionfilter::context in v1.
* geo-spatial support for locations and radial-search.

## 2.7.0 (01/28/2014)

### public api changes

* renamed `stackabledb::getrawdb()` to `stackabledb::getbasedb()`.
* renamed `writebatch::data()` `const std::string& data() const`.
* renamed class `tablestats` to `tableproperties`.
* deleted class `prefixhashrepfactory`. please use `newhashskiplistrepfactory()` instead.
* supported multi-threaded `enablefiledeletions()` and `disablefiledeletions()`.
* added `db::getoptions()`.
* added `db::getdbidentity()`.

### new features

* added [backupabledb](https://github.com/facebook/rocksdb/wiki/how-to-backup-rocksdb%3f)
* implemented [tailingiterator](https://github.com/facebook/rocksdb/wiki/tailing-iterator), a special type of iterator that
  doesn't create a snapshot (can be used to read newly inserted data)
  and is optimized for doing sequential reads.
* added property block for table, which allows (1) a table to store
  its metadata and (2) end user to collect and store properties they
  are interested in.
* enabled caching index and filter block in block cache (turned off by default).
* supported error report when doing manual compaction.
* supported additional linux platform flavors and mac os.
* put with `sliceparts` - variant of `put()` that gathers output like `writev(2)`
* bug fixes and code refactor for compatibility with upcoming column
  family feature.

### performance improvements

* huge benchmark performance improvements by multiple efforts. for example, increase in readonly qps from about 530k in 2.6 release to 1.1 million in 2.7 [1]
* speeding up a way rocksdb deleted obsolete files - no longer listing the whole directory under a lock -- decrease in p99
* use raw pointer instead of shared pointer for statistics: [5b825d](https://github.com/facebook/rocksdb/commit/5b825d6964e26ec3b4bb6faa708ebb1787f1d7bd) -- huge increase in performance -- shared pointers are slow
* optimized locking for `get()` -- [1fdb3f](https://github.com/facebook/rocksdb/commit/1fdb3f7dc60e96394e3e5b69a46ede5d67fb976c) -- 1.5x qps increase for some workloads
* cache speedup - [e8d40c3](https://github.com/facebook/rocksdb/commit/e8d40c31b3cca0c3e1ae9abe9b9003b1288026a9)
* implemented autovector, which allocates first n elements on stack. most of vectors in rocksdb are small. also, we never want to allocate heap objects while holding a mutex. -- [c01676e4](https://github.com/facebook/rocksdb/commit/c01676e46d3be08c3c140361ef1f5884f47d3b3c)
* lots of efforts to move malloc, memcpy and io outside of locks
