//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//
// the test uses an array to compare against values written to the database.
// keys written to the array are in 1:1 correspondence to the actual values in
// the database according to the formula in the function generatevalue.

// space is reserved in the array from 0 to flags_max_key and values are
// randomly written/deleted/read from those positions. during verification we
// compare all the positions in the array. to shorten/elongate the running
// time, you could change the settings: flags_max_key, flags_ops_per_thread,
// (sometimes also flags_threads).
//
// note that if flags_test_batches_snapshots is set, the test will have
// different behavior. see comment of the flag for details.

#ifndef gflags
#include <cstdio>
int main() {
  fprintf(stderr, "please install gflags to run rocksdb tools\n");
  return 1;
}
#else

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <gflags/gflags.h>
#include "db/db_impl.h"
#include "db/version_set.h"
#include "rocksdb/statistics.h"
#include "rocksdb/cache.h"
#include "rocksdb/utilities/db_ttl.h"
#include "rocksdb/env.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "port/port.h"
#include "util/coding.h"
#include "util/crc32c.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"
#include "util/logging.h"
#include "hdfs/env_hdfs.h"
#include "utilities/merge_operators.h"

using gflags::parsecommandlineflags;
using gflags::registerflagvalidator;
using gflags::setusagemessage;

static const long kb = 1024;

static bool validateuint32range(const char* flagname, uint64_t value) {
  if (value > std::numeric_limits<uint32_t>::max()) {
    fprintf(stderr,
            "invalid value for --%s: %lu, overflow\n",
            flagname,
            (unsigned long)value);
    return false;
  }
  return true;
}

define_uint64(seed, 2341234, "seed for prng");
static const bool flags_seed_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_seed, &validateuint32range);

define_int64(max_key, 1 * kb* kb,
             "max number of key/values to place in database");

define_int32(column_families, 10, "number of column families");

define_bool(test_batches_snapshots, false,
            "if set, the test uses multiget(), multiut() and multidelete()"
            " which read/write/delete multiple keys in a batch. in this mode,"
            " we do not verify db content by comparing the content with the "
            "pre-allocated array. instead, we do partial verification inside"
            " multiget() by checking various values in a batch. benefit of"
            " this mode:\n"
            "\t(a) no need to acquire mutexes during writes (less cache "
            "flushes in multi-core leading to speed up)\n"
            "\t(b) no long validation at the end (more speed up)\n"
            "\t(c) test snapshot and atomicity of batch writes");

define_int32(threads, 32, "number of concurrent threads to run.");

define_int32(ttl, -1,
             "opens the db with this ttl value if this is not -1. "
             "carefully specify a large value such that verifications on "
             "deleted values don't fail");

define_int32(value_size_mult, 8,
             "size of value will be this number times rand_int(1,3) bytes");

define_bool(verify_before_write, false, "verify before write");

define_bool(histogram, false, "print histogram of operation timings");

define_bool(destroy_db_initially, true,
            "destroys the database dir before start if this is true");

define_bool(verbose, false, "verbose");

define_bool(progress_reports, true,
            "if true, db_stress will report number of finished operations");

define_int32(write_buffer_size, rocksdb::options().write_buffer_size,
             "number of bytes to buffer in memtable before compacting");

define_int32(max_write_buffer_number,
             rocksdb::options().max_write_buffer_number,
             "the number of in-memory memtables. "
             "each memtable is of size flags_write_buffer_size.");

define_int32(min_write_buffer_number_to_merge,
             rocksdb::options().min_write_buffer_number_to_merge,
             "the minimum number of write buffers that will be merged together "
             "before writing to storage. this is cheap because it is an "
             "in-memory merge. if this feature is not enabled, then all these "
             "write buffers are flushed to l0 as separate files and this "
             "increases read amplification because a get request has to check "
             "in all of these files. also, an in-memory merge may result in "
             "writing less data to storage if there are duplicate records in"
             " each of these individual write buffers.");

define_int32(open_files, rocksdb::options().max_open_files,
             "maximum number of files to keep open at the same time "
             "(use default if == 0)");

define_int64(compressed_cache_size, -1,
             "number of bytes to use as a cache of compressed data."
             " negative means use default settings.");

define_int32(compaction_style, rocksdb::options().compaction_style, "");

define_int32(level0_file_num_compaction_trigger,
             rocksdb::options().level0_file_num_compaction_trigger,
             "level0 compaction start trigger");

define_int32(level0_slowdown_writes_trigger,
             rocksdb::options().level0_slowdown_writes_trigger,
             "number of files in level-0 that will slow down writes");

define_int32(level0_stop_writes_trigger,
             rocksdb::options().level0_stop_writes_trigger,
             "number of files in level-0 that will trigger put stop.");

define_int32(block_size, rocksdb::blockbasedtableoptions().block_size,
             "number of bytes in a block.");

define_int32(max_background_compactions,
             rocksdb::options().max_background_compactions,
             "the maximum number of concurrent background compactions "
             "that can occur in parallel.");

define_int32(compaction_thread_pool_adjust_interval, 0,
             "the interval (in milliseconds) to adjust compaction thread pool "
             "size. don't change it periodically if the value is 0.");

define_int32(compaction_thread_pool_varations, 2,
             "range of bakground thread pool size variations when adjusted "
             "periodically.");

define_int32(max_background_flushes, rocksdb::options().max_background_flushes,
             "the maximum number of concurrent background flushes "
             "that can occur in parallel.");

define_int32(universal_size_ratio, 0, "the ratio of file sizes that trigger"
             " compaction in universal style");

define_int32(universal_min_merge_width, 0, "the minimum number of files to "
             "compact in universal style compaction");

define_int32(universal_max_merge_width, 0, "the max number of files to compact"
             " in universal style compaction");

define_int32(universal_max_size_amplification_percent, 0,
             "the max size amplification for universal style compaction");

define_int32(clear_column_family_one_in, 1000000,
             "with a chance of 1/n, delete a column family and then recreate "
             "it again. if n == 0, never drop/create column families. "
             "when test_batches_snapshots is true, this flag has no effect");

define_int64(cache_size, 2 * kb * kb * kb,
             "number of bytes to use as a cache of uncompressed data.");

