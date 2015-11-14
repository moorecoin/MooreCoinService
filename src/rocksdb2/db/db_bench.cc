//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#define __stdc_format_macros

#ifndef gflags
#include <cstdio>
int main() {
  fprintf(stderr, "please install gflags to run rocksdb tools\n");
  return 1;
}
#else

#ifdef numa
#include <numa.h>
#include <numaif.h>
#endif

#include <inttypes.h>
#include <cstddef>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <gflags/gflags.h>
#include "db/db_impl.h"
#include "db/version_set.h"
#include "rocksdb/options.h"
#include "rocksdb/cache.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/statistics.h"
#include "rocksdb/perf_context.h"
#include "port/port.h"
#include "port/stack_trace.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/string_util.h"
#include "util/statistics.h"
#include "util/testutil.h"
#include "util/xxhash.h"
#include "hdfs/env_hdfs.h"
#include "utilities/merge_operators.h"

using gflags::parsecommandlineflags;
using gflags::registerflagvalidator;
using gflags::setusagemessage;

define_string(benchmarks,
              "fillseq,"
              "fillsync,"
              "fillrandom,"
              "overwrite,"
              "readrandom,"
              "newiterator,"
              "newiteratorwhilewriting,"
              "seekrandom,"
              "seekrandomwhilewriting,"
              "readseq,"
              "readreverse,"
              "compact,"
              "readrandom,"
              "multireadrandom,"
              "readseq,"
              "readtocache,"
              "readreverse,"
              "readwhilewriting,"
              "readrandomwriterandom,"
              "updaterandom,"
              "randomwithverify,"
              "fill100k,"
              "crc32c,"
              "xxhash,"
              "compress,"
              "uncompress,"
              "acquireload,",

              "comma-separated list of operations to run in the specified order"
              "actual benchmarks:\n"
              "\tfillseq       -- write n values in sequential key"
              " order in async mode\n"
              "\tfillrandom    -- write n values in random key order in async"
              " mode\n"
              "\toverwrite     -- overwrite n values in random key order in"
              " async mode\n"
              "\tfillsync      -- write n/100 values in random key order in "
              "sync mode\n"
              "\tfill100k      -- write n/1000 100k values in random order in"
              " async mode\n"
              "\tdeleteseq     -- delete n keys in sequential order\n"
              "\tdeleterandom  -- delete n keys in random order\n"
              "\treadseq       -- read n times sequentially\n"
              "\treadtocache   -- 1 thread reading database sequentially\n"
              "\treadreverse   -- read n times in reverse order\n"
              "\treadrandom    -- read n times in random order\n"
              "\treadmissing   -- read n missing keys in random order\n"
              "\treadhot       -- read n times in random order from 1% section "
              "of db\n"
              "\treadwhilewriting      -- 1 writer, n threads doing random "
              "reads\n"
              "\treadrandomwriterandom -- n threads doing random-read, "
              "random-write\n"
              "\tprefixscanrandom      -- prefix scan n times in random order\n"
              "\tupdaterandom  -- n threads doing read-modify-write for random "
              "keys\n"
              "\tappendrandom  -- n threads doing read-modify-write with "
              "growing values\n"
              "\tmergerandom   -- same as updaterandom/appendrandom using merge"
              " operator. "
              "must be used with merge_operator\n"
              "\treadrandommergerandom -- perform n random read-or-merge "
              "operations. must be used with merge_operator\n"
              "\tnewiterator   -- repeated iterator creation\n"
              "\tseekrandom    -- n random seeks\n"
              "\tseekrandom    -- 1 writer, n threads doing random seeks\n"
              "\tcrc32c        -- repeated crc32c of 4k of data\n"
              "\txxhash        -- repeated xxhash of 4k of data\n"
              "\tacquireload   -- load n*1000 times\n"
              "meta operations:\n"
              "\tcompact     -- compact the entire db\n"
              "\tstats       -- print db stats\n"
              "\tlevelstats  -- print the number of files and bytes per level\n"
              "\tsstables    -- print sstable info\n"
              "\theapprofile -- dump a heap profile (if supported by this"
              " port)\n");

define_int64(num, 1000000, "number of key/values to place in database");

define_int64(numdistinct, 1000,
             "number of distinct keys to use. used in randomwithverify to "
             "read/write on fewer keys so that gets are more likely to find the"
             " key and puts are more likely to update the same key");

define_int64(merge_keys, -1,
             "number of distinct keys to use for mergerandom and "
             "readrandommergerandom. "
             "if negative, there will be flags_num keys.");
define_int32(num_column_families, 1, "number of column families to use.");

define_int64(reads, -1, "number of read operations to do.  "
             "if negative, do flags_num reads.");

define_int32(bloom_locality, 0, "control bloom filter probes locality");

define_int64(seed, 0, "seed base for random number generators. "
             "when 0 it is deterministic.");

define_int32(threads, 1, "number of concurrent threads to run.");

define_int32(duration, 0, "time in seconds for the random-ops tests to run."
             " when 0 then num & reads determine the test duration");

define_int32(value_size, 100, "size of each value");

define_bool(use_uint64_comparator, false, "use uint64 user comparator");

static bool validatekeysize(const char* flagname, int32_t value) {
  return true;
}

define_int32(key_size, 16, "size of each key");

define_int32(num_multi_db, 0,
             "number of dbs used in the benchmark. 0 means single db.");

define_double(compression_ratio, 0.5, "arrange to generate values that shrink"
              " to this fraction of their original size after compression");

define_bool(histogram, false, "print histogram of operation timings");

define_bool(enable_numa, false,
            "make operations aware of numa architecture and bind memory "
            "and cpus corresponding to nodes together. in numa, memory "
            "in same node as cpus are closer when compared to memory in "
            "other nodes. reads can be faster when the process is bound to "
            "cpu and memory of same node. use \"$numactl --hardware\" command "
            "to see numa memory architecture.");

define_int64(write_buffer_size, rocksdb::options().write_buffer_size,
             "number of bytes to buffer in memtable before compacting");

define_int32(max_write_buffer_number,
             rocksdb::options().max_write_buffer_number,
             "the number of in-memory memtables. each memtable is of size"
             "write_buffer_size.");

define_int32(min_write_buffer_number_to_merge,
             rocksdb::options().min_write_buffer_number_to_merge,
             "the minimum number of write buffers that will be merged together"
             "before writing to storage. this is cheap because it is an"
             "in-memory merge. if this feature is not enabled, then all these"
             "write buffers are flushed to l0 as separate files and this "
             "increases read amplification because a get request has to check"
             " in all of these files. also, an in-memory merge may result in"
             " writing less data to storage if there are duplicate records "
             " in each of these individual write buffers.");

define_int32(max_background_compactions,
             rocksdb::options().max_background_compactions,
             "the maximum number of concurrent background compactions"
             " that can occur in parallel.");

define_int32(max_background_flushes,
             rocksdb::options().max_background_flushes,
             "the maximum number of concurrent background flushes"
             " that can occur in parallel.");

static rocksdb::compactionstyle flags_compaction_style_e;
define_int32(compaction_style, (int32_t) rocksdb::options().compaction_style,
             "style of compaction: level-based vs universal");

define_int32(universal_size_ratio, 0,
             "percentage flexibility while comparing file size"
             " (for universal compaction only).");

define_int32(universal_min_merge_width, 0, "the minimum number of files in a"
             " single compaction run (for universal compaction only).");

define_int32(universal_max_merge_width, 0, "the max number of files to compact"
             " in universal style compaction");

define_int32(universal_max_size_amplification_percent, 0,
             "the max size amplification for universal style compaction");

define_int32(universal_compression_size_percent, -1,
             "the percentage of the database to compress for universal "
             "compaction. -1 means compress everything.");

define_int64(cache_size, -1, "number of bytes to use as a cache of uncompressed"
             "data. negative means use default settings.");

define_int32(block_size, rocksdb::blockbasedtableoptions().block_size,
             "number of bytes in a block.");

define_int32(block_restart_interval,
             rocksdb::blockbasedtableoptions().block_restart_interval,
             "number of keys between restart points "
             "for delta encoding of keys.");

define_int64(compressed_cache_size, -1,
             "number of bytes to use as a cache of compressed data.");

define_int32(open_files, rocksdb::options().max_open_files,
             "maximum number of files to keep open at the same time"
             " (use default if == 0)");

define_int32(bloom_bits, -1, "bloom filter bits per key. negative means"
             " use default settings.");
define_int32(memtable_bloom_bits, 0, "bloom filter bits per key for memtable. "
             "negative means no bloom filter.");

define_bool(use_existing_db, false, "if true, do not destroy the existing"
            " database.  if you set this flag and also specify a benchmark that"
            " wants a fresh database, that benchmark will fail.");

define_string(db, "", "use the db with the following name.");

