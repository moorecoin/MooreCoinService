//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "db/internal_stats.h"
#define __stdc_format_macros
#include <inttypes.h>
#include <vector>
#include "db/column_family.h"
#include "db/db_impl.h"

namespace rocksdb {

namespace {
const double kmb = 1048576.0;
const double kgb = kmb * 1024;

void printlevelstatsheader(char* buf, size_t len, const std::string& cf_name) {
  snprintf(
      buf, len,
      "\n** compaction stats [%s] **\n"
      "level   files   size(mb) score read(gb)  rn(gb) rnp1(gb) "
      "write(gb) wnew(gb) rw-amp w-amp rd(mb/s) wr(mb/s)  rn(cnt) "
      "rnp1(cnt) wnp1(cnt) wnew(cnt)  comp(sec) comp(cnt) avg(sec) "
      "stall(sec) stall(cnt) avg(ms)\n"
      "--------------------------------------------------------------------"
      "--------------------------------------------------------------------"
      "--------------------------------------------------------------------\n",
      cf_name.c_str());
}

void printlevelstats(char* buf, size_t len, const std::string& name,
    int num_files, int being_compacted, double total_file_size, double score,
    double rw_amp, double w_amp, double stall_us, uint64_t stalls,
    const internalstats::compactionstats& stats) {
  uint64_t bytes_read = stats.bytes_readn + stats.bytes_readnp1;
  uint64_t bytes_new = stats.bytes_written - stats.bytes_readnp1;
  double elapsed = (stats.micros + 1) / 1000000.0;

  snprintf(buf, len,
    "%4s %5d/%-3d %8.0f %5.1f " /* level, files, size(mb), score */
    "%8.1f " /* read(gb) */
    "%7.1f " /* rn(gb) */
    "%8.1f " /* rnp1(gb) */
    "%9.1f " /* write(gb) */
    "%8.1f " /* wnew(gb) */
    "%6.1f " /* rw-amp */
    "%5.1f " /* w-amp */
    "%8.1f " /* rd(mb/s) */
    "%8.1f " /* wr(mb/s) */
    "%8d " /* rn(cnt) */
    "%9d " /* rnp1(cnt) */
    "%9d " /* wnp1(cnt) */
    "%9d " /* wnew(cnt) */
    "%10.0f " /* comp(sec) */
    "%9d " /* comp(cnt) */
    "%8.3f " /* avg(sec) */
    "%10.2f " /* stall(sec) */
    "%10" priu64 " " /* stall(cnt) */
    "%7.2f\n" /* avg(ms) */,
    name.c_str(), num_files, being_compacted, total_file_size / kmb, score,
    bytes_read / kgb,
    stats.bytes_readn / kgb,
    stats.bytes_readnp1 / kgb,
    stats.bytes_written / kgb,
    bytes_new / kgb,
    rw_amp,
    w_amp,
    bytes_read / kmb / elapsed,
    stats.bytes_written / kmb / elapsed,
    stats.files_in_leveln,
    stats.files_in_levelnp1,
    stats.files_out_levelnp1,
    stats.files_out_levelnp1 - stats.files_in_levelnp1,
    stats.micros / 1000000.0,
    stats.count,
    stats.count == 0 ? 0 : stats.micros / 1000000.0 / stats.count,
    stall_us / 1000000.0,
    stalls,
    stalls == 0 ? 0 : stall_us / 1000.0 / stalls);
}


}

dbpropertytype getpropertytype(const slice& property, bool* is_int_property,
                               bool* need_out_of_mutex) {
  assert(is_int_property != nullptr);
  assert(need_out_of_mutex != nullptr);
  slice in = property;
  slice prefix("rocksdb.");
  *need_out_of_mutex = false;
  *is_int_property = false;
  if (!in.starts_with(prefix)) {
    return kunknown;
  }
  in.remove_prefix(prefix.size());

  if (in.starts_with("num-files-at-level")) {
    return knumfilesatlevel;
  } else if (in == "levelstats") {
    return klevelstats;
  } else if (in == "stats") {
    return kstats;
  } else if (in == "cfstats") {
    return kcfstats;
  } else if (in == "dbstats") {
    return kdbstats;
  } else if (in == "sstables") {
    return ksstables;
  }

  *is_int_property = true;
  if (in == "num-immutable-mem-table") {
    return knumimmutablememtable;
  } else if (in == "mem-table-flush-pending") {
    return kmemtableflushpending;
  } else if (in == "compaction-pending") {
    return kcompactionpending;
  } else if (in == "background-errors") {
    return kbackgrounderrors;
  } else if (in == "cur-size-active-mem-table") {
    return kcursizeactivememtable;
  } else if (in == "num-entries-active-mem-table") {
    return knumentriesinmutablememtable;
  } else if (in == "num-entries-imm-mem-tables") {
    return knumentriesinimmutablememtable;
  } else if (in == "estimate-num-keys") {
    return kestimatednumkeys;
  } else if (in == "estimate-table-readers-mem") {
    *need_out_of_mutex = true;
    return kestimatedusagebytablereaders;
  } else if (in == "is-file-deletions-enabled") {
    return kisfiledeletionenabled;
  }
  return kunknown;
}

bool internalstats::getintpropertyoutofmutex(dbpropertytype property_type,
                                             version* version,
                                             uint64_t* value) const {
  assert(value != nullptr);
  if (property_type != kestimatedusagebytablereaders) {
    return false;
  }
  if (version == nullptr) {
    *value = 0;
  } else {
    *value = version->getmemoryusagebytablereaders();
  }
  return true;
}

bool internalstats::getstringproperty(dbpropertytype property_type,
                                      const slice& property,
                                      std::string* value) {
  assert(value != nullptr);
  version* current = cfd_->current();
  slice in = property;

  switch (property_type) {
    case knumfilesatlevel: {
      in.remove_prefix(strlen("rocksdb.num-files-at-level"));
      uint64_t level;
      bool ok = consumedecimalnumber(&in, &level) && in.empty();
      if (!ok || (int)level >= number_levels_) {
        return false;
      } else {
        char buf[100];
        snprintf(buf, sizeof(buf), "%d",
                 current->numlevelfiles(static_cast<int>(level)));
        *value = buf;
        return true;
      }
    }
    case klevelstats: {
      char buf[1000];
      snprintf(buf, sizeof(buf),
               "level files size(mb)\n"
               "--------------------\n");
      value->append(buf);

      for (int level = 0; level < number_levels_; level++) {
        snprintf(buf, sizeof(buf), "%3d %8d %8.0f\n", level,
                 current->numlevelfiles(level),
                 current->numlevelbytes(level) / kmb);
        value->append(buf);
      }
      return true;
    }
    case kstats: {
      if (!getstringproperty(kcfstats, "rocksdb.cfstats", value)) {
        return false;
      }
      if (!getstringproperty(kdbstats, "rocksdb.dbstats", value)) {
        return false;
      }
      return true;
    }
    case kcfstats: {
      dumpcfstats(value);
      return true;
    }
    case kdbstats: {
      dumpdbstats(value);
      return true;
    }
    case ksstables:
      *value = current->debugstring();
      return true;
    default:
      return false;
  }
}

bool internalstats::getintproperty(dbpropertytype property_type,
                                   uint64_t* value, dbimpl* db) const {
  version* current = cfd_->current();

  switch (property_type) {
    case knumimmutablememtable:
      *value = cfd_->imm()->size();
      return true;
    case kmemtableflushpending:
      // return number of mem tables that are ready to flush (made immutable)
      *value = (cfd_->imm()->isflushpending() ? 1 : 0);
      return true;
    case kcompactionpending:
      // 1 if the system already determines at least one compacdtion is needed.
      // 0 otherwise,
      *value = (current->needscompaction() ? 1 : 0);
      return true;
    case kbackgrounderrors:
      // accumulated number of  errors in background flushes or compactions.
      *value = getbackgrounderrorcount();
      return true;
    case kcursizeactivememtable:
      // current size of the active memtable
      *value = cfd_->mem()->approximatememoryusage();
      return true;
    case knumentriesinmutablememtable:
      // current size of the active memtable
      *value = cfd_->mem()->getnumentries();
      return true;
    case knumentriesinimmutablememtable:
      // current size of the active memtable
      *value = cfd_->imm()->current()->gettotalnumentries();
      return true;
    case kestimatednumkeys:
      // estimate number of entries in the column family:
      // use estimated entries in tables + total entries in memtables.
      *value = cfd_->mem()->getnumentries() +
               cfd_->imm()->current()->gettotalnumentries() +
               current->getestimatedactivekeys();
      return true;
#ifndef rocksdb_lite
    case kisfiledeletionenabled:
      *value = db->isfiledeletionsenabled();
      return true;
#endif
    default:
      return false;
  }
}

void internalstats::dumpdbstats(std::string* value) {
  char buf[1000];
  // db-level stats, only available from default column family
  double seconds_up = (env_->nowmicros() - started_at_ + 1) / 1000000.0;
  double interval_seconds_up = seconds_up - db_stats_snapshot_.seconds_up;
  snprintf(buf, sizeof(buf),
           "\n** db stats **\nuptime(secs): %.1f total, %.1f interval\n",
           seconds_up, interval_seconds_up);
  value->append(buf);
  // cumulative
  uint64_t user_bytes_written = db_stats_[internalstats::bytes_written];
  uint64_t write_other = db_stats_[internalstats::write_done_by_other];
  uint64_t write_self = db_stats_[internalstats::write_done_by_self];
  uint64_t wal_bytes = db_stats_[internalstats::wal_file_bytes];
  uint64_t wal_synced = db_stats_[internalstats::wal_file_synced];
  uint64_t write_with_wal = db_stats_[internalstats::write_with_wal];
  // data
  snprintf(buf, sizeof(buf),
           "cumulative writes: %" priu64 " writes, %" priu64 " batches, "
           "%.1f writes per batch, %.2f gb user ingest\n",
           write_other + write_self, write_self,
           (write_other + write_self) / static_cast<double>(write_self + 1),
           user_bytes_written / kgb);
  value->append(buf);
  // wal
  snprintf(buf, sizeof(buf),
           "cumulative wal: %" priu64 " writes, %" priu64 " syncs, "
           "%.2f writes per sync, %.2f gb written\n",
           write_with_wal, wal_synced,
           write_with_wal / static_cast<double>(wal_synced + 1),
           wal_bytes / kgb);
  value->append(buf);

  // interval
  uint64_t interval_write_other = write_other - db_stats_snapshot_.write_other;
  uint64_t interval_write_self = write_self - db_stats_snapshot_.write_self;
  snprintf(buf, sizeof(buf),
           "interval writes: %" priu64 " writes, %" priu64 " batches, "
           "%.1f writes per batch, %.1f mb user ingest\n",
           interval_write_other + interval_write_self,
           interval_write_self,
           static_cast<double>(interval_write_other + interval_write_self) /
               (interval_write_self + 1),
           (user_bytes_written - db_stats_snapshot_.ingest_bytes) / kmb);
  value->append(buf);

  uint64_t interval_write_with_wal =
      write_with_wal - db_stats_snapshot_.write_with_wal;
  uint64_t interval_wal_synced = wal_synced - db_stats_snapshot_.wal_synced;
  uint64_t interval_wal_bytes = wal_bytes - db_stats_snapshot_.wal_bytes;

  snprintf(buf, sizeof(buf),
           "interval wal: %" priu64 " writes, %" priu64 " syncs, "
           "%.2f writes per sync, %.2f mb written\n",
           interval_write_with_wal,
           interval_wal_synced,
           interval_write_with_wal /
              static_cast<double>(interval_wal_synced + 1),
           interval_wal_bytes / kgb);
  value->append(buf);

  db_stats_snapshot_.seconds_up = seconds_up;
  db_stats_snapshot_.ingest_bytes = user_bytes_written;
  db_stats_snapshot_.write_other = write_other;
  db_stats_snapshot_.write_self = write_self;
  db_stats_snapshot_.wal_bytes = wal_bytes;
  db_stats_snapshot_.wal_synced = wal_synced;
  db_stats_snapshot_.write_with_wal = write_with_wal;
}

void internalstats::dumpcfstats(std::string* value) {
  version* current = cfd_->current();

  int num_levels_to_check =
      (cfd_->options()->compaction_style != kcompactionstyleuniversal &&
       cfd_->options()->compaction_style != kcompactionstylefifo)
          ? current->numberlevels() - 1
          : 1;
  // compaction scores are sorted base on its value. restore them to the
  // level order
  std::vector<double> compaction_score(number_levels_, 0);
  for (int i = 0; i < num_levels_to_check; ++i) {
    compaction_score[current->compaction_level_[i]] =
      current->compaction_score_[i];
  }
  // count # of files being compacted for each level
  std::vector<int> files_being_compacted(number_levels_, 0);
  for (int level = 0; level < num_levels_to_check; ++level) {
    for (auto* f : current->files_[level]) {
      if (f->being_compacted) {
        ++files_being_compacted[level];
      }
    }
  }

  char buf[1000];
  // per-columnfamily stats
  printlevelstatsheader(buf, sizeof(buf), cfd_->getname());
  value->append(buf);

  compactionstats stats_sum(0);
  int total_files = 0;
  int total_files_being_compacted = 0;
  double total_file_size = 0;
  uint64_t total_slowdown_soft = 0;
  uint64_t total_slowdown_count_soft = 0;
  uint64_t total_slowdown_hard = 0;
  uint64_t total_slowdown_count_hard = 0;
  uint64_t total_stall_count = 0;
  double total_stall_us = 0;
  for (int level = 0; level < number_levels_; level++) {
    int files = current->numlevelfiles(level);
    total_files += files;
    total_files_being_compacted += files_being_compacted[level];
    if (comp_stats_[level].micros > 0 || files > 0) {
      uint64_t stalls = level == 0 ?
        (cf_stats_count_[level0_slowdown] +
         cf_stats_count_[level0_num_files] +
         cf_stats_count_[memtable_compaction])
        : (stall_leveln_slowdown_count_soft_[level] +
           stall_leveln_slowdown_count_hard_[level]);

      double stall_us = level == 0 ?
         (cf_stats_value_[level0_slowdown] +
          cf_stats_value_[level0_num_files] +
          cf_stats_value_[memtable_compaction])
         : (stall_leveln_slowdown_soft_[level] +
            stall_leveln_slowdown_hard_[level]);

      stats_sum.add(comp_stats_[level]);
      total_file_size += current->numlevelbytes(level);
      total_stall_us += stall_us;
      total_stall_count += stalls;
      total_slowdown_soft += stall_leveln_slowdown_soft_[level];
      total_slowdown_count_soft += stall_leveln_slowdown_count_soft_[level];
      total_slowdown_hard += stall_leveln_slowdown_hard_[level];
      total_slowdown_count_hard += stall_leveln_slowdown_count_hard_[level];
      int64_t bytes_read = comp_stats_[level].bytes_readn +
                           comp_stats_[level].bytes_readnp1;
      double rw_amp = (comp_stats_[level].bytes_readn == 0) ? 0.0
          : (comp_stats_[level].bytes_written + bytes_read) /
            static_cast<double>(comp_stats_[level].bytes_readn);
      double w_amp = (comp_stats_[level].bytes_readn == 0) ? 0.0
          : comp_stats_[level].bytes_written /
            static_cast<double>(comp_stats_[level].bytes_readn);
      printlevelstats(buf, sizeof(buf), "l" + std::to_string(level),
          files, files_being_compacted[level], current->numlevelbytes(level),
          compaction_score[level], rw_amp, w_amp, stall_us, stalls,
          comp_stats_[level]);
      value->append(buf);
    }
  }
  uint64_t curr_ingest = cf_stats_value_[bytes_flushed];
  // cumulative summary
  double rw_amp = (stats_sum.bytes_written + stats_sum.bytes_readn +
      stats_sum.bytes_readnp1) / static_cast<double>(curr_ingest + 1);
  double w_amp = stats_sum.bytes_written / static_cast<double>(curr_ingest + 1);
  // stats summary across levels
  printlevelstats(buf, sizeof(buf), "sum", total_files,
      total_files_being_compacted, total_file_size, 0, rw_amp, w_amp,
      total_stall_us, total_stall_count, stats_sum);
  value->append(buf);
  // interval summary
  uint64_t interval_ingest =
      curr_ingest - cf_stats_snapshot_.ingest_bytes + 1;
  compactionstats interval_stats(stats_sum);
  interval_stats.subtract(cf_stats_snapshot_.comp_stats);
  rw_amp = (interval_stats.bytes_written +
      interval_stats.bytes_readn + interval_stats.bytes_readnp1) /
      static_cast<double>(interval_ingest);
  w_amp = interval_stats.bytes_written / static_cast<double>(interval_ingest);
  printlevelstats(buf, sizeof(buf), "int", 0, 0, 0, 0,
      rw_amp, w_amp, total_stall_us - cf_stats_snapshot_.stall_us,
      total_stall_count - cf_stats_snapshot_.stall_count, interval_stats);
  value->append(buf);

  snprintf(buf, sizeof(buf),
           "flush(gb): accumulative %.3f, interval %.3f\n",
           curr_ingest / kgb, interval_ingest / kgb);
  value->append(buf);
  snprintf(buf, sizeof(buf),
           "stalls(secs): %.3f level0_slowdown, %.3f level0_numfiles, "
           "%.3f memtable_compaction, %.3f leveln_slowdown_soft, "
           "%.3f leveln_slowdown_hard\n",
           cf_stats_value_[level0_slowdown] / 1000000.0,
           cf_stats_value_[level0_num_files] / 1000000.0,
           cf_stats_value_[memtable_compaction] / 1000000.0,
           total_slowdown_soft / 1000000.0,
           total_slowdown_hard / 1000000.0);
  value->append(buf);

  snprintf(buf, sizeof(buf),
           "stalls(count): %" priu64 " level0_slowdown, "
           "%" priu64 " level0_numfiles, %" priu64 " memtable_compaction, "
           "%" priu64 " leveln_slowdown_soft, "
           "%" priu64 " leveln_slowdown_hard\n",
           cf_stats_count_[level0_slowdown],
           cf_stats_count_[level0_num_files],
           cf_stats_count_[memtable_compaction],
           total_slowdown_count_soft, total_slowdown_count_hard);
  value->append(buf);

  cf_stats_snapshot_.ingest_bytes = curr_ingest;
  cf_stats_snapshot_.comp_stats = stats_sum;
  cf_stats_snapshot_.stall_us = total_stall_us;
  cf_stats_snapshot_.stall_count = total_stall_count;
}

}  // namespace rocksdb