static bool validateint32positive(const char* flagname, int32_t value) {
  if (value < 0) {
    fprintf(stderr, "invalid value for --%s: %d, must be >=0\n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(reopen, 10, "number of times database reopens");
static const bool flags_reopen_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_reopen, &validateint32positive);

define_int32(bloom_bits, 10, "bloom filter bits per key. "
             "negative means use default settings.");

define_string(db, "", "use the db with the following name.");

define_bool(verify_checksum, false,
            "verify checksum for every block read from storage");

define_bool(mmap_read, rocksdb::envoptions().use_mmap_reads,
            "allow reads to occur via mmap-ing files");

// database statistics
static std::shared_ptr<rocksdb::statistics> dbstats;
define_bool(statistics, false, "create database statistics");

define_bool(sync, false, "sync all writes to disk");

define_bool(disable_data_sync, false,
            "if true, do not wait until data is synced to disk.");

define_bool(use_fsync, false, "if true, issue fsync instead of fdatasync");

define_int32(kill_random_test, 0,
             "if non-zero, kill at various points in source code with "
             "probability 1/this");
static const bool flags_kill_random_test_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_kill_random_test, &validateint32positive);
extern int rocksdb_kill_odds;

define_bool(disable_wal, false, "if true, do not write wal for write.");

define_int32(target_file_size_base, 64 * kb,
             "target level-1 file size for compaction");

define_int32(target_file_size_multiplier, 1,
             "a multiplier to compute targe level-n file size (n >= 2)");

define_uint64(max_bytes_for_level_base, 256 * kb, "max bytes for level-1");

define_int32(max_bytes_for_level_multiplier, 2,
             "a multiplier to compute max bytes for level-n (n >= 2)");

static bool validateint32percent(const char* flagname, int32_t value) {
  if (value < 0 || value>100) {
    fprintf(stderr, "invalid value for --%s: %d, 0<= pct <=100 \n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(readpercent, 10,
             "ratio of reads to total workload (expressed as a percentage)");
static const bool flags_readpercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_readpercent, &validateint32percent);

define_int32(prefixpercent, 20,
             "ratio of prefix iterators to total workload (expressed as a"
             " percentage)");
static const bool flags_prefixpercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_prefixpercent, &validateint32percent);

define_int32(writepercent, 45,
             " ratio of deletes to total workload (expressed as a percentage)");
static const bool flags_writepercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_writepercent, &validateint32percent);

define_int32(delpercent, 15,
             "ratio of deletes to total workload (expressed as a percentage)");
static const bool flags_delpercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_delpercent, &validateint32percent);

define_int32(iterpercent, 10, "ratio of iterations to total workload"
             " (expressed as a percentage)");
static const bool flags_iterpercent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_iterpercent, &validateint32percent);

define_uint64(num_iterations, 10, "number of iterations per multiiterate run");
static const bool flags_num_iterations_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_num_iterations, &validateuint32range);

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

define_string(hdfs, "", "name of hdfs environment");
// posix or hdfs environment
static rocksdb::env* flags_env = rocksdb::env::default();

define_uint64(ops_per_thread, 1200000, "number of operations per thread.");
static const bool flags_ops_per_thread_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_ops_per_thread, &validateuint32range);

define_uint64(log2_keys_per_lock, 2, "log2 of number of keys per lock");
static const bool flags_log2_keys_per_lock_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_log2_keys_per_lock, &validateuint32range);

define_int32(purge_redundant_percent, 50,
             "percentage of times we want to purge redundant keys in memory "
             "before flushing");
static const bool flags_purge_redundant_percent_dummy __attribute__((unused)) =
    registerflagvalidator(&flags_purge_redundant_percent,
                          &validateint32percent);

define_bool(filter_deletes, false, "on true, deletes use keymayexist to drop"
            " the delete if key not present");

enum repfactory {
  kskiplist,
  khashskiplist,
  kvectorrep
};

namespace {
enum repfactory stringtorepfactory(const char* ctype) {
  assert(ctype);

  if (!strcasecmp(ctype, "skip_list"))
    return kskiplist;
  else if (!strcasecmp(ctype, "prefix_hash"))
    return khashskiplist;
  else if (!strcasecmp(ctype, "vector"))
    return kvectorrep;

  fprintf(stdout, "cannot parse memreptable %s\n", ctype);
  return kskiplist;
}
}  // namespace

static enum repfactory flags_rep_factory;
define_string(memtablerep, "prefix_hash", "");

static bool validateprefixsize(const char* flagname, int32_t value) {
  if (value < 0 || value > 8) {
    fprintf(stderr, "invalid value for --%s: %d. 0 <= prefixsize <= 8\n",
            flagname, value);
    return false;
  }
  return true;
}
define_int32(prefix_size, 7, "control the prefix size for hashskiplistrep");
static const bool flags_prefix_size_dummy =
    registerflagvalidator(&flags_prefix_size, &validateprefixsize);

define_bool(use_merge, false, "on true, replaces all writes with a merge "
            "that behaves like a put");


namespace rocksdb {

// convert long to a big-endian slice key
static std::string key(long val) {
  std::string little_endian_key;
  std::string big_endian_key;
  putfixed64(&little_endian_key, val);
  assert(little_endian_key.size() == sizeof(val));
  big_endian_key.resize(sizeof(val));
  for (int i=0; i<(int)sizeof(val); i++) {
    big_endian_key[i] = little_endian_key[sizeof(val) - 1 - i];
  }
  return big_endian_key;
}

static std::string stringtohex(const std::string& str) {
  std::string result = "0x";
  char buf[10];
  for (size_t i = 0; i < str.length(); i++) {
    snprintf(buf, 10, "%02x", (unsigned char)str[i]);
    result += buf;
  }
  return result;
}


class stresstest;
namespace {

class stats {
 private:
  double start_;
  double finish_;
  double seconds_;
  long done_;
  long gets_;
  long prefixes_;
  long writes_;
  long deletes_;
  long iterator_size_sums_;
  long founds_;
  long iterations_;
  long errors_;
  int next_report_;
  size_t bytes_;
  double last_op_finish_;
  histogramimpl hist_;

 public:
  stats() { }

  void start() {
    next_report_ = 100;
    hist_.clear();
    done_ = 0;
    gets_ = 0;
    prefixes_ = 0;
    writes_ = 0;
    deletes_ = 0;
    iterator_size_sums_ = 0;
    founds_ = 0;
    iterations_ = 0;
    errors_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    start_ = flags_env->nowmicros();
    last_op_finish_ = start_;
    finish_ = start_;
  }

  void merge(const stats& other) {
    hist_.merge(other.hist_);
    done_ += other.done_;
    gets_ += other.gets_;
    prefixes_ += other.prefixes_;
    writes_ += other.writes_;
    deletes_ += other.deletes_;
    iterator_size_sums_ += other.iterator_size_sums_;
    founds_ += other.founds_;
    iterations_ += other.iterations_;
    errors_ += other.errors_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;
  }