static bool validatecachenumshardbits(const char* flagname, int32_t value) {
  if (value >= 20) {
    fprintf(stderr, "invalid value for --%s: %d, must be < 20\n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(cache_numshardbits, -1, "number of shards for the block cache"
             " is 2 ** cache_numshardbits. negative means use default settings."
             " this is applied only if flags_cache_size is non-negative.");

define_int32(cache_remove_scan_count_limit, 32, "");

define_bool(verify_checksum, false, "verify checksum for every block read"
            " from storage");

define_bool(statistics, false, "database statistics");
static class std::shared_ptr<rocksdb::statistics> dbstats;

define_int64(writes, -1, "number of write operations to do. if negative, do"
             " --num reads.");

define_int32(writes_per_second, 0, "per-thread rate limit on writes per second."
             " no limit when <= 0. only for the readwhilewriting test.");

define_bool(sync, false, "sync all writes to disk");

define_bool(disable_data_sync, false, "if true, do not wait until data is"
            " synced to disk.");

define_bool(use_fsync, false, "if true, issue fsync instead of fdatasync");

define_bool(disable_wal, false, "if true, do not write wal for write.");

define_string(wal_dir, "", "if not empty, use the given dir for wal");

define_int32(num_levels, 7, "the total number of levels");

define_int32(target_file_size_base, 2 * 1048576, "target file size at level-1");

define_int32(target_file_size_multiplier, 1,
             "a multiplier to compute target level-n file size (n >= 2)");

define_uint64(max_bytes_for_level_base,  10 * 1048576, "max bytes for level-1");

define_int32(max_bytes_for_level_multiplier, 10,
             "a multiplier to compute max bytes for level-n (n >= 2)");

static std::vector<int> flags_max_bytes_for_level_multiplier_additional_v;
define_string(max_bytes_for_level_multiplier_additional, "",
              "a vector that specifies additional fanout per level");

define_int32(level0_stop_writes_trigger, 12, "number of files in level-0"
             " that will trigger put stop.");

define_int32(level0_slowdown_writes_trigger, 8, "number of files in level-0"
             " that will slow down writes.");

define_int32(level0_file_num_compaction_trigger, 4, "number of files in level-0"
             " when compactions start");

static bool validateint32percent(const char* flagname, int32_t value) {
  if (value <= 0 || value>=100) {
    fprintf(stderr, "invalid value for --%s: %d, 0< pct <100 \n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(readwritepercent, 90, "ratio of reads to reads/writes (expressed"
             " as percentage) for the readrandomwriterandom workload. the "
             "default value 90 means 90% operations out of all reads and writes"
             " operations are reads. in other words, 9 gets for every 1 put.");

define_int32(mergereadpercent, 70, "ratio of merges to merges&reads (expressed"
             " as percentage) for the readrandommergerandom workload. the"
             " default value 70 means 70% out of all read and merge operations"
             " are merges. in other words, 7 merges for every 3 gets.");

define_int32(deletepercent, 2, "percentage of deletes out of reads/writes/"
             "deletes (used in randomwithverify only). randomwithverify "
             "calculates writepercent as (100 - flags_readwritepercent - "
             "deletepercent), so deletepercent must be smaller than (100 - "
             "flags_readwritepercent)");

define_uint64(delete_obsolete_files_period_micros, 0, "option to delete "
              "obsolete files periodically. 0 means that obsolete files are"
              " deleted after every compaction run.");

namespace {
enum rocksdb::compressiontype stringtocompressiontype(const char* ctype) {
  assert(ctype);

  if (!strcasecmp(ctype, "none"))
    return rocksdb::knocompression;
  else if (!strcasecmp(ctype, "snappy"))
    return rocksdb::ksnappycompression;
  else if (!strcasecmp(ctype, "zlib"))
    return rocksdb::kzlibcompression;
  else if (!strcasecmp(ctype, "bzip2"))
    return rocksdb::kbzip2compression;
  else if (!strcasecmp(ctype, "lz4"))
    return rocksdb::klz4compression;
  else if (!strcasecmp(ctype, "lz4hc"))
    return rocksdb::klz4hccompression;

  fprintf(stdout, "cannot parse compression type '%s'\n", ctype);
  return rocksdb::ksnappycompression; //default value
}
}  // namespace

define_string(compression_type, "snappy",
              "algorithm to use to compress the database");
static enum rocksdb::compressiontype flags_compression_type_e =
    rocksdb::ksnappycompression;

define_int32(compression_level, -1,
             "compression level. for zlib this should be -1 for the "
             "default level, or between 0 and 9.");

static bool validatecompressionlevel(const char* flagname, int32_t value) {
  if (value < -1 || value > 9) {
    fprintf(stderr, "invalid value for --%s: %d, must be between -1 and 9\n",
            flagname, value);
    return false;
  }
  return true;
}

static const bool flags_compression_level_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_compression_level, &validatecompressionlevel);

define_int32(min_level_to_compress, -1, "if non-negative, compression starts"
             " from this level. levels with number < min_level_to_compress are"
             " not compressed. otherwise, apply compression_type to "
             "all levels.");

static bool validatetablecachenumshardbits(const char* flagname,
                                           int32_t value) {
  if (0 >= value || value > 20) {
    fprintf(stderr, "invalid value for --%s: %d, must be  0 < val <= 20\n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(table_cache_numshardbits, 4, "");

define_string(hdfs, "", "name of hdfs environment");
// posix or hdfs environment
static rocksdb::env* flags_env = rocksdb::env::default();

define_int64(stats_interval, 0, "stats are reported every n operations when "
             "this is greater than zero. when 0 the interval grows over time.");

define_int32(stats_per_interval, 0, "reports additional stats per interval when"
             " this is greater than 0.");

define_int32(perf_level, 0, "level of perf collection");

static bool validateratelimit(const char* flagname, double value) {
  static constexpr double epsilon = 1e-10;
  if ( value < -epsilon ) {
    fprintf(stderr, "invalid value for --%s: %12.6f, must be >= 0.0\n",
            flagname, value);
    return false;
  }
  return true;
}
define_double(soft_rate_limit, 0.0, "");

define_double(hard_rate_limit, 0.0, "when not equal to 0 this make threads "
              "sleep at each stats reporting interval until the compaction"
              " score for all levels is less than or equal to this value.");

define_int32(rate_limit_delay_max_milliseconds, 1000,
             "when hard_rate_limit is set then this is the max time a put will"
             " be stalled.");

define_int32(max_grandparent_overlap_factor, 10, "control maximum bytes of "
             "overlaps in grandparent (i.e., level+2) before we stop building a"
             " single file in a level->level+1 compaction.");

define_bool(readonly, false, "run read only benchmarks.");

define_bool(disable_auto_compactions, false, "do not auto trigger compactions");

define_int32(source_compaction_factor, 1, "cap the size of data in level-k for"
             " a compaction run that compacts level-k with level-(k+1) (for"
             " k >= 1)");

define_uint64(wal_ttl_seconds, 0, "set the ttl for the wal files in seconds.");
define_uint64(wal_size_limit_mb, 0, "set the size limit for the wal files"
              " in mb.");

define_bool(bufferedio, rocksdb::envoptions().use_os_buffer,
            "allow buffered io using os buffers");

define_bool(mmap_read, rocksdb::envoptions().use_mmap_reads,
            "allow reads to occur via mmap-ing files");

define_bool(mmap_write, rocksdb::envoptions().use_mmap_writes,
            "allow writes to occur via mmap-ing files");

define_bool(advise_random_on_open, rocksdb::options().advise_random_on_open,
            "advise random access on table file open");

define_string(compaction_fadvice, "normal",
              "access pattern advice when a file is compacted");
static auto flags_compaction_fadvice_e =
  rocksdb::options().access_hint_on_compaction_start;

define_bool(use_tailing_iterator, false,
            "use tailing iterator to access a series of keys instead of get");
define_int64(iter_refresh_interval_us, -1,
             "how often to refresh iterators. disable refresh when -1");

define_bool(use_adaptive_mutex, rocksdb::options().use_adaptive_mutex,
            "use adaptive mutex");

define_uint64(bytes_per_sync,  rocksdb::options().bytes_per_sync,
              "allows os to incrementally sync files to disk while they are"
              " being written, in the background. issue one request for every"
              " bytes_per_sync written. 0 turns it off.");
define_bool(filter_deletes, false, " on true, deletes use bloom-filter and drop"
            " the delete if key not present");

define_int32(max_successive_merges, 0, "maximum number of successive merge"
             " operations on a key in the memtable");

static bool validateprefixsize(const char* flagname, int32_t value) {
  if (value < 0 || value>=2000000000) {
    fprintf(stderr, "invalid value for --%s: %d. 0<= prefixsize <=2000000000\n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(prefix_size, 0, "control the prefix size for hashskiplist and "
             "plain table");
define_int64(keys_per_prefix, 0, "control average number of keys generated "
             "per prefix, 0 means no special handling of the prefix, "
             "i.e. use the prefix comes with the generated random number.");
define_bool(enable_io_prio, false, "lower the background flush/compaction "
            "threads' io priority");

enum repfactory {
  kskiplist,
  kprefixhash,
  kvectorrep,
  khashlinkedlist,
  kcuckoo
};

namespace {
enum repfactory stringtorepfactory(const char* ctype) {
  assert(ctype);

  if (!strcasecmp(ctype, "skip_list"))
    return kskiplist;
  else if (!strcasecmp(ctype, "prefix_hash"))
    return kprefixhash;
  else if (!strcasecmp(ctype, "vector"))
    return kvectorrep;
  else if (!strcasecmp(ctype, "hash_linkedlist"))
    return khashlinkedlist;
  else if (!strcasecmp(ctype, "cuckoo"))
    return kcuckoo;

  fprintf(stdout, "cannot parse memreptable %s\n", ctype);
  return kskiplist;
}
}  // namespace

static enum repfactory flags_rep_factory;
define_string(memtablerep, "skip_list", "");
define_int64(hash_bucket_count, 1024 * 1024, "hash bucket count");
define_bool(use_plain_table, false, "if use plain table "
            "instead of block-based table format");
define_bool(use_cuckoo_table, false, "if use cuckoo table format");
define_double(cuckoo_hash_ratio, 0.9, "hash ratio for cuckoo sst table.");
define_bool(use_hash_search, false, "if use khashsearch "
            "instead of kbinarysearch. "
            "this is valid if only we use blocktable");

define_string(merge_operator, "", "the merge operator to use with the database."
              "if a new merge operator is specified, be sure to use fresh"
              " database the possible merge operators are defined in"
              " utilities/merge_operators.h");

static const bool flags_soft_rate_limit_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_soft_rate_limit, &validateratelimit);

static const bool flags_hard_rate_limit_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_hard_rate_limit, &validateratelimit);

static const bool flags_prefix_size_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_prefix_size, &validateprefixsize);

static const bool flags_key_size_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_key_size, &validatekeysize);

static const bool flags_cache_numshardbits_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_cache_numshardbits,
                          &validatecachenumshardbits);

static const bool flags_readwritepercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_readwritepercent, &validateint32percent);

define_int32(disable_seek_compaction, false,
             "not used, left here for backwards compatibility");

static const bool flags_deletepercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_deletepercent, &validateint32percent);
static const bool flags_table_cache_numshardbits_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_table_cache_numshardbits,
                          &validatetablecachenumshardbits);

namespace rocksdb {

// helper for quickly generating random data.
class randomgenerator {
 private:
  std::string data_;
  unsigned int pos_;

 public:
  randomgenerator() {
    // we use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32kb), and also
    // large enough to serve all typical value sizes we want to write.
    random rnd(301);
    std::string piece;
    while (data_.size() < (unsigned)std::max(1048576, flags_value_size)) {
      // add a short fragment that is as compressible as specified
      // by flags_compression_ratio.
      test::compressiblestring(&rnd, flags_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  slice generate(unsigned int len) {
    assert(len <= data_.size());
    if (pos_ + len > data_.size()) {
      pos_ = 0;
    }
    pos_ += len;
    return slice(data_.data() + pos_ - len, len);
  }
};

static void appendwithspace(std::string* str, slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class stats {
 private:
  int id_;
  double start_;
  double finish_;
  double seconds_;
  int64_t done_;
  int64_t last_report_done_;
  int64_t next_report_;
  int64_t bytes_;
  double last_op_finish_;
  double last_report_finish_;
  histogramimpl hist_;
  std::string message_;
  bool exclude_from_merge_;

 public:
  stats() { start(-1); }

  void start(int id) {
    id_ = id;
    next_report_ = flags_stats_interval ? flags_stats_interval : 100;
    last_op_finish_ = start_;
    hist_.clear();
    done_ = 0;
    last_report_done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = flags_env->nowmicros();
    finish_ = start_;
    last_report_finish_ = start_;
    message_.clear();
    // when set, stats from this thread won't be merged with others.
    exclude_from_merge_ = false;
  }

  void merge(const stats& other) {
    if (other.exclude_from_merge_)
      return;

    hist_.merge(other.hist_);
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void stop() {
    finish_ = flags_env->nowmicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void addmessage(slice msg) {
    appendwithspace(&message_, msg);
  }

  void setid(int id) { id_ = id; }
  void setexcludefrommerge() { exclude_from_merge_ = true; }

  void finishedops(db* db, int64_t num_ops) {
    if (flags_histogram) {
      double now = flags_env->nowmicros();
      double micros = now - last_op_finish_;
      hist_.add(micros);
      if (micros > 20000 && !flags_stats_interval) {
        fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_ += num_ops;
    if (done_ >= next_report_) {
      if (!flags_stats_interval) {
        if      (next_report_ < 1000)   next_report_ += 100;
        else if (next_report_ < 5000)   next_report_ += 500;
        else if (next_report_ < 10000)  next_report_ += 1000;
        else if (next_report_ < 50000)  next_report_ += 5000;
        else if (next_report_ < 100000) next_report_ += 10000;
        else if (next_report_ < 500000) next_report_ += 50000;
        else                            next_report_ += 100000;
        fprintf(stderr, "... finished %" priu64 " ops%30s\r", done_, "");
        fflush(stderr);
      } else {
        double now = flags_env->nowmicros();
        fprintf(stderr,
                "%s ... thread %d: (%" priu64 ",%" priu64 ") ops and "
                "(%.1f,%.1f) ops/second in (%.6f,%.6f) seconds\n",
                flags_env->timetostring((uint64_t) now/1000000).c_str(),
                id_,
                done_ - last_report_done_, done_,
                (done_ - last_report_done_) /
                ((now - last_report_finish_) / 1000000.0),
                done_ / ((now - start_) / 1000000.0),
                (now - last_report_finish_) / 1000000.0,
                (now - start_) / 1000000.0);

        if (flags_stats_per_interval) {
          std::string stats;
          if (db && db->getproperty("rocksdb.stats", &stats))
            fprintf(stderr, "%s\n", stats.c_str());
        }

        fflush(stderr);
        next_report_ += flags_stats_interval;
        last_report_finish_ = now;
        last_report_done_ = done_;
      }
    }
  }

  void addbytes(int64_t n) {
    bytes_ += n;
  }

  void report(const slice& name) {
    // pretend at least one op was done in case we are running a benchmark
    // that does not call finishedops().
    if (done_ < 1) done_ = 1;

    std::string extra;
    if (bytes_ > 0) {
      // rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      double elapsed = (finish_ - start_) * 1e-6;
      char rate[100];
      snprintf(rate, sizeof(rate), "%6.1f mb/s",
               (bytes_ / 1048576.0) / elapsed);
      extra = rate;
    }
    appendwithspace(&extra, message_);
    double elapsed = (finish_ - start_) * 1e-6;
    double throughput = (double)done_/elapsed;

    fprintf(stdout, "%-12s : %11.3f micros/op %ld ops/sec;%s%s\n",
            name.tostring().c_str(),
            elapsed * 1e6 / done_,
            (long)throughput,
            (extra.empty() ? "" : " "),
            extra.c_str());
    if (flags_histogram) {
      fprintf(stdout, "microseconds per op:\n%s\n", hist_.tostring().c_str());
    }
    fflush(stdout);
  }
};

// state shared by all concurrent executions of the same benchmark.
struct sharedstate {
  port::mutex mu;
  port::condvar cv;
  int total;
  int perf_level;

  // each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  long num_initialized;
  long num_done;
  bool start;

  sharedstate() : cv(&mu), perf_level(flags_perf_level) { }
};

// per-thread state for concurrent executions of the same benchmark.
struct threadstate {
  int tid;             // 0..n-1 when running in n threads
  random64 rand;         // has different seeds for different threads
  stats stats;
  sharedstate* shared;

  /* implicit */ threadstate(int index)
      : tid(index),
        rand((flags_seed ? flags_seed : 1000) + index) {
  }
};

class duration {
 public:
  duration(int max_seconds, int64_t max_ops) {
    max_seconds_ = max_seconds;
    max_ops_= max_ops;
    ops_ = 0;
    start_at_ = flags_env->nowmicros();
  }

  bool done(int64_t increment) {
    if (increment <= 0) increment = 1;    // avoid done(0) and infinite loops
    ops_ += increment;

    if (max_seconds_) {
      // recheck every appx 1000 ops (exact iff increment is factor of 1000)
      if ((ops_/1000) != ((ops_-increment)/1000)) {
        double now = flags_env->nowmicros();
        return ((now - start_at_) / 1000000.0) >= max_seconds_;
      } else {
        return false;
      }
    } else {
      return ops_ > max_ops_;
    }
  }

 private:
  int max_seconds_;
  int64_t max_ops_;
  int64_t ops_;
  double start_at_;
};

class benchmark {
 private:
  std::shared_ptr<cache> cache_;
  std::shared_ptr<cache> compressed_cache_;
  std::shared_ptr<const filterpolicy> filter_policy_;
  const slicetransform* prefix_extractor_;
  struct dbwithcolumnfamilies {
    std::vector<columnfamilyhandle*> cfh;
    db* db;
    dbwithcolumnfamilies() : db(nullptr) {
      cfh.clear();
    }
  };
  dbwithcolumnfamilies db_;
  std::vector<dbwithcolumnfamilies> multi_dbs_;
  int64_t num_;
  int value_size_;
  int key_size_;
  int prefix_size_;
  int64_t keys_per_prefix_;
  int64_t entries_per_batch_;
  writeoptions write_options_;
  int64_t reads_;
  int64_t writes_;
  int64_t readwrites_;
  int64_t merge_keys_;

  bool sanitycheck() {
    if (flags_compression_ratio > 1) {
      fprintf(stderr, "compression_ratio should be between 0 and 1\n");
      return false;
    }
    return true;
  }

  void printheader() {
    printenvironment();
    fprintf(stdout, "keys:       %d bytes each\n", flags_key_size);
    fprintf(stdout, "values:     %d bytes each (%d bytes after compression)\n",
            flags_value_size,
            static_cast<int>(flags_value_size * flags_compression_ratio + 0.5));
    fprintf(stdout, "entries:    %" priu64 "\n", num_);
    fprintf(stdout, "prefix:    %d bytes\n", flags_prefix_size);
    fprintf(stdout, "keys per prefix:    %" priu64 "\n", keys_per_prefix_);
    fprintf(stdout, "rawsize:    %.1f mb (estimated)\n",
            ((static_cast<int64_t>(flags_key_size + flags_value_size) * num_)
             / 1048576.0));
    fprintf(stdout, "filesize:   %.1f mb (estimated)\n",
            (((flags_key_size + flags_value_size * flags_compression_ratio)
              * num_)
             / 1048576.0));
    fprintf(stdout, "write rate limit: %d\n", flags_writes_per_second);
    if (flags_enable_numa) {
      fprintf(stderr, "running in numa enabled mode.\n");
#ifndef numa
      fprintf(stderr, "numa is not defined in the system.\n");
      exit(1);
#else
      if (numa_available() == -1) {
        fprintf(stderr, "numa is not supported by the system.\n");
        exit(1);
      }
#endif
    }
    switch (flags_compression_type_e) {
      case rocksdb::knocompression:
        fprintf(stdout, "compression: none\n");
        break;
      case rocksdb::ksnappycompression:
        fprintf(stdout, "compression: snappy\n");
        break;
      case rocksdb::kzlibcompression:
        fprintf(stdout, "compression: zlib\n");
        break;
      case rocksdb::kbzip2compression:
        fprintf(stdout, "compression: bzip2\n");
        break;
      case rocksdb::klz4compression:
        fprintf(stdout, "compression: lz4\n");
        break;
      case rocksdb::klz4hccompression:
        fprintf(stdout, "compression: lz4hc\n");
        break;
    }

    switch (flags_rep_factory) {
      case kprefixhash:
        fprintf(stdout, "memtablerep: prefix_hash\n");
        break;
      case kskiplist:
        fprintf(stdout, "memtablerep: skip_list\n");
        break;
      case kvectorrep:
        fprintf(stdout, "memtablerep: vector\n");
        break;
      case khashlinkedlist:
        fprintf(stdout, "memtablerep: hash_linkedlist\n");
        break;
      case kcuckoo:
        fprintf(stdout, "memtablerep: cuckoo\n");
        break;
    }
    fprintf(stdout, "perf level: %d\n", flags_perf_level);

    printwarnings();
    fprintf(stdout, "------------------------------------------------\n");
  }

  void printwarnings() {
#if defined(__gnuc__) && !defined(__optimize__)
    fprintf(stdout,
            "warning: optimization is disabled: benchmarks unnecessarily slow\n"
            );
#endif
#ifndef ndebug
    fprintf(stdout,
            "warning: assertions are enabled; benchmarks unnecessarily slow\n");
#endif
    if (flags_compression_type_e != rocksdb::knocompression) {
      // the test string should not be too small.
      const int len = flags_block_size;
      char* text = (char*) malloc(len+1);
      bool result = true;
      const char* name = nullptr;
      std::string compressed;

      memset(text, (int) 'y', len);
      text[len] = '\0';
      switch (flags_compression_type_e) {
        case ksnappycompression:
          result = port::snappy_compress(options().compression_opts, text,
                                         strlen(text), &compressed);
          name = "snappy";
          break;
        case kzlibcompression:
          result = port::zlib_compress(options().compression_opts, text,
                                       strlen(text), &compressed);
          name = "zlib";
          break;
        case kbzip2compression:
          result = port::bzip2_compress(options().compression_opts, text,
                                        strlen(text), &compressed);
          name = "bzip2";
          break;
        case klz4compression:
          result = port::lz4_compress(options().compression_opts, text,
                                      strlen(text), &compressed);
          name = "lz4";
          break;
        case klz4hccompression:
          result = port::lz4hc_compress(options().compression_opts, text,
                                        strlen(text), &compressed);
          name = "lz4hc";
          break;
        case knocompression:
          assert(false); // cannot happen
          break;
      }

      if (!result) {
        fprintf(stdout, "warning: %s compression is not enabled\n", name);
      } else if (name && compressed.size() >= strlen(text)) {
        fprintf(stdout, "warning: %s compression is not effective\n", name);
      }

      free(text);
    }
  }

// current the following isn't equivalent to os_linux.
#if defined(__linux)
  static slice trimspace(slice s) {
    unsigned int start = 0;
    while (start < s.size() && isspace(s[start])) {
      start++;
    }
    unsigned int limit = s.size();
    while (limit > start && isspace(s[limit-1])) {
      limit--;
    }
    return slice(s.data() + start, limit - start);
  }
#endif

  void printenvironment() {
    fprintf(stderr, "leveldb:    version %d.%d\n",
            kmajorversion, kminorversion);

#if defined(__linux)
    time_t now = time(nullptr);
    fprintf(stderr, "date:       %s", ctime(&now));  // ctime() adds newline

    file* cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo != nullptr) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
        const char* sep = strchr(line, ':');
        if (sep == nullptr) {
          continue;
        }
        slice key = trimspace(slice(line, sep - 1 - line));
        slice val = trimspace(slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.tostring();
        } else if (key == "cache size") {
          cache_size = val.tostring();
        }
      }
      fclose(cpuinfo);
      fprintf(stderr, "cpu:        %d * %s\n", num_cpus, cpu_type.c_str());
      fprintf(stderr, "cpucache:   %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  benchmark()
  : cache_(flags_cache_size >= 0 ?
           (flags_cache_numshardbits >= 1 ?
            newlrucache(flags_cache_size, flags_cache_numshardbits,
                        flags_cache_remove_scan_count_limit) :
            newlrucache(flags_cache_size)) : nullptr),
    compressed_cache_(flags_compressed_cache_size >= 0 ?
           (flags_cache_numshardbits >= 1 ?
            newlrucache(flags_compressed_cache_size, flags_cache_numshardbits) :
            newlrucache(flags_compressed_cache_size)) : nullptr),
    filter_policy_(flags_bloom_bits >= 0
                   ? newbloomfilterpolicy(flags_bloom_bits)
                   : nullptr),
    prefix_extractor_(newfixedprefixtransform(flags_prefix_size)),
    num_(flags_num),
    value_size_(flags_value_size),
    key_size_(flags_key_size),
    prefix_size_(flags_prefix_size),
    keys_per_prefix_(flags_keys_per_prefix),
    entries_per_batch_(1),
    reads_(flags_reads < 0 ? flags_num : flags_reads),
    writes_(flags_writes < 0 ? flags_num : flags_writes),
    readwrites_((flags_writes < 0  && flags_reads < 0)? flags_num :
                ((flags_writes > flags_reads) ? flags_writes : flags_reads)
               ),
    merge_keys_(flags_merge_keys < 0 ? flags_num : flags_merge_keys) {
    if (flags_prefix_size > flags_key_size) {
      fprintf(stderr, "prefix size is larger than key size");
      exit(1);
    }

    std::vector<std::string> files;
    flags_env->getchildren(flags_db, &files);
    for (unsigned int i = 0; i < files.size(); i++) {
      if (slice(files[i]).starts_with("heap-")) {
        flags_env->deletefile(flags_db + "/" + files[i]);
      }
    }
    if (!flags_use_existing_db) {
      destroydb(flags_db, options());
    }
  }

  ~benchmark() {
    delete db_.db;
    delete prefix_extractor_;
  }

  slice allocatekey() {
    return slice(new char[key_size_], key_size_);
  }

  // generate key according to the given specification and random number.
  // the resulting key will have the following format (if keys_per_prefix_
  // is positive), extra trailing bytes are either cut off or paddd with '0'.
  // the prefix value is derived from key value.
  //   ----------------------------
  //   | prefix 00000 | key 00000 |
  //   ----------------------------
  // if keys_per_prefix_ is 0, the key is simply a binary representation of
  // random number followed by trailing '0's
  //   ----------------------------
  //   |        key 00000         |
  //   ----------------------------
  void generatekeyfromint(uint64_t v, int64_t num_keys, slice* key) {
    char* start = const_cast<char*>(key->data());
    char* pos = start;
    if (keys_per_prefix_ > 0) {
      int64_t num_prefix = num_keys / keys_per_prefix_;
      int64_t prefix = v % num_prefix;
      int bytes_to_fill = std::min(prefix_size_, 8);
      if (port::klittleendian) {
        for (int i = 0; i < bytes_to_fill; ++i) {
          pos[i] = (prefix >> ((bytes_to_fill - i - 1) << 3)) & 0xff;
        }
      } else {
        memcpy(pos, static_cast<void*>(&prefix), bytes_to_fill);
      }
      if (prefix_size_ > 8) {
        // fill the rest with 0s
        memset(pos + 8, '0', prefix_size_ - 8);
      }
      pos += prefix_size_;
    }

    int bytes_to_fill = std::min(key_size_ - static_cast<int>(pos - start), 8);
    if (port::klittleendian) {
      for (int i = 0; i < bytes_to_fill; ++i) {
        pos[i] = (v >> ((bytes_to_fill - i - 1) << 3)) & 0xff;
      }
    } else {
      memcpy(pos, static_cast<void*>(&v), bytes_to_fill);
    }
    pos += bytes_to_fill;
    if (key_size_ > pos - start) {
      memset(pos, '0', key_size_ - (pos - start));
    }
  }

  std::string getdbnameformultiple(std::string base_name, size_t id) {
    return base_name + std::to_string(id);
  }

  std::string columnfamilyname(int i) {
    if (i == 0) {
      return kdefaultcolumnfamilyname;
    } else {
      char name[100];
      snprintf(name, sizeof(name), "column_family_name_%06d", i);
      return std::string(name);
    }
  }

  void run() {
    if (!sanitycheck()) {
      exit(1);
    }
    printheader();
    open();
    const char* benchmarks = flags_benchmarks.c_str();
    while (benchmarks != nullptr) {
      const char* sep = strchr(benchmarks, ',');
      slice name;
      if (sep == nullptr) {
        name = benchmarks;
        benchmarks = nullptr;
      } else {
        name = slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // sanitize parameters
      num_ = flags_num;
      reads_ = (flags_reads < 0 ? flags_num : flags_reads);
      writes_ = (flags_writes < 0 ? flags_num : flags_writes);
      value_size_ = flags_value_size;
      key_size_ = flags_key_size;
      entries_per_batch_ = 1;
      write_options_ = writeoptions();
      if (flags_sync) {
        write_options_.sync = true;
      }
      write_options_.disablewal = flags_disable_wal;

      void (benchmark::*method)(threadstate*) = nullptr;
      bool fresh_db = false;
      int num_threads = flags_threads;

      if (name == slice("fillseq")) {
        fresh_db = true;
        method = &benchmark::writeseq;
      } else if (name == slice("fillbatch")) {
        fresh_db = true;
        entries_per_batch_ = 1000;
        method = &benchmark::writeseq;
      } else if (name == slice("fillrandom")) {
        fresh_db = true;
        method = &benchmark::writerandom;
      } else if (name == slice("filluniquerandom")) {
        fresh_db = true;
        if (num_threads > 1) {
          fprintf(stderr, "filluniquerandom multithreaded not supported"
                           ", use 1 thread");
          num_threads = 1;
        }
        method = &benchmark::writeuniquerandom;
      } else if (name == slice("overwrite")) {
        fresh_db = false;
        method = &benchmark::writerandom;
      } else if (name == slice("fillsync")) {
        fresh_db = true;
        num_ /= 1000;
        write_options_.sync = true;
        method = &benchmark::writerandom;
      } else if (name == slice("fill100k")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        method = &benchmark::writerandom;
      } else if (name == slice("readseq")) {
        method = &benchmark::readsequential;
      } else if (name == slice("readtocache")) {
        method = &benchmark::readsequential;
        num_threads = 1;
        reads_ = num_;
      } else if (name == slice("readreverse")) {
        method = &benchmark::readreverse;
      } else if (name == slice("readrandom")) {
        method = &benchmark::readrandom;
      } else if (name == slice("multireadrandom")) {
        method = &benchmark::multireadrandom;
      } else if (name == slice("readmissing")) {
        ++key_size_;
        method = &benchmark::readrandom;
      } else if (name == slice("newiterator")) {
        method = &benchmark::iteratorcreation;
      } else if (name == slice("newiteratorwhilewriting")) {
        num_threads++;  // add extra thread for writing
        method = &benchmark::iteratorcreationwhilewriting;
      } else if (name == slice("seekrandom")) {
        method = &benchmark::seekrandom;
      } else if (name == slice("seekrandomwhilewriting")) {
        num_threads++;  // add extra thread for writing
        method = &benchmark::seekrandomwhilewriting;
      } else if (name == slice("readrandomsmall")) {
        reads_ /= 1000;
        method = &benchmark::readrandom;
      } else if (name == slice("deleteseq")) {
        method = &benchmark::deleteseq;
      } else if (name == slice("deleterandom")) {
        method = &benchmark::deleterandom;
      } else if (name == slice("readwhilewriting")) {
        num_threads++;  // add extra thread for writing
        method = &benchmark::readwhilewriting;
      } else if (name == slice("readrandomwriterandom")) {
        method = &benchmark::readrandomwriterandom;
      } else if (name == slice("readrandommergerandom")) {
        if (flags_merge_operator.empty()) {
          fprintf(stdout, "%-12s : skipped (--merge_operator is unknown)\n",
                  name.tostring().c_str());
          exit(1);
        }
        method = &benchmark::readrandommergerandom;
      } else if (name == slice("updaterandom")) {
        method = &benchmark::updaterandom;
      } else if (name == slice("appendrandom")) {
        method = &benchmark::appendrandom;
      } else if (name == slice("mergerandom")) {
        if (flags_merge_operator.empty()) {
          fprintf(stdout, "%-12s : skipped (--merge_operator is unknown)\n",
                  name.tostring().c_str());
          exit(1);
        }
        method = &benchmark::mergerandom;
      } else if (name == slice("randomwithverify")) {
        method = &benchmark::randomwithverify;
      } else if (name == slice("compact")) {
        method = &benchmark::compact;
      } else if (name == slice("crc32c")) {
        method = &benchmark::crc32c;
      } else if (name == slice("xxhash")) {
        method = &benchmark::xxhash;
      } else if (name == slice("acquireload")) {
        method = &benchmark::acquireload;
      } else if (name == slice("compress")) {
        method = &benchmark::compress;
      } else if (name == slice("uncompress")) {
        method = &benchmark::uncompress;
      } else if (name == slice("stats")) {
        printstats("rocksdb.stats");
      } else if (name == slice("levelstats")) {
        printstats("rocksdb.levelstats");
      } else if (name == slice("sstables")) {
        printstats("rocksdb.sstables");
      } else {
        if (name != slice()) {  // no error message for empty name
          fprintf(stderr, "unknown benchmark '%s'\n", name.tostring().c_str());
          exit(1);
        }
      }

      if (fresh_db) {
        if (flags_use_existing_db) {
          fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                  name.tostring().c_str());
          method = nullptr;
        } else {
          if (db_.db != nullptr) {
            delete db_.db;
            db_.db = nullptr;
            db_.cfh.clear();
            destroydb(flags_db, options());
          }
          for (size_t i = 0; i < multi_dbs_.size(); i++) {
            delete multi_dbs_[i].db;
            destroydb(getdbnameformultiple(flags_db, i), options());
          }
          multi_dbs_.clear();
        }
        open();
      }

      if (method != nullptr) {
        fprintf(stdout, "db path: [%s]\n", flags_db.c_str());
        runbenchmark(num_threads, name, method);
      }
    }
    if (flags_statistics) {
     fprintf(stdout, "statistics:\n%s\n", dbstats->tostring().c_str());
    }
  }

 private:
  struct threadarg {
    benchmark* bm;
    sharedstate* shared;
    threadstate* thread;
    void (benchmark::*method)(threadstate*);
  };

  static void threadbody(void* v) {
    threadarg* arg = reinterpret_cast<threadarg*>(v);
    sharedstate* shared = arg->shared;
    threadstate* thread = arg->thread;
    {
      mutexlock l(&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.signalall();
      }
      while (!shared->start) {
        shared->cv.wait();
      }
    }

    setperflevel(static_cast<perflevel> (shared->perf_level));
    thread->stats.start(thread->tid);
    (arg->bm->*(arg->method))(thread);
    thread->stats.stop();

    {
      mutexlock l(&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.signalall();
      }
    }
  }

  void runbenchmark(int n, slice name,
                    void (benchmark::*method)(threadstate*)) {
    sharedstate shared;
    shared.total = n;
    shared.num_initialized = 0;
    shared.num_done = 0;
    shared.start = false;

    threadarg* arg = new threadarg[n];

    for (int i = 0; i < n; i++) {
#ifdef numa
      if (flags_enable_numa) {
        // performs a local allocation of memory to threads in numa node.
        int n_nodes = numa_num_task_nodes();  // number of nodes in numa.
        numa_exit_on_error = 1;
        int numa_node = i % n_nodes;
        bitmask* nodes = numa_allocate_nodemask();
        numa_bitmask_clearall(nodes);
        numa_bitmask_setbit(nodes, numa_node);
        // numa_bind() call binds the process to the node and these
        // properties are passed on to the thread that is created in
        // startthread method called later in the loop.
        numa_bind(nodes);
        numa_set_strict(1);
        numa_free_nodemask(nodes);
      }
#endif
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      arg[i].thread = new threadstate(i);
      arg[i].thread->shared = &shared;
      flags_env->startthread(threadbody, &arg[i]);
    }

    shared.mu.lock();
    while (shared.num_initialized < n) {
      shared.cv.wait();
    }

    shared.start = true;
    shared.cv.signalall();
    while (shared.num_done < n) {
      shared.cv.wait();
    }
    shared.mu.unlock();

    // stats for some threads can be excluded.
    stats merge_stats;
    for (int i = 0; i < n; i++) {
      merge_stats.merge(arg[i].thread->stats);
    }
    merge_stats.report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
  }

  void crc32c(threadstate* thread) {
    // checksum about 500mb of data total
    const int size = 4096;
    const char* label = "(4k per op)";
    std::string data(size, 'x');
    int64_t bytes = 0;
    uint32_t crc = 0;
    while (bytes < 500 * 1048576) {
      crc = crc32c::value(data.data(), size);
      thread->stats.finishedops(nullptr, 1);
      bytes += size;
    }
    // print so result is not dead
    fprintf(stderr, "... crc=0x%x\r", static_cast<unsigned int>(crc));

    thread->stats.addbytes(bytes);
    thread->stats.addmessage(label);
  }

  void xxhash(threadstate* thread) {
    // checksum about 500mb of data total
    const int size = 4096;
    const char* label = "(4k per op)";
    std::string data(size, 'x');
    int64_t bytes = 0;
    unsigned int xxh32 = 0;
    while (bytes < 500 * 1048576) {
      xxh32 = xxh32(data.data(), size, 0);
      thread->stats.finishedops(nullptr, 1);
      bytes += size;
    }
    // print so result is not dead
    fprintf(stderr, "... xxh32=0x%x\r", static_cast<unsigned int>(xxh32));

    thread->stats.addbytes(bytes);
    thread->stats.addmessage(label);
  }

  void acquireload(threadstate* thread) {
    int dummy;
    port::atomicpointer ap(&dummy);
    int count = 0;
    void *ptr = nullptr;
    thread->stats.addmessage("(each op is 1000 loads)");
    while (count < 100000) {
      for (int i = 0; i < 1000; i++) {
        ptr = ap.acquire_load();
      }
      count++;
      thread->stats.finishedops(nullptr, 1);
    }
    if (ptr == nullptr) exit(1); // disable unused variable warning.
  }

  void compress(threadstate *thread) {
    randomgenerator gen;
    slice input = gen.generate(flags_block_size);
    int64_t bytes = 0;
    int64_t produced = 0;
    bool ok = true;
    std::string compressed;

    // compress 1g
    while (ok && bytes < int64_t(1) << 30) {
      switch (flags_compression_type_e) {
      case rocksdb::ksnappycompression:
        ok = port::snappy_compress(options().compression_opts, input.data(),
                                   input.size(), &compressed);
        break;
      case rocksdb::kzlibcompression:
        ok = port::zlib_compress(options().compression_opts, input.data(),
                                 input.size(), &compressed);
        break;
      case rocksdb::kbzip2compression:
        ok = port::bzip2_compress(options().compression_opts, input.data(),
                                  input.size(), &compressed);
        break;
      case rocksdb::klz4compression:
        ok = port::lz4_compress(options().compression_opts, input.data(),
                                input.size(), &compressed);
        break;
      case rocksdb::klz4hccompression:
        ok = port::lz4hc_compress(options().compression_opts, input.data(),
                                  input.size(), &compressed);
        break;
      default:
        ok = false;
      }
      produced += compressed.size();
      bytes += input.size();
      thread->stats.finishedops(nullptr, 1);
    }

    if (!ok) {
      thread->stats.addmessage("(compression failure)");
    } else {
      char buf[100];
      snprintf(buf, sizeof(buf), "(output: %.1f%%)",
               (produced * 100.0) / bytes);
      thread->stats.addmessage(buf);
      thread->stats.addbytes(bytes);
    }
  }

  void uncompress(threadstate *thread) {
    randomgenerator gen;
    slice input = gen.generate(flags_block_size);
    std::string compressed;

    bool ok;
    switch (flags_compression_type_e) {
    case rocksdb::ksnappycompression:
      ok = port::snappy_compress(options().compression_opts, input.data(),
                                 input.size(), &compressed);
      break;
    case rocksdb::kzlibcompression:
      ok = port::zlib_compress(options().compression_opts, input.data(),
                               input.size(), &compressed);
      break;
    case rocksdb::kbzip2compression:
      ok = port::bzip2_compress(options().compression_opts, input.data(),
                                input.size(), &compressed);
      break;
    case rocksdb::klz4compression:
      ok = port::lz4_compress(options().compression_opts, input.data(),
                              input.size(), &compressed);
      break;
    case rocksdb::klz4hccompression:
      ok = port::lz4hc_compress(options().compression_opts, input.data(),
                                input.size(), &compressed);
      break;
    default:
      ok = false;
    }

    int64_t bytes = 0;
    int decompress_size;
    while (ok && bytes < 1024 * 1048576) {
      char *uncompressed = nullptr;
      switch (flags_compression_type_e) {
      case rocksdb::ksnappycompression:
        // allocate here to make comparison fair
        uncompressed = new char[input.size()];
        ok = port::snappy_uncompress(compressed.data(), compressed.size(),
                                     uncompressed);
        break;
      case rocksdb::kzlibcompression:
        uncompressed = port::zlib_uncompress(
            compressed.data(), compressed.size(), &decompress_size);
        ok = uncompressed != nullptr;
        break;
      case rocksdb::kbzip2compression:
        uncompressed = port::bzip2_uncompress(
            compressed.data(), compressed.size(), &decompress_size);
        ok = uncompressed != nullptr;
        break;
      case rocksdb::klz4compression:
        uncompressed = port::lz4_uncompress(
            compressed.data(), compressed.size(), &decompress_size);
        ok = uncompressed != nullptr;
        break;
      case rocksdb::klz4hccompression:
        uncompressed = port::lz4_uncompress(
            compressed.data(), compressed.size(), &decompress_size);
        ok = uncompressed != nullptr;
        break;
      default:
        ok = false;
      }
      delete[] uncompressed;
      bytes += input.size();
      thread->stats.finishedops(nullptr, 1);
    }

    if (!ok) {
      thread->stats.addmessage("(compression failure)");
    } else {
      thread->stats.addbytes(bytes);
    }
  }

  void open() {
    assert(db_.db == nullptr);
    options options;
    options.create_if_missing = !flags_use_existing_db;
    options.create_missing_column_families = flags_num_column_families > 1;
    options.write_buffer_size = flags_write_buffer_size;
    options.max_write_buffer_number = flags_max_write_buffer_number;
    options.min_write_buffer_number_to_merge =
      flags_min_write_buffer_number_to_merge;
    options.max_background_compactions = flags_max_background_compactions;
    options.max_background_flushes = flags_max_background_flushes;
    options.compaction_style = flags_compaction_style_e;
    if (flags_prefix_size != 0) {
      options.prefix_extractor.reset(
          newfixedprefixtransform(flags_prefix_size));
    }
    if (flags_use_uint64_comparator) {
      options.comparator = test::uint64comparator();
      if (flags_key_size != 8) {
        fprintf(stderr, "using uint64 comparator but key size is not 8.\n");
        exit(1);
      }
    }
    options.memtable_prefix_bloom_bits = flags_memtable_bloom_bits;
    options.bloom_locality = flags_bloom_locality;
    options.max_open_files = flags_open_files;
    options.statistics = dbstats;
    if (flags_enable_io_prio) {
      flags_env->lowerthreadpooliopriority(env::low);
      flags_env->lowerthreadpooliopriority(env::high);
    }
    options.env = flags_env;
    options.disabledatasync = flags_disable_data_sync;
    options.use_fsync = flags_use_fsync;
    options.wal_dir = flags_wal_dir;
    options.num_levels = flags_num_levels;
    options.target_file_size_base = flags_target_file_size_base;
    options.target_file_size_multiplier = flags_target_file_size_multiplier;
    options.max_bytes_for_level_base = flags_max_bytes_for_level_base;
    options.max_bytes_for_level_multiplier =
        flags_max_bytes_for_level_multiplier;
    options.filter_deletes = flags_filter_deletes;
    if ((flags_prefix_size == 0) && (flags_rep_factory == kprefixhash ||
                                     flags_rep_factory == khashlinkedlist)) {
      fprintf(stderr, "prefix_size should be non-zero if prefixhash or "
                      "hashlinkedlist memtablerep is used\n");
      exit(1);
    }
    switch (flags_rep_factory) {
      case kprefixhash:
        options.memtable_factory.reset(newhashskiplistrepfactory(
            flags_hash_bucket_count));
        break;
      case kskiplist:
        // no need to do anything
        break;
      case khashlinkedlist:
        options.memtable_factory.reset(newhashlinklistrepfactory(
            flags_hash_bucket_count));
        break;
      case kvectorrep:
        options.memtable_factory.reset(
          new vectorrepfactory
        );
        break;
      case kcuckoo:
        options.memtable_factory.reset(newhashcuckoorepfactory(
            options.write_buffer_size, flags_key_size + flags_value_size));
        break;
    }
    if (flags_use_plain_table) {
      if (flags_rep_factory != kprefixhash &&
          flags_rep_factory != khashlinkedlist) {
        fprintf(stderr, "waring: plain table is used with skiplist\n");
      }
      if (!flags_mmap_read && !flags_mmap_write) {
        fprintf(stderr, "plain table format requires mmap to operate\n");
        exit(1);
      }

      int bloom_bits_per_key = flags_bloom_bits;
      if (bloom_bits_per_key < 0) {
        bloom_bits_per_key = 0;
      }

      plaintableoptions plain_table_options;
      plain_table_options.user_key_len = flags_key_size;
      plain_table_options.bloom_bits_per_key = bloom_bits_per_key;
      plain_table_options.hash_table_ratio = 0.75;
      options.table_factory = std::shared_ptr<tablefactory>(
          newplaintablefactory(plain_table_options));
    } else if (flags_use_cuckoo_table) {
      if (flags_cuckoo_hash_ratio > 1 || flags_cuckoo_hash_ratio < 0) {
        fprintf(stderr, "invalid cuckoo_hash_ratio\n");
        exit(1);
      }
      options.table_factory = std::shared_ptr<tablefactory>(
          newcuckootablefactory(flags_cuckoo_hash_ratio));
    } else {
      blockbasedtableoptions block_based_options;
      if (flags_use_hash_search) {
        if (flags_prefix_size == 0) {
          fprintf(stderr,
              "prefix_size not assigned when enable use_hash_search \n");
          exit(1);
        }
        block_based_options.index_type = blockbasedtableoptions::khashsearch;
      } else {
        block_based_options.index_type = blockbasedtableoptions::kbinarysearch;
      }
      if (cache_ == nullptr) {
        block_based_options.no_block_cache = true;
      }
      block_based_options.block_cache = cache_;
      block_based_options.block_cache_compressed = compressed_cache_;
      block_based_options.block_size = flags_block_size;
      block_based_options.block_restart_interval = flags_block_restart_interval;
      block_based_options.filter_policy = filter_policy_;
      options.table_factory.reset(
          newblockbasedtablefactory(block_based_options));
    }
    if (flags_max_bytes_for_level_multiplier_additional_v.size() > 0) {
      if (flags_max_bytes_for_level_multiplier_additional_v.size() !=
          (unsigned int)flags_num_levels) {
        fprintf(stderr, "insufficient number of fanouts specified %d\n",
                (int)flags_max_bytes_for_level_multiplier_additional_v.size());
        exit(1);
      }
      options.max_bytes_for_level_multiplier_additional =
        flags_max_bytes_for_level_multiplier_additional_v;
    }
    options.level0_stop_writes_trigger = flags_level0_stop_writes_trigger;
    options.level0_file_num_compaction_trigger =
        flags_level0_file_num_compaction_trigger;
    options.level0_slowdown_writes_trigger =
      flags_level0_slowdown_writes_trigger;
    options.compression = flags_compression_type_e;
    options.compression_opts.level = flags_compression_level;
    options.wal_ttl_seconds = flags_wal_ttl_seconds;
    options.wal_size_limit_mb = flags_wal_size_limit_mb;
    if (flags_min_level_to_compress >= 0) {
      assert(flags_min_level_to_compress <= flags_num_levels);
      options.compression_per_level.resize(flags_num_levels);
      for (int i = 0; i < flags_min_level_to_compress; i++) {
        options.compression_per_level[i] = knocompression;
      }
      for (int i = flags_min_level_to_compress;
           i < flags_num_levels; i++) {
        options.compression_per_level[i] = flags_compression_type_e;
      }
    }
    options.delete_obsolete_files_period_micros =
      flags_delete_obsolete_files_period_micros;
    options.soft_rate_limit = flags_soft_rate_limit;
    options.hard_rate_limit = flags_hard_rate_limit;
    options.rate_limit_delay_max_milliseconds =
      flags_rate_limit_delay_max_milliseconds;
    options.table_cache_numshardbits = flags_table_cache_numshardbits;
    options.max_grandparent_overlap_factor =
      flags_max_grandparent_overlap_factor;
    options.disable_auto_compactions = flags_disable_auto_compactions;
    options.source_compaction_factor = flags_source_compaction_factor;

    // fill storage options
    options.allow_os_buffer = flags_bufferedio;
    options.allow_mmap_reads = flags_mmap_read;
    options.allow_mmap_writes = flags_mmap_write;
    options.advise_random_on_open = flags_advise_random_on_open;
    options.access_hint_on_compaction_start = flags_compaction_fadvice_e;
    options.use_adaptive_mutex = flags_use_adaptive_mutex;
    options.bytes_per_sync = flags_bytes_per_sync;

    // merge operator options
    options.merge_operator = mergeoperators::createfromstringid(
        flags_merge_operator);
    if (options.merge_operator == nullptr && !flags_merge_operator.empty()) {
      fprintf(stderr, "invalid merge operator: %s\n",
              flags_merge_operator.c_str());
      exit(1);
    }
    options.max_successive_merges = flags_max_successive_merges;

    // set universal style compaction configurations, if applicable
    if (flags_universal_size_ratio != 0) {
      options.compaction_options_universal.size_ratio =
        flags_universal_size_ratio;
    }
    if (flags_universal_min_merge_width != 0) {
      options.compaction_options_universal.min_merge_width =
        flags_universal_min_merge_width;
    }
    if (flags_universal_max_merge_width != 0) {
      options.compaction_options_universal.max_merge_width =
        flags_universal_max_merge_width;
    }
    if (flags_universal_max_size_amplification_percent != 0) {
      options.compaction_options_universal.max_size_amplification_percent =
        flags_universal_max_size_amplification_percent;
    }
    if (flags_universal_compression_size_percent != -1) {
      options.compaction_options_universal.compression_size_percent =
        flags_universal_compression_size_percent;
    }

    if (flags_num_multi_db <= 1) {
      opendb(options, flags_db, &db_);
    } else {
      multi_dbs_.clear();
      multi_dbs_.resize(flags_num_multi_db);
      for (int i = 0; i < flags_num_multi_db; i++) {
        opendb(options, getdbnameformultiple(flags_db, i), &multi_dbs_[i]);
      }
    }
    if (flags_min_level_to_compress >= 0) {
      options.compression_per_level.clear();
    }
  }

  void opendb(const options& options, const std::string& db_name,
      dbwithcolumnfamilies* db) {
    status s;
    // open with column families if necessary.
    if (flags_num_column_families > 1) {
      db->cfh.resize(flags_num_column_families);
      std::vector<columnfamilydescriptor> column_families;
      for (int i = 0; i < flags_num_column_families; i++) {
        column_families.push_back(columnfamilydescriptor(
              columnfamilyname(i), columnfamilyoptions(options)));
      }
      if (flags_readonly) {
        s = db::openforreadonly(options, db_name, column_families,
            &db->cfh, &db->db);
      } else {
        s = db::open(options, db_name, column_families, &db->cfh, &db->db);
      }
    } else if (flags_readonly) {
      s = db::openforreadonly(options, db_name, &db->db);
    } else {
      s = db::open(options, db_name, &db->db);
    }
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.tostring().c_str());
      exit(1);
    }
  }

  enum writemode {
    random, sequential, unique_random
  };

  void writeseq(threadstate* thread) {
    dowrite(thread, sequential);
  }

  void writerandom(threadstate* thread) {
    dowrite(thread, random);
  }

  void writeuniquerandom(threadstate* thread) {
    dowrite(thread, unique_random);
  }

  class keygenerator {
   public:
    keygenerator(random64* rand, writemode mode,
        uint64_t num, uint64_t num_per_set = 64 * 1024)
      : rand_(rand),
        mode_(mode),
        num_(num),
        next_(0) {
      if (mode_ == unique_random) {
        // note: if memory consumption of this approach becomes a concern,
        // we can either break it into pieces and only random shuffle a section
        // each time. alternatively, use a bit map implementation
        // (https://reviews.facebook.net/differential/diff/54627/)
        values_.resize(num_);
        for (uint64_t i = 0; i < num_; ++i) {
          values_[i] = i;
        }
        std::shuffle(values_.begin(), values_.end(),
            std::default_random_engine(flags_seed));
      }
    }

    uint64_t next() {
      switch (mode_) {
        case sequential:
          return next_++;
        case random:
          return rand_->next() % num_;
        case unique_random:
          return values_[next_++];
      }
      assert(false);
      return std::numeric_limits<uint64_t>::max();
    }

   private:
    random64* rand_;
    writemode mode_;
    const uint64_t num_;
    uint64_t next_;
    std::vector<uint64_t> values_;
  };

  db* selectdb(threadstate* thread) {
    return selectdbwithcfh(thread)->db;
  }

  dbwithcolumnfamilies* selectdbwithcfh(threadstate* thread) {
    return selectdbwithcfh(thread->rand.next());
  }

  dbwithcolumnfamilies* selectdbwithcfh(uint64_t rand_int) {
    if (db_.db != nullptr) {
      return &db_;
    } else  {
      return &multi_dbs_[rand_int % multi_dbs_.size()];
    }
  }

  void dowrite(threadstate* thread, writemode write_mode) {
    const int test_duration = write_mode == random ? flags_duration : 0;
    const int64_t num_ops = writes_ == 0 ? num_ : writes_;

    size_t num_key_gens = 1;
    if (db_.db == nullptr) {
      num_key_gens = multi_dbs_.size();
    }
    std::vector<std::unique_ptr<keygenerator>> key_gens(num_key_gens);
    duration duration(test_duration, num_ops * num_key_gens);
    for (size_t i = 0; i < num_key_gens; i++) {
      key_gens[i].reset(new keygenerator(&(thread->rand), write_mode, num_ops));
    }

    if (num_ != flags_num) {
      char msg[100];
      snprintf(msg, sizeof(msg), "(%" priu64 " ops)", num_);
      thread->stats.addmessage(msg);
    }

    randomgenerator gen;
    writebatch batch;
    status s;
    int64_t bytes = 0;

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    while (!duration.done(entries_per_batch_)) {
      size_t id = thread->rand.next() % num_key_gens;
      dbwithcolumnfamilies* db_with_cfh = selectdbwithcfh(id);
      batch.clear();
      for (int64_t j = 0; j < entries_per_batch_; j++) {
        int64_t rand_num = key_gens[id]->next();
        generatekeyfromint(rand_num, flags_num, &key);
        if (flags_num_column_families <= 1) {
          batch.put(key, gen.generate(value_size_));
        } else {
          // we use same rand_num as seed for key and column family so that we
          // can deterministically find the cfh corresponding to a particular
          // key while reading the key.
          batch.put(db_with_cfh->cfh[rand_num % db_with_cfh->cfh.size()],
              key, gen.generate(value_size_));
        }
        bytes += value_size_ + key_size_;
      }
      s = db_with_cfh->db->write(write_options_, &batch);
      thread->stats.finishedops(db_with_cfh->db, entries_per_batch_);
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.tostring().c_str());
        exit(1);
      }
    }
    thread->stats.addbytes(bytes);
  }

  void readsequential(threadstate* thread) {
    if (db_.db != nullptr) {
      readsequential(thread, db_.db);
    } else {
      for (const auto& db_with_cfh : multi_dbs_) {
        readsequential(thread, db_with_cfh.db);
      }
    }
  }

  void readsequential(threadstate* thread, db* db) {
    iterator* iter = db->newiterator(readoptions(flags_verify_checksum, true));
    int64_t i = 0;
    int64_t bytes = 0;
    for (iter->seektofirst(); i < reads_ && iter->valid(); iter->next()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.finishedops(db, 1);
      ++i;
    }
    delete iter;
    thread->stats.addbytes(bytes);
  }

  void readreverse(threadstate* thread) {
    if (db_.db != nullptr) {
      readreverse(thread, db_.db);
    } else {
      for (const auto& db_with_cfh : multi_dbs_) {
        readreverse(thread, db_with_cfh.db);
      }
    }
  }

  void readreverse(threadstate* thread, db* db) {
    iterator* iter = db->newiterator(readoptions(flags_verify_checksum, true));
    int64_t i = 0;
    int64_t bytes = 0;
    for (iter->seektolast(); i < reads_ && iter->valid(); iter->prev()) {
      bytes += iter->key().size() + iter->value().size();
      thread->stats.finishedops(db, 1);
      ++i;
    }
    delete iter;
    thread->stats.addbytes(bytes);
  }

  void readrandom(threadstate* thread) {
    int64_t read = 0;
    int64_t found = 0;
    readoptions options(flags_verify_checksum, true);
    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    std::string value;

    duration duration(flags_duration, reads_);
    while (!duration.done(1)) {
      dbwithcolumnfamilies* db_with_cfh = selectdbwithcfh(thread);
      // we use same key_rand as seed for key and column family so that we can
      // deterministically find the cfh corresponding to a particular key, as it
      // is done in dowrite method.
      int64_t key_rand = thread->rand.next() % flags_num;
      generatekeyfromint(key_rand, flags_num, &key);
      read++;
      status s;
      if (flags_num_column_families > 1) {
        s = db_with_cfh->db->get(options,
            db_with_cfh->cfh[key_rand % db_with_cfh->cfh.size()], key, &value);
      } else {
        s = db_with_cfh->db->get(options, key, &value);
      }
      if (s.ok()) {
        found++;
      }
      thread->stats.finishedops(db_with_cfh->db, 1);
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "(%" priu64 " of %" priu64 " found)\n",
             found, read);

    thread->stats.addmessage(msg);

    if (flags_perf_level > 0) {
      thread->stats.addmessage(perf_context.tostring());
    }
  }

  // calls multiget over a list of keys from a random distribution.
  // returns the total number of keys found.
  void multireadrandom(threadstate* thread) {
    int64_t read = 0;
    int64_t found = 0;
    readoptions options(flags_verify_checksum, true);
    std::vector<slice> keys;
    std::vector<std::string> values(entries_per_batch_);
    while (static_cast<int64_t>(keys.size()) < entries_per_batch_) {
      keys.push_back(allocatekey());
    }

    duration duration(flags_duration, reads_);
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      for (int64_t i = 0; i < entries_per_batch_; ++i) {
        generatekeyfromint(thread->rand.next() % flags_num,
            flags_num, &keys[i]);
      }
      std::vector<status> statuses = db->multiget(options, keys, &values);
      assert(static_cast<int64_t>(statuses.size()) == entries_per_batch_);

      read += entries_per_batch_;
      for (int64_t i = 0; i < entries_per_batch_; ++i) {
        if (statuses[i].ok()) {
          ++found;
        }
      }
      thread->stats.finishedops(db, entries_per_batch_);
    }
    for (auto& k : keys) {
      delete k.data();
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "(%" priu64 " of %" priu64 " found)",
             found, read);
    thread->stats.addmessage(msg);
  }

  void iteratorcreation(threadstate* thread) {
    duration duration(flags_duration, reads_);
    readoptions options(flags_verify_checksum, true);
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      iterator* iter = db->newiterator(options);
      delete iter;
      thread->stats.finishedops(db, 1);
    }
  }

  void iteratorcreationwhilewriting(threadstate* thread) {
    if (thread->tid > 0) {
      iteratorcreation(thread);
    } else {
      bgwriter(thread);
    }
  }

  void seekrandom(threadstate* thread) {
    int64_t read = 0;
    int64_t found = 0;
    readoptions options(flags_verify_checksum, true);
    options.tailing = flags_use_tailing_iterator;

    iterator* single_iter = nullptr;
    std::vector<iterator*> multi_iters;
    if (db_.db != nullptr) {
      single_iter = db_.db->newiterator(options);
    } else {
      for (const auto& db_with_cfh : multi_dbs_) {
        multi_iters.push_back(db_with_cfh.db->newiterator(options));
      }
    }
    uint64_t last_refresh = flags_env->nowmicros();

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());

    duration duration(flags_duration, reads_);
    while (!duration.done(1)) {
      if (!flags_use_tailing_iterator && flags_iter_refresh_interval_us >= 0) {
        uint64_t now = flags_env->nowmicros();
        if (now - last_refresh > (uint64_t)flags_iter_refresh_interval_us) {
          if (db_.db != nullptr) {
            delete single_iter;
            single_iter = db_.db->newiterator(options);
          } else {
            for (auto iter : multi_iters) {
              delete iter;
            }
            multi_iters.clear();
            for (const auto& db_with_cfh : multi_dbs_) {
              multi_iters.push_back(db_with_cfh.db->newiterator(options));
            }
          }
        }
        last_refresh = now;
      }
      // pick a iterator to use
      iterator* iter_to_use = single_iter;
      if (single_iter == nullptr) {
        iter_to_use = multi_iters[thread->rand.next() % multi_iters.size()];
      }

      generatekeyfromint(thread->rand.next() % flags_num, flags_num, &key);
      iter_to_use->seek(key);
      read++;
      if (iter_to_use->valid() && iter_to_use->key().compare(key) == 0) {
        found++;
      }
      thread->stats.finishedops(db_.db, 1);
    }
    delete single_iter;
    for (auto iter : multi_iters) {
      delete iter;
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "(%" priu64 " of %" priu64 " found)\n",
             found, read);
    thread->stats.addmessage(msg);
    if (flags_perf_level > 0) {
      thread->stats.addmessage(perf_context.tostring());
    }
  }

  void seekrandomwhilewriting(threadstate* thread) {
    if (thread->tid > 0) {
      seekrandom(thread);
    } else {
      bgwriter(thread);
    }
  }

  void dodelete(threadstate* thread, bool seq) {
    writebatch batch;
    duration duration(seq ? 0 : flags_duration, num_);
    int64_t i = 0;
    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());

    while (!duration.done(entries_per_batch_)) {
      db* db = selectdb(thread);
      batch.clear();
      for (int64_t j = 0; j < entries_per_batch_; ++j) {
        const int64_t k = seq ? i + j : (thread->rand.next() % flags_num);
        generatekeyfromint(k, flags_num, &key);
        batch.delete(key);
      }
      auto s = db->write(write_options_, &batch);
      thread->stats.finishedops(db, entries_per_batch_);
      if (!s.ok()) {
        fprintf(stderr, "del error: %s\n", s.tostring().c_str());
        exit(1);
      }
      i += entries_per_batch_;
    }
  }

  void deleteseq(threadstate* thread) {
    dodelete(thread, true);
  }

  void deleterandom(threadstate* thread) {
    dodelete(thread, false);
  }

  void readwhilewriting(threadstate* thread) {
    if (thread->tid > 0) {
      readrandom(thread);
    } else {
      bgwriter(thread);
    }
  }

  void bgwriter(threadstate* thread) {
    // special thread that keeps writing until other threads are done.
    randomgenerator gen;
    double last = flags_env->nowmicros();
    int writes_per_second_by_10 = 0;
    int num_writes = 0;

    // --writes_per_second rate limit is enforced per 100 milliseconds
    // intervals to avoid a burst of writes at the start of each second.

    if (flags_writes_per_second > 0)
      writes_per_second_by_10 = flags_writes_per_second / 10;

    // don't merge stats from this thread with the readers.
    thread->stats.setexcludefrommerge();

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());

    while (true) {
      db* db = selectdb(thread);
      {
        mutexlock l(&thread->shared->mu);
        if (thread->shared->num_done + 1 >= thread->shared->num_initialized) {
          // other threads have finished
          break;
        }
      }

      generatekeyfromint(thread->rand.next() % flags_num, flags_num, &key);
      status s = db->put(write_options_, key, gen.generate(value_size_));
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.tostring().c_str());
        exit(1);
      }
      thread->stats.finishedops(db_.db, 1);

      ++num_writes;
      if (writes_per_second_by_10 && num_writes >= writes_per_second_by_10) {
        double now = flags_env->nowmicros();
        double usecs_since_last = now - last;

        num_writes = 0;
        last = now;

        if (usecs_since_last < 100000.0) {
          flags_env->sleepformicroseconds(100000.0 - usecs_since_last);
          last = flags_env->nowmicros();
        }
      }
    }
  }

  // given a key k and value v, this puts (k+"0", v), (k+"1", v), (k+"2", v)
  // in db atomically i.e in a single batch. also refer getmany.
  status putmany(db* db, const writeoptions& writeoptions, const slice& key,
                 const slice& value) {
    std::string suffixes[3] = {"2", "1", "0"};
    std::string keys[3];

    writebatch batch;
    status s;
    for (int i = 0; i < 3; i++) {
      keys[i] = key.tostring() + suffixes[i];
      batch.put(keys[i], value);
    }

    s = db->write(writeoptions, &batch);
    return s;
  }


  // given a key k, this deletes (k+"0", v), (k+"1", v), (k+"2", v)
  // in db atomically i.e in a single batch. also refer getmany.
  status deletemany(db* db, const writeoptions& writeoptions,
                    const slice& key) {
    std::string suffixes[3] = {"1", "2", "0"};
    std::string keys[3];

    writebatch batch;
    status s;
    for (int i = 0; i < 3; i++) {
      keys[i] = key.tostring() + suffixes[i];
      batch.delete(keys[i]);
    }

    s = db->write(writeoptions, &batch);
    return s;
  }

  // given a key k and value v, this gets values for k+"0", k+"1" and k+"2"
  // in the same snapshot, and verifies that all the values are identical.
  // assumes that putmany was used to put (k, v) into the db.
  status getmany(db* db, const readoptions& readoptions, const slice& key,
                 std::string* value) {
    std::string suffixes[3] = {"0", "1", "2"};
    std::string keys[3];
    slice key_slices[3];
    std::string values[3];
    readoptions readoptionscopy = readoptions;
    readoptionscopy.snapshot = db->getsnapshot();
    status s;
    for (int i = 0; i < 3; i++) {
      keys[i] = key.tostring() + suffixes[i];
      key_slices[i] = keys[i];
      s = db->get(readoptionscopy, key_slices[i], value);
      if (!s.ok() && !s.isnotfound()) {
        fprintf(stderr, "get error: %s\n", s.tostring().c_str());
        values[i] = "";
        // we continue after error rather than exiting so that we can
        // find more errors if any
      } else if (s.isnotfound()) {
        values[i] = "";
      } else {
        values[i] = *value;
      }
    }
    db->releasesnapshot(readoptionscopy.snapshot);

    if ((values[0] != values[1]) || (values[1] != values[2])) {
      fprintf(stderr, "inconsistent values for key %s: %s, %s, %s\n",
              key.tostring().c_str(), values[0].c_str(), values[1].c_str(),
              values[2].c_str());
      // we continue after error rather than exiting so that we can
      // find more errors if any
    }

    return s;
  }

  // differs from readrandomwriterandom in the following ways:
  // (a) uses getmany/putmany to read/write key values. refer to those funcs.
  // (b) does deletes as well (per flags_deletepercent)
  // (c) in order to achieve high % of 'found' during lookups, and to do
  //     multiple writes (including puts and deletes) it uses upto
  //     flags_numdistinct distinct keys instead of flags_num distinct keys.
  // (d) does not have a multiget option.
  void randomwithverify(threadstate* thread) {
    readoptions options(flags_verify_checksum, true);
    randomgenerator gen;
    std::string value;
    int64_t found = 0;
    int get_weight = 0;
    int put_weight = 0;
    int delete_weight = 0;
    int64_t gets_done = 0;
    int64_t puts_done = 0;
    int64_t deletes_done = 0;

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());

    // the number of iterations is the larger of read_ or write_
    for (int64_t i = 0; i < readwrites_; i++) {
      db* db = selectdb(thread);
      if (get_weight == 0 && put_weight == 0 && delete_weight == 0) {
        // one batch completed, reinitialize for next batch
        get_weight = flags_readwritepercent;
        delete_weight = flags_deletepercent;
        put_weight = 100 - get_weight - delete_weight;
      }
      generatekeyfromint(thread->rand.next() % flags_numdistinct,
          flags_numdistinct, &key);
      if (get_weight > 0) {
        // do all the gets first
        status s = getmany(db, options, key, &value);
        if (!s.ok() && !s.isnotfound()) {
          fprintf(stderr, "getmany error: %s\n", s.tostring().c_str());
          // we continue after error rather than exiting so that we can
          // find more errors if any
        } else if (!s.isnotfound()) {
          found++;
        }
        get_weight--;
        gets_done++;
      } else if (put_weight > 0) {
        // then do all the corresponding number of puts
        // for all the gets we have done earlier
        status s = putmany(db, write_options_, key, gen.generate(value_size_));
        if (!s.ok()) {
          fprintf(stderr, "putmany error: %s\n", s.tostring().c_str());
          exit(1);
        }
        put_weight--;
        puts_done++;
      } else if (delete_weight > 0) {
        status s = deletemany(db, write_options_, key);
        if (!s.ok()) {
          fprintf(stderr, "deletemany error: %s\n", s.tostring().c_str());
          exit(1);
        }
        delete_weight--;
        deletes_done++;
      }

      thread->stats.finishedops(db_.db, 1);
    }
    char msg[100];
    snprintf(msg, sizeof(msg),
             "( get:%" priu64 " put:%" priu64 " del:%" priu64 " total:%" \
             priu64 " found:%" priu64 ")",
             gets_done, puts_done, deletes_done, readwrites_, found);
    thread->stats.addmessage(msg);
  }

  // this is different from readwhilewriting because it does not use
  // an extra thread.
  void readrandomwriterandom(threadstate* thread) {
    readoptions options(flags_verify_checksum, true);
    randomgenerator gen;
    std::string value;
    int64_t found = 0;
    int get_weight = 0;
    int put_weight = 0;
    int64_t reads_done = 0;
    int64_t writes_done = 0;
    duration duration(flags_duration, readwrites_);

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());

    // the number of iterations is the larger of read_ or write_
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      generatekeyfromint(thread->rand.next() % flags_num, flags_num, &key);
      if (get_weight == 0 && put_weight == 0) {
        // one batch completed, reinitialize for next batch
        get_weight = flags_readwritepercent;
        put_weight = 100 - get_weight;
      }
      if (get_weight > 0) {
        // do all the gets first
        status s = db->get(options, key, &value);
        if (!s.ok() && !s.isnotfound()) {
          fprintf(stderr, "get error: %s\n", s.tostring().c_str());
          // we continue after error rather than exiting so that we can
          // find more errors if any
        } else if (!s.isnotfound()) {
          found++;
        }
        get_weight--;
        reads_done++;
      } else  if (put_weight > 0) {
        // then do all the corresponding number of puts
        // for all the gets we have done earlier
        status s = db->put(write_options_, key, gen.generate(value_size_));
        if (!s.ok()) {
          fprintf(stderr, "put error: %s\n", s.tostring().c_str());
          exit(1);
        }
        put_weight--;
        writes_done++;
      }
      thread->stats.finishedops(db, 1);
    }
    char msg[100];
    snprintf(msg, sizeof(msg), "( reads:%" priu64 " writes:%" priu64 \
             " total:%" priu64 " found:%" priu64 ")",
             reads_done, writes_done, readwrites_, found);
    thread->stats.addmessage(msg);
  }

  //
  // read-modify-write for random keys
  void updaterandom(threadstate* thread) {
    readoptions options(flags_verify_checksum, true);
    randomgenerator gen;
    std::string value;
    int64_t found = 0;
    duration duration(flags_duration, readwrites_);

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    // the number of iterations is the larger of read_ or write_
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      generatekeyfromint(thread->rand.next() % flags_num, flags_num, &key);

      if (db->get(options, key, &value).ok()) {
        found++;
      }

      status s = db->put(write_options_, key, gen.generate(value_size_));
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.tostring().c_str());
        exit(1);
      }
      thread->stats.finishedops(db, 1);
    }
    char msg[100];
    snprintf(msg, sizeof(msg),
             "( updates:%" priu64 " found:%" priu64 ")", readwrites_, found);
    thread->stats.addmessage(msg);
  }

  // read-modify-write for random keys.
  // each operation causes the key grow by value_size (simulating an append).
  // generally used for benchmarking against merges of similar type
  void appendrandom(threadstate* thread) {
    readoptions options(flags_verify_checksum, true);
    randomgenerator gen;
    std::string value;
    int64_t found = 0;

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    // the number of iterations is the larger of read_ or write_
    duration duration(flags_duration, readwrites_);
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      generatekeyfromint(thread->rand.next() % flags_num, flags_num, &key);

      // get the existing value
      if (db->get(options, key, &value).ok()) {
        found++;
      } else {
        // if not existing, then just assume an empty string of data
        value.clear();
      }

      // update the value (by appending data)
      slice operand = gen.generate(value_size_);
      if (value.size() > 0) {
        // use a delimeter to match the semantics for stringappendoperator
        value.append(1,',');
      }
      value.append(operand.data(), operand.size());

      // write back to the database
      status s = db->put(write_options_, key, value);
      if (!s.ok()) {
        fprintf(stderr, "put error: %s\n", s.tostring().c_str());
        exit(1);
      }
      thread->stats.finishedops(db, 1);
    }

    char msg[100];
    snprintf(msg, sizeof(msg), "( updates:%" priu64 " found:%" priu64 ")",
            readwrites_, found);
    thread->stats.addmessage(msg);
  }

  // read-modify-write for random keys (using mergeoperator)
  // the merge operator to use should be defined by flags_merge_operator
  // adjust flags_value_size so that the keys are reasonable for this operator
  // assumes that the merge operator is non-null (i.e.: is well-defined)
  //
  // for example, use flags_merge_operator="uint64add" and flags_value_size=8
  // to simulate random additions over 64-bit integers using merge.
  //
  // the number of merges on the same key can be controlled by adjusting
  // flags_merge_keys.
  void mergerandom(threadstate* thread) {
    randomgenerator gen;

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    // the number of iterations is the larger of read_ or write_
    duration duration(flags_duration, readwrites_);
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      generatekeyfromint(thread->rand.next() % merge_keys_, merge_keys_, &key);

      status s = db->merge(write_options_, key, gen.generate(value_size_));

      if (!s.ok()) {
        fprintf(stderr, "merge error: %s\n", s.tostring().c_str());
        exit(1);
      }
      thread->stats.finishedops(db, 1);
    }

    // print some statistics
    char msg[100];
    snprintf(msg, sizeof(msg), "( updates:%" priu64 ")", readwrites_);
    thread->stats.addmessage(msg);
  }

  // read and merge random keys. the amount of reads and merges are controlled
  // by adjusting flags_num and flags_mergereadpercent. the number of distinct
  // keys (and thus also the number of reads and merges on the same key) can be
  // adjusted with flags_merge_keys.
  //
  // as with mergerandom, the merge operator to use should be defined by
  // flags_merge_operator.
  void readrandommergerandom(threadstate* thread) {
    readoptions options(flags_verify_checksum, true);
    randomgenerator gen;
    std::string value;
    int64_t num_hits = 0;
    int64_t num_gets = 0;
    int64_t num_merges = 0;
    size_t max_length = 0;

    slice key = allocatekey();
    std::unique_ptr<const char[]> key_guard(key.data());
    // the number of iterations is the larger of read_ or write_
    duration duration(flags_duration, readwrites_);
    while (!duration.done(1)) {
      db* db = selectdb(thread);
      generatekeyfromint(thread->rand.next() % merge_keys_, merge_keys_, &key);

      bool do_merge = int(thread->rand.next() % 100) < flags_mergereadpercent;

      if (do_merge) {
        status s = db->merge(write_options_, key, gen.generate(value_size_));
        if (!s.ok()) {
          fprintf(stderr, "merge error: %s\n", s.tostring().c_str());
          exit(1);
        }

        num_merges++;

      } else {
        status s = db->get(options, key, &value);
        if (value.length() > max_length)
          max_length = value.length();

        if (!s.ok() && !s.isnotfound()) {
          fprintf(stderr, "get error: %s\n", s.tostring().c_str());
          // we continue after error rather than exiting so that we can
          // find more errors if any
        } else if (!s.isnotfound()) {
          num_hits++;
        }

        num_gets++;

      }

      thread->stats.finishedops(db, 1);
    }

    char msg[100];
    snprintf(msg, sizeof(msg),
             "(reads:%" priu64 " merges:%" priu64 " total:%" priu64 " hits:%" \
             priu64 " maxlength:%zu)",
             num_gets, num_merges, readwrites_, num_hits, max_length);
    thread->stats.addmessage(msg);
  }

  void compact(threadstate* thread) {
    db* db = selectdb(thread);
    db->compactrange(nullptr, nullptr);
  }

  void printstats(const char* key) {
    if (db_.db != nullptr) {
      printstats(db_.db, key, false);
    }
    for (const auto& db_with_cfh : multi_dbs_) {
      printstats(db_with_cfh.db, key, true);
    }
  }

  void printstats(db* db, const char* key, bool print_header = false) {
    if (print_header) {
      fprintf(stdout, "\n==== db: %s ===\n", db->getname().c_str());
    }
    std::string stats;
    if (!db->getproperty(key, &stats)) {
      stats = "(failed)";
    }
    fprintf(stdout, "\n%s\n", stats.c_str());
  }
};

}  // namespace rocksdb

