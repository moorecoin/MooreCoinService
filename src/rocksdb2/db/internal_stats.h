//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.
//

#pragma once
#include "db/version_set.h"

#include <vector>
#include <string>

class columnfamilydata;

namespace rocksdb {

class memtablelist;
class dbimpl;

enum dbpropertytype : uint32_t {
  kunknown,
  knumfilesatlevel,  // number of files at a specific level
  klevelstats,       // return number of files and total sizes of each level
  kcfstats,          // return general statitistics of cf
  kdbstats,          // return general statitistics of db
  kstats,            // return general statitistics of both db and cf
  ksstables,         // return a human readable string of current sst files
  kstartinttypes,    // ---- dummy value to indicate the start of integer values
  knumimmutablememtable,   // return number of immutable mem tables
  kmemtableflushpending,   // return 1 if mem table flushing is pending,
                           // otherwise 0.
  kcompactionpending,      // return 1 if a compaction is pending. otherwise 0.
  kbackgrounderrors,       // return accumulated background errors encountered.
  kcursizeactivememtable,  // return current size of the active memtable
  knumentriesinmutablememtable,    // return number of entries in the mutable
                                   // memtable.
  knumentriesinimmutablememtable,  // return sum of number of entries in all
                                   // the immutable mem tables.
  kestimatednumkeys,  // estimated total number of keys in the database.
  kestimatedusagebytablereaders,  // estimated memory by table readers.
  kisfiledeletionenabled,         // equals disable_delete_obsolete_files_,
                                  // 0 means file deletions enabled
};

extern dbpropertytype getpropertytype(const slice& property,
                                      bool* is_int_property,
                                      bool* need_out_of_mutex);

class internalstats {
 public:
  enum internalcfstatstype {
    level0_slowdown,
    memtable_compaction,
    level0_num_files,
    write_stalls_enum_max,
    bytes_flushed,
    internal_cf_stats_enum_max,
  };

  enum internaldbstatstype {
    wal_file_bytes,
    wal_file_synced,
    bytes_written,
    write_done_by_other,
    write_done_by_self,
    write_with_wal,
    internal_db_stats_enum_max,
  };

  internalstats(int num_levels, env* env, columnfamilydata* cfd)
      : db_stats_(internal_db_stats_enum_max),
        cf_stats_value_(internal_cf_stats_enum_max),
        cf_stats_count_(internal_cf_stats_enum_max),
        comp_stats_(num_levels),
        stall_leveln_slowdown_hard_(num_levels),
        stall_leveln_slowdown_count_hard_(num_levels),
        stall_leveln_slowdown_soft_(num_levels),
        stall_leveln_slowdown_count_soft_(num_levels),
        bg_error_count_(0),
        number_levels_(num_levels),
        env_(env),
        cfd_(cfd),
        started_at_(env->nowmicros()) {
    for (int i = 0; i< internal_db_stats_enum_max; ++i) {
      db_stats_[i] = 0;
    }
    for (int i = 0; i< internal_cf_stats_enum_max; ++i) {
      cf_stats_value_[i] = 0;
      cf_stats_count_[i] = 0;
    }
    for (int i = 0; i < num_levels; ++i) {
      stall_leveln_slowdown_hard_[i] = 0;
      stall_leveln_slowdown_count_hard_[i] = 0;
      stall_leveln_slowdown_soft_[i] = 0;
      stall_leveln_slowdown_count_soft_[i] = 0;
    }
  }

  // per level compaction stats.  comp_stats_[level] stores the stats for
  // compactions that produced data for the specified "level".
  struct compactionstats {
    uint64_t micros;

    // bytes read from level n during compaction between levels n and n+1
    uint64_t bytes_readn;

    // bytes read from level n+1 during compaction between levels n and n+1
    uint64_t bytes_readnp1;

    // total bytes written during compaction between levels n and n+1
    uint64_t bytes_written;

    // files read from level n during compaction between levels n and n+1
    int files_in_leveln;

    // files read from level n+1 during compaction between levels n and n+1
    int files_in_levelnp1;

    // files written during compaction between levels n and n+1
    int files_out_levelnp1;

    // number of compactions done
    int count;

    explicit compactionstats(int count = 0)
        : micros(0),
          bytes_readn(0),
          bytes_readnp1(0),
          bytes_written(0),
          files_in_leveln(0),
          files_in_levelnp1(0),
          files_out_levelnp1(0),
          count(count) {}

    explicit compactionstats(const compactionstats& c)
        : micros(c.micros),
          bytes_readn(c.bytes_readn),
          bytes_readnp1(c.bytes_readnp1),
          bytes_written(c.bytes_written),
          files_in_leveln(c.files_in_leveln),
          files_in_levelnp1(c.files_in_levelnp1),
          files_out_levelnp1(c.files_out_levelnp1),
          count(c.count) {}