  void stop() {
    finish_ = flags_env->nowmicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void finishedsingleop() {
    if (flags_histogram) {
      double now = flags_env->nowmicros();
      double micros = now - last_op_finish_;
      hist_.add(micros);
      if (micros > 20000) {
        fprintf(stdout, "long op: %.1f micros%30s\r", micros, "");
      }
      last_op_finish_ = now;
    }

      done_++;
    if (flags_progress_reports) {
      if (done_ >= next_report_) {
        if      (next_report_ < 1000)   next_report_ += 100;
        else if (next_report_ < 5000)   next_report_ += 500;
        else if (next_report_ < 10000)  next_report_ += 1000;
        else if (next_report_ < 50000)  next_report_ += 5000;
        else if (next_report_ < 100000) next_report_ += 10000;
        else if (next_report_ < 500000) next_report_ += 50000;
        else                            next_report_ += 100000;
        fprintf(stdout, "... finished %ld ops%30s\r", done_, "");
      }
    }
  }

  void addbytesforwrites(int nwrites, size_t nbytes) {
    writes_ += nwrites;
    bytes_ += nbytes;
  }

  void addgets(int ngets, int nfounds) {
    founds_ += nfounds;
    gets_ += ngets;
  }

  void addprefixes(int nprefixes, int count) {
    prefixes_ += nprefixes;
    iterator_size_sums_ += count;
  }

  void additerations(int n) {
    iterations_ += n;
  }

  void adddeletes(int n) {
    deletes_ += n;
  }

  void adderrors(int n) {
    errors_ += n;
  }

  void report(const char* name) {
    std::string extra;
    if (bytes_ < 1 || done_ < 1) {
      fprintf(stderr, "no writes or ops?\n");
      return;
    }

    double elapsed = (finish_ - start_) * 1e-6;
    double bytes_mb = bytes_ / 1048576.0;
    double rate = bytes_mb / elapsed;
    double throughput = (double)done_/elapsed;

    fprintf(stdout, "%-12s: ", name);
    fprintf(stdout, "%.3f micros/op %ld ops/sec\n",
            seconds_ * 1e6 / done_, (long)throughput);
    fprintf(stdout, "%-12s: wrote %.2f mb (%.2f mb/sec) (%ld%% of %ld ops)\n",
            "", bytes_mb, rate, (100*writes_)/done_, done_);
    fprintf(stdout, "%-12s: wrote %ld times\n", "", writes_);
    fprintf(stdout, "%-12s: deleted %ld times\n", "", deletes_);
    fprintf(stdout, "%-12s: %ld read and %ld found the key\n", "",
            gets_, founds_);
    fprintf(stdout, "%-12s: prefix scanned %ld times\n", "", prefixes_);
    fprintf(stdout, "%-12s: iterator size sum is %ld\n", "",
            iterator_size_sums_);
    fprintf(stdout, "%-12s: iterated %ld times\n", "", iterations_);
    fprintf(stdout, "%-12s: got errors %ld times\n", "", errors_);

    if (flags_histogram) {
      fprintf(stdout, "microseconds per op:\n%s\n", hist_.tostring().c_str());
    }
    fflush(stdout);
  }
};

// state shared by all concurrent executions of the same benchmark.
class sharedstate {
 public:
  static const uint32_t sentinel;

  explicit sharedstate(stresstest* stress_test)
      : cv_(&mu_),
        seed_(flags_seed),
        max_key_(flags_max_key),
        log2_keys_per_lock_(flags_log2_keys_per_lock),
        num_threads_(flags_threads),
        num_initialized_(0),
        num_populated_(0),
        vote_reopen_(0),
        num_done_(0),
        start_(false),
        start_verify_(false),
        should_stop_bg_thread_(false),
        bg_thread_finished_(false),
        stress_test_(stress_test),
        verification_failure_(false) {
    if (flags_test_batches_snapshots) {
      fprintf(stdout, "no lock creation because test_batches_snapshots set\n");
      return;
    }
    values_.resize(flags_column_families);

    for (int i = 0; i < flags_column_families; ++i) {
      values_[i] = std::vector<uint32_t>(max_key_, sentinel);
    }

    long num_locks = (max_key_ >> log2_keys_per_lock_);
    if (max_key_ & ((1 << log2_keys_per_lock_) - 1)) {
      num_locks++;
    }
    fprintf(stdout, "creating %ld locks\n", num_locks * flags_column_families);
    key_locks_.resize(flags_column_families);
    for (int i = 0; i < flags_column_families; ++i) {
      key_locks_[i] = std::vector<port::mutex>(num_locks);
    }
  }

  ~sharedstate() {}

  port::mutex* getmutex() {
    return &mu_;
  }

  port::condvar* getcondvar() {
    return &cv_;
  }

  stresstest* getstresstest() const {
    return stress_test_;
  }

  long getmaxkey() const {
    return max_key_;
  }

  uint32_t getnumthreads() const {
    return num_threads_;
  }

  void incinitialized() {
    num_initialized_++;
  }

  void incoperated() {
    num_populated_++;
  }

  void incdone() {
    num_done_++;
  }

  void incvotedreopen() {
    vote_reopen_ = (vote_reopen_ + 1) % num_threads_;
  }

  bool allinitialized() const {
    return num_initialized_ >= num_threads_;
  }

  bool alloperated() const {
    return num_populated_ >= num_threads_;
  }

  bool alldone() const {
    return num_done_ >= num_threads_;
  }

  bool allvotedreopen() {
    return (vote_reopen_ == 0);
  }

  void setstart() {
    start_ = true;
  }

  void setstartverify() {
    start_verify_ = true;
  }

  bool started() const {
    return start_;
  }

  bool verifystarted() const {
    return start_verify_;
  }

  void setverificationfailure() { verification_failure_.store(true); }

  bool hasverificationfailedyet() { return verification_failure_.load(); }

  port::mutex* getmutexforkey(int cf, long key) {
    return &key_locks_[cf][key >> log2_keys_per_lock_];
  }

  void lockcolumnfamily(int cf) {
    for (auto& mutex : key_locks_[cf]) {
      mutex.lock();
    }
  }

  void unlockcolumnfamily(int cf) {
    for (auto& mutex : key_locks_[cf]) {
      mutex.unlock();
    }
  }

  void clearcolumnfamily(int cf) {
    std::fill(values_[cf].begin(), values_[cf].end(), sentinel);
  }

  void put(int cf, long key, uint32_t value_base) {
    values_[cf][key] = value_base;
  }

  uint32_t get(int cf, long key) const { return values_[cf][key]; }

  void delete(int cf, long key) { values_[cf][key] = sentinel; }

  uint32_t getseed() const { return seed_; }

  void setshouldstopbgthread() { should_stop_bg_thread_ = true; }

  bool shoudstopbgthread() { return should_stop_bg_thread_; }

  void setbgthreadfinish() { bg_thread_finished_ = true; }

  bool bgthreadfinished() const { return bg_thread_finished_; }

 private:
  port::mutex mu_;
  port::condvar cv_;
  const uint32_t seed_;
  const long max_key_;
  const uint32_t log2_keys_per_lock_;
  const int num_threads_;
  long num_initialized_;
  long num_populated_;
  long vote_reopen_;
  long num_done_;
  bool start_;
  bool start_verify_;
  bool should_stop_bg_thread_;
  bool bg_thread_finished_;
  stresstest* stress_test_;
  std::atomic<bool> verification_failure_;

  std::vector<std::vector<uint32_t>> values_;
  std::vector<std::vector<port::mutex>> key_locks_;
};

const uint32_t sharedstate::sentinel = 0xffffffff;

// per-thread state for concurrent executions of the same benchmark.
struct threadstate {
  uint32_t tid; // 0..n-1
  random rand;  // has different seeds for different threads
  sharedstate* shared;
  stats stats;

  threadstate(uint32_t index, sharedstate *shared)
      : tid(index),
        rand(1000 + index + shared->getseed()),
        shared(shared) {
  }
};

}  // namespace

class stresstest {
 public:
  stresstest()
      : cache_(newlrucache(flags_cache_size)),
        compressed_cache_(flags_compressed_cache_size >= 0
                              ? newlrucache(flags_compressed_cache_size)
                              : nullptr),
        filter_policy_(flags_bloom_bits >= 0
                           ? newbloomfilterpolicy(flags_bloom_bits)
                           : nullptr),
        db_(nullptr),
        new_column_family_name_(1),
        num_times_reopened_(0) {
    if (flags_destroy_db_initially) {
      std::vector<std::string> files;
      flags_env->getchildren(flags_db, &files);
      for (unsigned int i = 0; i < files.size(); i++) {
        if (slice(files[i]).starts_with("heap-")) {
          flags_env->deletefile(flags_db + "/" + files[i]);
        }
      }
      destroydb(flags_db, options());
    }
  }