int main(int argc, char** argv) {
  rocksdb::port::installstacktracehandler();
  setusagemessage(std::string("\nusage:\n") + std::string(argv[0]) +
                  " [options]...");
  parsecommandlineflags(&argc, &argv, true);

  flags_compaction_style_e = (rocksdb::compactionstyle) flags_compaction_style;
  if (flags_statistics) {
    dbstats = rocksdb::createdbstatistics();
  }

  std::vector<std::string> fanout =
    rocksdb::stringsplit(flags_max_bytes_for_level_multiplier_additional, ',');
  for (unsigned int j= 0; j < fanout.size(); j++) {
    flags_max_bytes_for_level_multiplier_additional_v.push_back(
      std::stoi(fanout[j]));
  }

  flags_compression_type_e =
    stringtocompressiontype(flags_compression_type.c_str());

  if (!flags_hdfs.empty()) {
    flags_env  = new rocksdb::hdfsenv(flags_hdfs);
  }

  if (!strcasecmp(flags_compaction_fadvice.c_str(), "none"))
    flags_compaction_fadvice_e = rocksdb::options::none;
  else if (!strcasecmp(flags_compaction_fadvice.c_str(), "normal"))
    flags_compaction_fadvice_e = rocksdb::options::normal;
  else if (!strcasecmp(flags_compaction_fadvice.c_str(), "sequential"))
    flags_compaction_fadvice_e = rocksdb::options::sequential;
  else if (!strcasecmp(flags_compaction_fadvice.c_str(), "willneed"))
    flags_compaction_fadvice_e = rocksdb::options::willneed;
  else {
    fprintf(stdout, "unknown compaction fadvice:%s\n",
            flags_compaction_fadvice.c_str());
  }

  flags_rep_factory = stringtorepfactory(flags_memtablerep.c_str());

  // the number of background threads should be at least as much the
  // max number of concurrent compactions.
  flags_env->setbackgroundthreads(flags_max_background_compactions);
  // choose a location for the test database if none given with --db=<path>
  if (flags_db.empty()) {
    std::string default_db_path;
    rocksdb::env::default()->gettestdirectory(&default_db_path);
    default_db_path += "/dbbench";
    flags_db = default_db_path;
  }

  rocksdb::benchmark benchmark;
  benchmark.run();
  return 0;
}

#endif  // gflags