    void add(const compactionstats& c) {
      this->micros += c.micros;
      this->bytes_readn += c.bytes_readn;
      this->bytes_readnp1 += c.bytes_readnp1;
      this->bytes_written += c.bytes_written;
      this->files_in_leveln += c.files_in_leveln;
      this->files_in_levelnp1 += c.files_in_levelnp1;
      this->files_out_levelnp1 += c.files_out_levelnp1;
      this->count += c.count;
    }

    void subtract(const compactionstats& c) {
      this->micros -= c.micros;
      this->bytes_readn -= c.bytes_readn;
      this->bytes_readnp1 -= c.bytes_readnp1;
      this->bytes_written -= c.bytes_written;
      this->files_in_leveln -= c.files_in_leveln;
      this->files_in_levelnp1 -= c.files_in_levelnp1;
      this->files_out_levelnp1 -= c.files_out_levelnp1;
      this->count -= c.count;
    }
  };

  void addcompactionstats(int level, const compactionstats& stats) {
    comp_stats_[level].add(stats);
  }

  void recordlevelnslowdown(int level, uint64_t micros, bool soft) {
    if (soft) {
      stall_leveln_slowdown_soft_[level] += micros;
      ++stall_leveln_slowdown_count_soft_[level];
    } else {
      stall_leveln_slowdown_hard_[level] += micros;
      ++stall_leveln_slowdown_count_hard_[level];
    }
  }

  void addcfstats(internalcfstatstype type, uint64_t value) {
    cf_stats_value_[type] += value;
    ++cf_stats_count_[type];
  }

  void adddbstats(internaldbstatstype type, uint64_t value) {
    db_stats_[type] += value;
  }

  uint64_t getbackgrounderrorcount() const { return bg_error_count_; }

  uint64_t bumpandgetbackgrounderrorcount() { return ++bg_error_count_; }

  bool getstringproperty(dbpropertytype property_type, const slice& property,
                         std::string* value);

  bool getintproperty(dbpropertytype property_type, uint64_t* value,
                      dbimpl* db) const;

  bool getintpropertyoutofmutex(dbpropertytype property_type, version* version,
                                uint64_t* value) const;

 private:
  void dumpdbstats(std::string* value);
  void dumpcfstats(std::string* value);

  // per-db stats
  std::vector<uint64_t> db_stats_;
  // per-columnfamily stats
  std::vector<uint64_t> cf_stats_value_;
  std::vector<uint64_t> cf_stats_count_;
  // per-columnfamily/level compaction stats
  std::vector<compactionstats> comp_stats_;
  // these count the number of microseconds for which makeroomforwrite stalls.
  std::vector<uint64_t> stall_leveln_slowdown_hard_;
  std::vector<uint64_t> stall_leveln_slowdown_count_hard_;
  std::vector<uint64_t> stall_leveln_slowdown_soft_;
  std::vector<uint64_t> stall_leveln_slowdown_count_soft_;

  // used to compute per-interval statistics
  struct cfstatssnapshot {
    // columnfamily-level stats
    compactionstats comp_stats;
    uint64_t ingest_bytes;            // bytes written to l0
    uint64_t stall_us;                // stall time in micro-seconds
    uint64_t stall_count;             // stall count

    cfstatssnapshot()
        : comp_stats(0),
          ingest_bytes(0),
          stall_us(0),
          stall_count(0) {}
  } cf_stats_snapshot_;

  struct dbstatssnapshot {
    // db-level stats
    uint64_t ingest_bytes;            // bytes written by user
    uint64_t wal_bytes;               // bytes written to wal
    uint64_t wal_synced;              // number of times wal is synced
    uint64_t write_with_wal;          // number of writes that request wal
    // these count the number of writes processed by the calling thread or
    // another thread.
    uint64_t write_other;
    uint64_t write_self;
    double seconds_up;

    dbstatssnapshot()
        : ingest_bytes(0),
          wal_bytes(0),
          wal_synced(0),
          write_with_wal(0),
          write_other(0),
          write_self(0),
          seconds_up(0) {}
  } db_stats_snapshot_;

  // total number of background errors encountered. every time a flush task
  // or compaction task fails, this counter is incremented. the failure can
  // be caused by any possible reason, including file system errors, out of
  // resources, or input file corruption. failing when retrying the same flush
  // or compaction will cause the counter to increase too.
  uint64_t bg_error_count_;

  const int number_levels_;
  env* env_;
  columnfamilydata* cfd_;
  const uint64_t started_at_;
};

}  // namespace rocksdb