  ~stresstest() {
    for (auto cf : column_families_) {
      delete cf;
    }
    column_families_.clear();
    delete db_;
  }

  bool run() {
    printenv();
    open();
    sharedstate shared(this);
    uint32_t n = shared.getnumthreads();

    std::vector<threadstate*> threads(n);
    for (uint32_t i = 0; i < n; i++) {
      threads[i] = new threadstate(i, &shared);
      flags_env->startthread(threadbody, threads[i]);
    }
    threadstate bg_thread(0, &shared);
    if (flags_compaction_thread_pool_adjust_interval > 0) {
      flags_env->startthread(poolsizechangethread, &bg_thread);
    }

    // each thread goes through the following states:
    // initializing -> wait for others to init -> read/populate/depopulate
    // wait for others to operate -> verify -> done

    {
      mutexlock l(shared.getmutex());
      while (!shared.allinitialized()) {
        shared.getcondvar()->wait();
      }

      double now = flags_env->nowmicros();
      fprintf(stdout, "%s starting database operations\n",
              flags_env->timetostring((uint64_t) now/1000000).c_str());

      shared.setstart();
      shared.getcondvar()->signalall();
      while (!shared.alloperated()) {
        shared.getcondvar()->wait();
      }

      now = flags_env->nowmicros();
      if (flags_test_batches_snapshots) {
        fprintf(stdout, "%s limited verification already done during gets\n",
                flags_env->timetostring((uint64_t) now/1000000).c_str());
      } else {
        fprintf(stdout, "%s starting verification\n",
                flags_env->timetostring((uint64_t) now/1000000).c_str());
      }

      shared.setstartverify();
      shared.getcondvar()->signalall();
      while (!shared.alldone()) {
        shared.getcondvar()->wait();
      }
    }

    for (unsigned int i = 1; i < n; i++) {
      threads[0]->stats.merge(threads[i]->stats);
    }
    threads[0]->stats.report("stress test");

    for (unsigned int i = 0; i < n; i++) {
      delete threads[i];
      threads[i] = nullptr;
    }
    double now = flags_env->nowmicros();
    if (!flags_test_batches_snapshots) {
      fprintf(stdout, "%s verification successful\n",
              flags_env->timetostring((uint64_t) now/1000000).c_str());
    }
    printstatistics();

    if (flags_compaction_thread_pool_adjust_interval > 0) {
      mutexlock l(shared.getmutex());
      shared.setshouldstopbgthread();
      while (!shared.bgthreadfinished()) {
        shared.getcondvar()->wait();
      }
    }

    if (shared.hasverificationfailedyet()) {
      printf("verification failed :(\n");
      return false;
    }
    return true;
  }

 private:

  static void threadbody(void* v) {
    threadstate* thread = reinterpret_cast<threadstate*>(v);
    sharedstate* shared = thread->shared;

    {
      mutexlock l(shared->getmutex());
      shared->incinitialized();
      if (shared->allinitialized()) {
        shared->getcondvar()->signalall();
      }
      while (!shared->started()) {
        shared->getcondvar()->wait();
      }
    }
    thread->shared->getstresstest()->operatedb(thread);

    {
      mutexlock l(shared->getmutex());
      shared->incoperated();
      if (shared->alloperated()) {
        shared->getcondvar()->signalall();
      }
      while (!shared->verifystarted()) {
        shared->getcondvar()->wait();
      }
    }

    if (!flags_test_batches_snapshots) {
      thread->shared->getstresstest()->verifydb(thread);
    }

    {
      mutexlock l(shared->getmutex());
      shared->incdone();
      if (shared->alldone()) {
        shared->getcondvar()->signalall();
      }
    }

  }

  static void poolsizechangethread(void* v) {
    assert(flags_compaction_thread_pool_adjust_interval > 0);
    threadstate* thread = reinterpret_cast<threadstate*>(v);
    sharedstate* shared = thread->shared;

    while (true) {
      {
        mutexlock l(shared->getmutex());
        if (shared->shoudstopbgthread()) {
          shared->setbgthreadfinish();
          shared->getcondvar()->signalall();
          return;
        }
      }

      auto thread_pool_size_base = flags_max_background_compactions;
      auto thread_pool_size_var = flags_compaction_thread_pool_varations;
      int new_thread_pool_size =
          thread_pool_size_base - thread_pool_size_var +
          thread->rand.next() % (thread_pool_size_var * 2 + 1);
      if (new_thread_pool_size < 1) {
        new_thread_pool_size = 1;
      }
      flags_env->setbackgroundthreads(new_thread_pool_size);
      // sleep up to 3 seconds
      flags_env->sleepformicroseconds(
          thread->rand.next() % flags_compaction_thread_pool_adjust_interval *
              1000 +
          1);
    }
  }

  // given a key k and value v, this puts ("0"+k, "0"+v), ("1"+k, "1"+v), ...
  // ("9"+k, "9"+v) in db atomically i.e in a single batch.
  // also refer multiget.
  status multiput(threadstate* thread, const writeoptions& writeoptions,
                  columnfamilyhandle* column_family, const slice& key,
                  const slice& value, size_t sz) {
    std::string keys[10] = {"9", "8", "7", "6", "5",
                            "4", "3", "2", "1", "0"};
    std::string values[10] = {"9", "8", "7", "6", "5",
                              "4", "3", "2", "1", "0"};
    slice value_slices[10];
    writebatch batch;
    status s;
    for (int i = 0; i < 10; i++) {
      keys[i] += key.tostring();
      values[i] += value.tostring();
      value_slices[i] = values[i];
      if (flags_use_merge) {
        batch.merge(column_family, keys[i], value_slices[i]);
      } else {
        batch.put(column_family, keys[i], value_slices[i]);
      }
    }

    s = db_->write(writeoptions, &batch);
    if (!s.ok()) {
      fprintf(stderr, "multiput error: %s\n", s.tostring().c_str());
      thread->stats.adderrors(1);
    } else {
      // we did 10 writes each of size sz + 1
      thread->stats.addbytesforwrites(10, (sz + 1) * 10);
    }

    return s;
  }

  // given a key k, this deletes ("0"+k), ("1"+k),... ("9"+k)
  // in db atomically i.e in a single batch. also refer multiget.
  status multidelete(threadstate* thread, const writeoptions& writeoptions,
                     columnfamilyhandle* column_family, const slice& key) {
    std::string keys[10] = {"9", "7", "5", "3", "1",
                            "8", "6", "4", "2", "0"};

    writebatch batch;
    status s;
    for (int i = 0; i < 10; i++) {
      keys[i] += key.tostring();
      batch.delete(column_family, keys[i]);
    }

    s = db_->write(writeoptions, &batch);
    if (!s.ok()) {
      fprintf(stderr, "multidelete error: %s\n", s.tostring().c_str());
      thread->stats.adderrors(1);
    } else {
      thread->stats.adddeletes(10);
    }

    return s;
  }

  // given a key k, this gets values for "0"+k, "1"+k,..."9"+k
  // in the same snapshot, and verifies that all the values are of the form
  // "0"+v, "1"+v,..."9"+v.
  // assumes that multiput was used to put (k, v) into the db.
  status multiget(threadstate* thread, const readoptions& readoptions,
                  columnfamilyhandle* column_family, const slice& key,
                  std::string* value) {
    std::string keys[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
    slice key_slices[10];
    std::string values[10];
    readoptions readoptionscopy = readoptions;
    readoptionscopy.snapshot = db_->getsnapshot();
    status s;
    for (int i = 0; i < 10; i++) {
      keys[i] += key.tostring();
      key_slices[i] = keys[i];
      s = db_->get(readoptionscopy, column_family, key_slices[i], value);
      if (!s.ok() && !s.isnotfound()) {
        fprintf(stderr, "get error: %s\n", s.tostring().c_str());
        values[i] = "";
        thread->stats.adderrors(1);
        // we continue after error rather than exiting so that we can
        // find more errors if any
      } else if (s.isnotfound()) {
        values[i] = "";
        thread->stats.addgets(1, 0);
      } else {
        values[i] = *value;

        char expected_prefix = (keys[i])[0];
        char actual_prefix = (values[i])[0];
        if (actual_prefix != expected_prefix) {
          fprintf(stderr, "error expected prefix = %c actual = %c\n",
                  expected_prefix, actual_prefix);
        }
        (values[i])[0] = ' '; // blank out the differing character
        thread->stats.addgets(1, 1);
      }
    }
    db_->releasesnapshot(readoptionscopy.snapshot);

    // now that we retrieved all values, check that they all match
    for (int i = 1; i < 10; i++) {
      if (values[i] != values[0]) {
        fprintf(stderr, "error : inconsistent values for key %s: %s, %s\n",
                key.tostring(true).c_str(), stringtohex(values[0]).c_str(),
                stringtohex(values[i]).c_str());
      // we continue after error rather than exiting so that we can
      // find more errors if any
      }
    }

    return s;
  }

  // given a key, this does prefix scans for "0"+p, "1"+p,..."9"+p
  // in the same snapshot where p is the first flags_prefix_size - 1 bytes
  // of the key. each of these 10 scans returns a series of values;
  // each series should be the same length, and it is verified for each
  // index i that all the i'th values are of the form "0"+v, "1"+v,..."9"+v.
  // assumes that multiput was used to put (k, v)
  status multiprefixscan(threadstate* thread, const readoptions& readoptions,
                         columnfamilyhandle* column_family,
                         const slice& key) {
    std::string prefixes[10] = {"0", "1", "2", "3", "4",
                                "5", "6", "7", "8", "9"};
    slice prefix_slices[10];
    readoptions readoptionscopy[10];
    const snapshot* snapshot = db_->getsnapshot();
    iterator* iters[10];
    status s = status::ok();
    for (int i = 0; i < 10; i++) {
      prefixes[i] += key.tostring();
      prefixes[i].resize(flags_prefix_size);
      prefix_slices[i] = slice(prefixes[i]);
      readoptionscopy[i] = readoptions;
      readoptionscopy[i].snapshot = snapshot;
      iters[i] = db_->newiterator(readoptionscopy[i], column_family);
      iters[i]->seek(prefix_slices[i]);
    }

    int count = 0;
    while (iters[0]->valid() && iters[0]->key().starts_with(prefix_slices[0])) {
      count++;
      std::string values[10];
      // get list of all values for this iteration
      for (int i = 0; i < 10; i++) {
        // no iterator should finish before the first one
        assert(iters[i]->valid() &&
               iters[i]->key().starts_with(prefix_slices[i]));
        values[i] = iters[i]->value().tostring();

        char expected_first = (prefixes[i])[0];
        char actual_first = (values[i])[0];

        if (actual_first != expected_first) {
          fprintf(stderr, "error expected first = %c actual = %c\n",
                  expected_first, actual_first);
        }
        (values[i])[0] = ' '; // blank out the differing character
      }
      // make sure all values are equivalent
      for (int i = 0; i < 10; i++) {
        if (values[i] != values[0]) {
          fprintf(stderr, "error : %d, inconsistent values for prefix %s: %s, %s\n",
                  i, prefixes[i].c_str(), stringtohex(values[0]).c_str(),
                  stringtohex(values[i]).c_str());
          // we continue after error rather than exiting so that we can
          // find more errors if any
        }
        iters[i]->next();
      }
    }

    // cleanup iterators and snapshot
    for (int i = 0; i < 10; i++) {
      // if the first iterator finished, they should have all finished
      assert(!iters[i]->valid() ||
             !iters[i]->key().starts_with(prefix_slices[i]));
      assert(iters[i]->status().ok());
      delete iters[i];
    }
    db_->releasesnapshot(snapshot);

    if (s.ok()) {
      thread->stats.addprefixes(1, count);
    } else {
      thread->stats.adderrors(1);
    }

    return s;
  }

  // given a key k, this creates an iterator which scans to k and then
  // does a random sequence of next/prev operations.
  status multiiterate(threadstate* thread, const readoptions& readoptions,
                      columnfamilyhandle* column_family, const slice& key) {
    status s;
    const snapshot* snapshot = db_->getsnapshot();
    readoptions readoptionscopy = readoptions;
    readoptionscopy.snapshot = snapshot;
    unique_ptr<iterator> iter(db_->newiterator(readoptionscopy, column_family));

    iter->seek(key);
    for (uint64_t i = 0; i < flags_num_iterations && iter->valid(); i++) {
      if (thread->rand.onein(2)) {
        iter->next();
      } else {
        iter->prev();
      }
    }

    if (s.ok()) {
      thread->stats.additerations(1);
    } else {
      thread->stats.adderrors(1);
    }

    db_->releasesnapshot(snapshot);

    return s;
  }

  void operatedb(threadstate* thread) {
    readoptions read_opts(flags_verify_checksum, true);
    writeoptions write_opts;
    char value[100];
    long max_key = thread->shared->getmaxkey();
    std::string from_db;
    if (flags_sync) {
      write_opts.sync = true;
    }
    write_opts.disablewal = flags_disable_wal;
    const int prefixbound = (int)flags_readpercent + (int)flags_prefixpercent;
    const int writebound = prefixbound + (int)flags_writepercent;
    const int delbound = writebound + (int)flags_delpercent;

    thread->stats.start();
    for (uint64_t i = 0; i < flags_ops_per_thread; i++) {
      if (thread->shared->hasverificationfailedyet()) {
        break;
      }
      if (i != 0 && (i % (flags_ops_per_thread / (flags_reopen + 1))) == 0) {
        {
          thread->stats.finishedsingleop();
          mutexlock l(thread->shared->getmutex());
          thread->shared->incvotedreopen();
          if (thread->shared->allvotedreopen()) {
            thread->shared->getstresstest()->reopen();
            thread->shared->getcondvar()->signalall();
          }
          else {
            thread->shared->getcondvar()->wait();
          }
          // commenting this out as we don't want to reset stats on each open.
          // thread->stats.start();
        }
      }

      if (!flags_test_batches_snapshots &&
          flags_clear_column_family_one_in != 0 && flags_column_families > 1) {
        if (thread->rand.onein(flags_clear_column_family_one_in)) {
          // drop column family and then create it again (can't drop default)
          int cf = thread->rand.next() % (flags_column_families - 1) + 1;
          std::string new_name =
              std::to_string(new_column_family_name_.fetch_add(1));
          {
            mutexlock l(thread->shared->getmutex());
            fprintf(
                stdout,
                "[cf %d] dropping and recreating column family. new name: %s\n",
                cf, new_name.c_str());
          }
          thread->shared->lockcolumnfamily(cf);
          status s __attribute__((unused));
          s = db_->dropcolumnfamily(column_families_[cf]);
          delete column_families_[cf];
          if (!s.ok()) {
            fprintf(stderr, "dropping column family error: %s\n",
                s.tostring().c_str());
            std::terminate();
          }
          s = db_->createcolumnfamily(columnfamilyoptions(options_), new_name,
                                      &column_families_[cf]);
          column_family_names_[cf] = new_name;
          thread->shared->clearcolumnfamily(cf);
          if (!s.ok()) {
            fprintf(stderr, "creating column family error: %s\n",
                s.tostring().c_str());
            std::terminate();
          }
          thread->shared->unlockcolumnfamily(cf);
        }
      }

      long rand_key = thread->rand.next() % max_key;
      int rand_column_family = thread->rand.next() % flags_column_families;
      std::string keystr = key(rand_key);
      slice key = keystr;
      int prob_op = thread->rand.uniform(100);
      std::unique_ptr<mutexlock> l;
      if (!flags_test_batches_snapshots) {
        l.reset(new mutexlock(
            thread->shared->getmutexforkey(rand_column_family, rand_key)));
      }
      auto column_family = column_families_[rand_column_family];

      if (prob_op >= 0 && prob_op < (int)flags_readpercent) {
        // operation read
        if (!flags_test_batches_snapshots) {
          status s = db_->get(read_opts, column_family, key, &from_db);
          if (s.ok()) {
            // found case
            thread->stats.addgets(1, 1);
          } else if (s.isnotfound()) {
            // not found case
            thread->stats.addgets(1, 0);
          } else {
            // errors case
            thread->stats.adderrors(1);
          }
        } else {
          multiget(thread, read_opts, column_family, key, &from_db);
        }
      } else if ((int)flags_readpercent <= prob_op && prob_op < prefixbound) {
        // operation prefix scan
        // keys are 8 bytes long, prefix size is flags_prefix_size. there are
        // (8 - flags_prefix_size) bytes besides the prefix. so there will
        // be 2 ^ ((8 - flags_prefix_size) * 8) possible keys with the same
        // prefix
        if (!flags_test_batches_snapshots) {
          slice prefix = slice(key.data(), flags_prefix_size);
          iterator* iter = db_->newiterator(read_opts, column_family);
          int64_t count = 0;
          for (iter->seek(prefix);
               iter->valid() && iter->key().starts_with(prefix); iter->next()) {
            ++count;
          }
          assert(count <=
                 (static_cast<int64_t>(1) << ((8 - flags_prefix_size) * 8)));
          if (iter->status().ok()) {
            thread->stats.addprefixes(1, count);
          } else {
            thread->stats.adderrors(1);
          }
          delete iter;
        } else {
          multiprefixscan(thread, read_opts, column_family, key);
        }
      } else if (prefixbound <= prob_op && prob_op < writebound) {
        // operation write
        uint32_t value_base = thread->rand.next();
        size_t sz = generatevalue(value_base, value, sizeof(value));
        slice v(value, sz);
        if (!flags_test_batches_snapshots) {
          if (flags_verify_before_write) {
            std::string keystr2 = key(rand_key);
            slice k = keystr2;
            status s = db_->get(read_opts, column_family, k, &from_db);
            if (verifyvalue(rand_column_family, rand_key, read_opts,
                            thread->shared, from_db, s, true) == false) {
              break;
            }
          }
          thread->shared->put(rand_column_family, rand_key, value_base);
          status s;
          if (flags_use_merge) {
            s = db_->merge(write_opts, column_family, key, v);
          } else {
            s = db_->put(write_opts, column_family, key, v);
          }
          if (!s.ok()) {
            fprintf(stderr, "put or merge error: %s\n", s.tostring().c_str());
            std::terminate();
          }
          thread->stats.addbytesforwrites(1, sz);
        } else {
          multiput(thread, write_opts, column_family, key, v, sz);
        }
        printkeyvalue(rand_column_family, rand_key, value, sz);
      } else if (writebound <= prob_op && prob_op < delbound) {
        // operation delete
        if (!flags_test_batches_snapshots) {
          thread->shared->delete(rand_column_family, rand_key);
          status s = db_->delete(write_opts, column_family, key);
          thread->stats.adddeletes(1);
          if (!s.ok()) {
            fprintf(stderr, "delete error: %s\n", s.tostring().c_str());
            std::terminate();
          }
        } else {
          multidelete(thread, write_opts, column_family, key);
        }
      } else {
        // operation iterate
        multiiterate(thread, read_opts, column_family, key);
      }
      thread->stats.finishedsingleop();
    }

    thread->stats.stop();
  }

  void verifydb(threadstate* thread) const {
    readoptions options(flags_verify_checksum, true);
    auto shared = thread->shared;
    static const long max_key = shared->getmaxkey();
    static const long keys_per_thread = max_key / shared->getnumthreads();
    long start = keys_per_thread * thread->tid;
    long end = start + keys_per_thread;
    if (thread->tid == shared->getnumthreads() - 1) {
      end = max_key;
    }
    for (size_t cf = 0; cf < column_families_.size(); ++cf) {
      if (thread->shared->hasverificationfailedyet()) {
        break;
      }
      if (!thread->rand.onein(2)) {
        // use iterator to verify this range
        unique_ptr<iterator> iter(
            db_->newiterator(options, column_families_[cf]));
        iter->seek(key(start));
        for (long i = start; i < end; i++) {
          if (thread->shared->hasverificationfailedyet()) {
            break;
          }
          // todo(ljin): update "long" to uint64_t
          // reseek when the prefix changes
          if (i % (static_cast<int64_t>(1) << 8 * (8 - flags_prefix_size)) ==
              0) {
            iter->seek(key(i));
          }
          std::string from_db;
          std::string keystr = key(i);
          slice k = keystr;
          status s = iter->status();
          if (iter->valid()) {
            if (iter->key().compare(k) > 0) {
              s = status::notfound(slice());
            } else if (iter->key().compare(k) == 0) {
              from_db = iter->value().tostring();
              iter->next();
            } else if (iter->key().compare(k) < 0) {
              verificationabort(shared, "an out of range key was found", cf, i);
            }
          } else {
            // the iterator found no value for the key in question, so do not
            // move to the next item in the iterator
            s = status::notfound(slice());
          }
          verifyvalue(cf, i, options, shared, from_db, s, true);
          if (from_db.length()) {
            printkeyvalue(cf, i, from_db.data(), from_db.length());
          }
        }
      } else {
        // use get to verify this range
        for (long i = start; i < end; i++) {
          if (thread->shared->hasverificationfailedyet()) {
            break;
          }
          std::string from_db;
          std::string keystr = key(i);
          slice k = keystr;
          status s = db_->get(options, column_families_[cf], k, &from_db);
          verifyvalue(cf, i, options, shared, from_db, s, true);
          if (from_db.length()) {
            printkeyvalue(cf, i, from_db.data(), from_db.length());
          }
        }
      }
    }
  }

  void verificationabort(sharedstate* shared, std::string msg, int cf,
                         long key) const {
    printf("verification failed for column family %d key %ld: %s\n", cf, key,
           msg.c_str());
    shared->setverificationfailure();
  }

  bool verifyvalue(int cf, long key, const readoptions& opts,
                   sharedstate* shared, const std::string& value_from_db,
                   status s, bool strict = false) const {
    if (shared->hasverificationfailedyet()) {
      return false;
    }
    // compare value_from_db with the value in the shared state
    char value[100];
    uint32_t value_base = shared->get(cf, key);
    if (value_base == sharedstate::sentinel && !strict) {
      return true;
    }

    if (s.ok()) {
      if (value_base == sharedstate::sentinel) {
        verificationabort(shared, "unexpected value found", cf, key);
        return false;
      }
      size_t sz = generatevalue(value_base, value, sizeof(value));
      if (value_from_db.length() != sz) {
        verificationabort(shared, "length of value read is not equal", cf, key);
        return false;
      }
      if (memcmp(value_from_db.data(), value, sz) != 0) {
        verificationabort(shared, "contents of value read don't match", cf,
                          key);
        return false;
      }
    } else {
      if (value_base != sharedstate::sentinel) {
        verificationabort(shared, "value not found: " + s.tostring(), cf, key);
        return false;
      }
    }
    return true;
  }

  static void printkeyvalue(int cf, uint32_t key, const char* value,
                            size_t sz) {
    if (!flags_verbose) {
      return;
    }
    fprintf(stdout, "[cf %d] %u ==> (%u) ", cf, key, (unsigned int)sz);
    for (size_t i = 0; i < sz; i++) {
      fprintf(stdout, "%x", value[i]);
    }
    fprintf(stdout, "\n");
  }

  static size_t generatevalue(uint32_t rand, char *v, size_t max_sz) {
    size_t value_sz = ((rand % 3) + 1) * flags_value_size_mult;
    assert(value_sz <= max_sz && value_sz >= sizeof(uint32_t));
    *((uint32_t*)v) = rand;
    for (size_t i=sizeof(uint32_t); i < value_sz; i++) {
      v[i] = (char)(rand ^ i);
    }
    v[value_sz] = '\0';
    return value_sz; // the size of the value set.
  }

  void printenv() const {
    fprintf(stdout, "rocksdb version     : %d.%d\n", kmajorversion,
            kminorversion);
    fprintf(stdout, "column families     : %d\n", flags_column_families);
    if (!flags_test_batches_snapshots) {
      fprintf(stdout, "clear cfs one in    : %d\n",
              flags_clear_column_family_one_in);
    }
    fprintf(stdout, "number of threads   : %d\n", flags_threads);
    fprintf(stdout,
            "ops per thread      : %lu\n",
            (unsigned long)flags_ops_per_thread);
    std::string ttl_state("unused");
    if (flags_ttl > 0) {
      ttl_state = numbertostring(flags_ttl);
    }
    fprintf(stdout, "time to live(sec)   : %s\n", ttl_state.c_str());
    fprintf(stdout, "read percentage     : %d%%\n", flags_readpercent);
    fprintf(stdout, "prefix percentage   : %d%%\n", flags_prefixpercent);
    fprintf(stdout, "write percentage    : %d%%\n", flags_writepercent);
    fprintf(stdout, "delete percentage   : %d%%\n", flags_delpercent);
    fprintf(stdout, "iterate percentage  : %d%%\n", flags_iterpercent);
    fprintf(stdout, "write-buffer-size   : %d\n", flags_write_buffer_size);
    fprintf(stdout,
            "iterations          : %lu\n",
            (unsigned long)flags_num_iterations);
    fprintf(stdout,
            "max key             : %lu\n",
            (unsigned long)flags_max_key);
    fprintf(stdout, "ratio #ops/#keys    : %f\n",
            (1.0 * flags_ops_per_thread * flags_threads)/flags_max_key);
    fprintf(stdout, "num times db reopens: %d\n", flags_reopen);
    fprintf(stdout, "batches/snapshots   : %d\n",
            flags_test_batches_snapshots);
    fprintf(stdout, "purge redundant %%   : %d\n",
            flags_purge_redundant_percent);
    fprintf(stdout, "deletes use filter  : %d\n",
            flags_filter_deletes);
    fprintf(stdout, "num keys per lock   : %d\n",
            1 << flags_log2_keys_per_lock);

    const char* compression = "";
    switch (flags_compression_type_e) {
      case rocksdb::knocompression:
        compression = "none";
        break;
      case rocksdb::ksnappycompression:
        compression = "snappy";
        break;
      case rocksdb::kzlibcompression:
        compression = "zlib";
        break;
      case rocksdb::kbzip2compression:
        compression = "bzip2";
        break;
      case rocksdb::klz4compression:
        compression = "lz4";
      case rocksdb::klz4hccompression:
        compression = "lz4hc";
        break;
      }

    fprintf(stdout, "compression         : %s\n", compression);

    const char* memtablerep = "";
    switch (flags_rep_factory) {
      case kskiplist:
        memtablerep = "skip_list";
        break;
      case khashskiplist:
        memtablerep = "prefix_hash";
        break;
      case kvectorrep:
        memtablerep = "vector";
        break;
    }

    fprintf(stdout, "memtablerep         : %s\n", memtablerep);

    fprintf(stdout, "------------------------------------------------\n");
  }

  void open() {
    assert(db_ == nullptr);
    blockbasedtableoptions block_based_options;
    block_based_options.block_cache = cache_;
    block_based_options.block_cache_compressed = compressed_cache_;
    block_based_options.block_size = flags_block_size;
    block_based_options.filter_policy = filter_policy_;
    options_.table_factory.reset(
        newblockbasedtablefactory(block_based_options));
    options_.write_buffer_size = flags_write_buffer_size;
    options_.max_write_buffer_number = flags_max_write_buffer_number;
    options_.min_write_buffer_number_to_merge =
        flags_min_write_buffer_number_to_merge;
    options_.max_background_compactions = flags_max_background_compactions;
    options_.max_background_flushes = flags_max_background_flushes;
    options_.compaction_style =
        static_cast<rocksdb::compactionstyle>(flags_compaction_style);
    options_.prefix_extractor.reset(newfixedprefixtransform(flags_prefix_size));
    options_.max_open_files = flags_open_files;
    options_.statistics = dbstats;
    options_.env = flags_env;
    options_.disabledatasync = flags_disable_data_sync;
    options_.use_fsync = flags_use_fsync;
    options_.allow_mmap_reads = flags_mmap_read;
    rocksdb_kill_odds = flags_kill_random_test;
    options_.target_file_size_base = flags_target_file_size_base;
    options_.target_file_size_multiplier = flags_target_file_size_multiplier;
    options_.max_bytes_for_level_base = flags_max_bytes_for_level_base;
    options_.max_bytes_for_level_multiplier =
        flags_max_bytes_for_level_multiplier;
    options_.level0_stop_writes_trigger = flags_level0_stop_writes_trigger;
    options_.level0_slowdown_writes_trigger =
        flags_level0_slowdown_writes_trigger;
    options_.level0_file_num_compaction_trigger =
        flags_level0_file_num_compaction_trigger;
    options_.compression = flags_compression_type_e;
    options_.create_if_missing = true;
    options_.max_manifest_file_size = 10 * 1024;
    options_.filter_deletes = flags_filter_deletes;
    if ((flags_prefix_size == 0) == (flags_rep_factory == khashskiplist)) {
      fprintf(stderr,
            "prefix_size should be non-zero iff memtablerep == prefix_hash\n");
      exit(1);
    }
    switch (flags_rep_factory) {
      case khashskiplist:
        options_.memtable_factory.reset(newhashskiplistrepfactory(10000));
        break;
      case kskiplist:
        // no need to do anything
        break;
      case kvectorrep:
        options_.memtable_factory.reset(new vectorrepfactory());
        break;
    }
    static random purge_percent(1000); // no benefit from non-determinism here
    if (static_cast<int32_t>(purge_percent.uniform(100)) <
        flags_purge_redundant_percent - 1) {
      options_.purge_redundant_kvs_while_flush = false;
    }

    if (flags_use_merge) {
      options_.merge_operator = mergeoperators::createputoperator();
    }

    // set universal style compaction configurations, if applicable
    if (flags_universal_size_ratio != 0) {
      options_.compaction_options_universal.size_ratio =
          flags_universal_size_ratio;
    }
    if (flags_universal_min_merge_width != 0) {
      options_.compaction_options_universal.min_merge_width =
          flags_universal_min_merge_width;
    }
    if (flags_universal_max_merge_width != 0) {
      options_.compaction_options_universal.max_merge_width =
          flags_universal_max_merge_width;
    }
    if (flags_universal_max_size_amplification_percent != 0) {
      options_.compaction_options_universal.max_size_amplification_percent =
          flags_universal_max_size_amplification_percent;
    }

    fprintf(stdout, "db path: [%s]\n", flags_db.c_str());

    status s;
    if (flags_ttl == -1) {
      std::vector<std::string> existing_column_families;
      s = db::listcolumnfamilies(dboptions(options_), flags_db,
                                 &existing_column_families);  // ignore errors
      if (!s.ok()) {
        // db doesn't exist
        assert(existing_column_families.empty());
        assert(column_family_names_.empty());
        column_family_names_.push_back(kdefaultcolumnfamilyname);
      } else if (column_family_names_.empty()) {
        // this is the first call to the function open()
        column_family_names_ = existing_column_families;
      } else {
        // this is a reopen. just assert that existing column_family_names are
        // equivalent to what we remember
        auto sorted_cfn = column_family_names_;
        sort(sorted_cfn.begin(), sorted_cfn.end());
        sort(existing_column_families.begin(), existing_column_families.end());
        if (sorted_cfn != existing_column_families) {
          fprintf(stderr,
                  "expected column families differ from the existing:\n");
          printf("expected: {");
          for (auto cf : sorted_cfn) {
            printf("%s ", cf.c_str());
          }
          printf("}\n");
          printf("existing: {");
          for (auto cf : existing_column_families) {
            printf("%s ", cf.c_str());
          }
          printf("}\n");
        }
        assert(sorted_cfn == existing_column_families);
      }
      std::vector<columnfamilydescriptor> cf_descriptors;
      for (auto name : column_family_names_) {
        if (name != kdefaultcolumnfamilyname) {
          new_column_family_name_ =
              std::max(new_column_family_name_.load(), std::stoi(name) + 1);
        }
        cf_descriptors.emplace_back(name, columnfamilyoptions(options_));
      }
      while (cf_descriptors.size() < (size_t)flags_column_families) {
        std::string name = std::to_string(new_column_family_name_.load());
        new_column_family_name_++;
        cf_descriptors.emplace_back(name, columnfamilyoptions(options_));
        column_family_names_.push_back(name);
      }
      options_.create_missing_column_families = true;
      s = db::open(dboptions(options_), flags_db, cf_descriptors,
                   &column_families_, &db_);
      assert(!s.ok() || column_families_.size() ==
                            static_cast<size_t>(flags_column_families));
    } else {
      dbwithttl* db_with_ttl;
      s = dbwithttl::open(options_, flags_db, &db_with_ttl, flags_ttl);
      db_ = db_with_ttl;
    }
    if (!s.ok()) {
      fprintf(stderr, "open error: %s\n", s.tostring().c_str());
      exit(1);
    }
  }

  void reopen() {
    for (auto cf : column_families_) {
      delete cf;
    }
    column_families_.clear();
    delete db_;
    db_ = nullptr;

    num_times_reopened_++;
    double now = flags_env->nowmicros();
    fprintf(stdout, "%s reopening database for the %dth time\n",
            flags_env->timetostring((uint64_t) now/1000000).c_str(),
            num_times_reopened_);
    open();
  }

  void printstatistics() {
    if (dbstats) {
      fprintf(stdout, "statistics:\n%s\n", dbstats->tostring().c_str());
    }
  }

 private:
  std::shared_ptr<cache> cache_;
  std::shared_ptr<cache> compressed_cache_;
  std::shared_ptr<const filterpolicy> filter_policy_;
  db* db_;
  options options_;
  std::vector<columnfamilyhandle*> column_families_;
  std::vector<std::string> column_family_names_;
  std::atomic<int> new_column_family_name_;
  int num_times_reopened_;
};

}  // namespace rocksdb

int main(int argc, char** argv) {
  setusagemessage(std::string("\nusage:\n") + std::string(argv[0]) +
                  " [options]...");
  parsecommandlineflags(&argc, &argv, true);

  if (flags_statistics) {
    dbstats = rocksdb::createdbstatistics();
  }
  flags_compression_type_e =
    stringtocompressiontype(flags_compression_type.c_str());
  if (!flags_hdfs.empty()) {
    flags_env  = new rocksdb::hdfsenv(flags_hdfs);
  }
  flags_rep_factory = stringtorepfactory(flags_memtablerep.c_str());

  // the number of background threads should be at least as much the
  // max number of concurrent compactions.
  flags_env->setbackgroundthreads(flags_max_background_compactions);

  if (flags_prefixpercent > 0 && flags_prefix_size <= 0) {
    fprintf(stderr,
            "error: prefixpercent is non-zero while prefix_size is "
            "not positive!\n");
    exit(1);
  }
  if (flags_test_batches_snapshots && flags_prefix_size <= 0) {
    fprintf(stderr,
            "error: please specify prefix_size for "
            "test_batches_snapshots test!\n");
    exit(1);
  }
  if ((flags_readpercent + flags_prefixpercent +
       flags_writepercent + flags_delpercent + flags_iterpercent) != 100) {
      fprintf(stderr,
              "error: read+prefix+write+delete+iterate percents != 100!\n");
      exit(1);
  }
  if (flags_disable_wal == 1 && flags_reopen > 0) {
      fprintf(stderr, "error: db cannot reopen safely with disable_wal set!\n");
      exit(1);
  }
  if ((unsigned)flags_reopen >= flags_ops_per_thread) {
      fprintf(stderr,
              "error: #db-reopens should be < ops_per_thread\n"
              "provided reopens = %d and ops_per_thread = %lu\n",
              flags_reopen,
              (unsigned long)flags_ops_per_thread);
      exit(1);
  }

  // choose a location for the test database if none given with --db=<path>
  if (flags_db.empty()) {
      std::string default_db_path;
      rocksdb::env::default()->gettestdirectory(&default_db_path);
      default_db_path += "/dbstress";
      flags_db = default_db_path;
  }

  rocksdb::stresstest stress;
  if (stress.run()) {
    return 0;
  } else {
    return 1;
  }
}

#endif  // gflags
