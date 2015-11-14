//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "rocksdb/options.h"

#define __stdc_format_macros
#include <inttypes.h>
#include <limits>

#include "rocksdb/cache.h"
#include "rocksdb/compaction_filter.h"
#include "rocksdb/comparator.h"
#include "rocksdb/env.h"
#include "rocksdb/memtablerep.h"
#include "rocksdb/merge_operator.h"
#include "rocksdb/slice.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "rocksdb/table_properties.h"
#include "table/block_based_table_factory.h"
#include "util/statistics.h"

namespace rocksdb {

columnfamilyoptions::columnfamilyoptions()
    : comparator(bytewisecomparator()),
      merge_operator(nullptr),
      compaction_filter(nullptr),
      compaction_filter_factory(std::shared_ptr<compactionfilterfactory>(
          new defaultcompactionfilterfactory())),
      compaction_filter_factory_v2(new defaultcompactionfilterfactoryv2()),
      write_buffer_size(4 << 20),
      max_write_buffer_number(2),
      min_write_buffer_number_to_merge(1),
      compression(ksnappycompression),
      prefix_extractor(nullptr),
      num_levels(7),
      level0_file_num_compaction_trigger(4),
      level0_slowdown_writes_trigger(20),
      level0_stop_writes_trigger(24),
      max_mem_compaction_level(2),
      target_file_size_base(2 * 1048576),
      target_file_size_multiplier(1),
      max_bytes_for_level_base(10 * 1048576),
      max_bytes_for_level_multiplier(10),
      max_bytes_for_level_multiplier_additional(num_levels, 1),
      expanded_compaction_factor(25),
      source_compaction_factor(1),
      max_grandparent_overlap_factor(10),
      soft_rate_limit(0.0),
      hard_rate_limit(0.0),
      rate_limit_delay_max_milliseconds(1000),
      arena_block_size(0),
      disable_auto_compactions(false),
      purge_redundant_kvs_while_flush(true),
      compaction_style(kcompactionstylelevel),
      verify_checksums_in_compaction(true),
      filter_deletes(false),
      max_sequential_skip_in_iterations(8),
      memtable_factory(std::shared_ptr<skiplistfactory>(new skiplistfactory)),
      table_factory(
          std::shared_ptr<tablefactory>(new blockbasedtablefactory())),
      inplace_update_support(false),
      inplace_update_num_locks(10000),
      inplace_callback(nullptr),
      memtable_prefix_bloom_bits(0),
      memtable_prefix_bloom_probes(6),
      memtable_prefix_bloom_huge_page_tlb_size(0),
      bloom_locality(0),
      max_successive_merges(0),
      min_partial_merge_operands(2) {
  assert(memtable_factory.get() != nullptr);
}

columnfamilyoptions::columnfamilyoptions(const options& options)
    : comparator(options.comparator),
      merge_operator(options.merge_operator),
      compaction_filter(options.compaction_filter),
      compaction_filter_factory(options.compaction_filter_factory),
      compaction_filter_factory_v2(options.compaction_filter_factory_v2),
      write_buffer_size(options.write_buffer_size),
      max_write_buffer_number(options.max_write_buffer_number),
      min_write_buffer_number_to_merge(
          options.min_write_buffer_number_to_merge),
      compression(options.compression),
      compression_per_level(options.compression_per_level),
      compression_opts(options.compression_opts),
      prefix_extractor(options.prefix_extractor),
      num_levels(options.num_levels),
      level0_file_num_compaction_trigger(
          options.level0_file_num_compaction_trigger),
      level0_slowdown_writes_trigger(options.level0_slowdown_writes_trigger),
      level0_stop_writes_trigger(options.level0_stop_writes_trigger),
      max_mem_compaction_level(options.max_mem_compaction_level),
      target_file_size_base(options.target_file_size_base),
      target_file_size_multiplier(options.target_file_size_multiplier),
      max_bytes_for_level_base(options.max_bytes_for_level_base),
      max_bytes_for_level_multiplier(options.max_bytes_for_level_multiplier),
      max_bytes_for_level_multiplier_additional(
          options.max_bytes_for_level_multiplier_additional),
      expanded_compaction_factor(options.expanded_compaction_factor),
      source_compaction_factor(options.source_compaction_factor),
      max_grandparent_overlap_factor(options.max_grandparent_overlap_factor),
      soft_rate_limit(options.soft_rate_limit),
      hard_rate_limit(options.hard_rate_limit),
      rate_limit_delay_max_milliseconds(
          options.rate_limit_delay_max_milliseconds),
      arena_block_size(options.arena_block_size),
      disable_auto_compactions(options.disable_auto_compactions),
      purge_redundant_kvs_while_flush(options.purge_redundant_kvs_while_flush),
      compaction_style(options.compaction_style),
      verify_checksums_in_compaction(options.verify_checksums_in_compaction),
      compaction_options_universal(options.compaction_options_universal),
      compaction_options_fifo(options.compaction_options_fifo),
      filter_deletes(options.filter_deletes),
      max_sequential_skip_in_iterations(
          options.max_sequential_skip_in_iterations),
      memtable_factory(options.memtable_factory),
      table_factory(options.table_factory),
      table_properties_collector_factories(
          options.table_properties_collector_factories),
      inplace_update_support(options.inplace_update_support),
      inplace_update_num_locks(options.inplace_update_num_locks),
      inplace_callback(options.inplace_callback),
      memtable_prefix_bloom_bits(options.memtable_prefix_bloom_bits),
      memtable_prefix_bloom_probes(options.memtable_prefix_bloom_probes),
      memtable_prefix_bloom_huge_page_tlb_size(
          options.memtable_prefix_bloom_huge_page_tlb_size),
      bloom_locality(options.bloom_locality),
      max_successive_merges(options.max_successive_merges),
      min_partial_merge_operands(options.min_partial_merge_operands) {
  assert(memtable_factory.get() != nullptr);
  if (max_bytes_for_level_multiplier_additional.size() <
      static_cast<unsigned int>(num_levels)) {
    max_bytes_for_level_multiplier_additional.resize(num_levels, 1);
  }
}

dboptions::dboptions()
    : create_if_missing(false),
      create_missing_column_families(false),
      error_if_exists(false),
      paranoid_checks(true),
      env(env::default()),
      rate_limiter(nullptr),
      info_log(nullptr),
      info_log_level(info_level),
      max_open_files(5000),
      max_total_wal_size(0),
      statistics(nullptr),
      disabledatasync(false),
      use_fsync(false),
      db_log_dir(""),
      wal_dir(""),
      delete_obsolete_files_period_micros(6 * 60 * 60 * 1000000ul),
      max_background_compactions(1),
      max_background_flushes(1),
      max_log_file_size(0),
      log_file_time_to_roll(0),
      keep_log_file_num(1000),
      max_manifest_file_size(std::numeric_limits<uint64_t>::max()),
      table_cache_numshardbits(4),
      table_cache_remove_scan_count_limit(16),
      wal_ttl_seconds(0),
      wal_size_limit_mb(0),
      manifest_preallocation_size(4 * 1024 * 1024),
      allow_os_buffer(true),
      allow_mmap_reads(false),
      allow_mmap_writes(false),
      is_fd_close_on_exec(true),
      skip_log_error_on_recovery(false),
      stats_dump_period_sec(3600),
      advise_random_on_open(true),
      access_hint_on_compaction_start(normal),
      use_adaptive_mutex(false),
      allow_thread_local(true),
      bytes_per_sync(0) {}

dboptions::dboptions(const options& options)
    : create_if_missing(options.create_if_missing),
      create_missing_column_families(options.create_missing_column_families),
      error_if_exists(options.error_if_exists),
      paranoid_checks(options.paranoid_checks),
      env(options.env),
      rate_limiter(options.rate_limiter),
      info_log(options.info_log),
      info_log_level(options.info_log_level),
      max_open_files(options.max_open_files),
      max_total_wal_size(options.max_total_wal_size),
      statistics(options.statistics),
      disabledatasync(options.disabledatasync),
      use_fsync(options.use_fsync),
      db_paths(options.db_paths),
      db_log_dir(options.db_log_dir),
      wal_dir(options.wal_dir),
      delete_obsolete_files_period_micros(
          options.delete_obsolete_files_period_micros),
      max_background_compactions(options.max_background_compactions),
      max_background_flushes(options.max_background_flushes),
      max_log_file_size(options.max_log_file_size),
      log_file_time_to_roll(options.log_file_time_to_roll),
      keep_log_file_num(options.keep_log_file_num),
      max_manifest_file_size(options.max_manifest_file_size),
      table_cache_numshardbits(options.table_cache_numshardbits),
      table_cache_remove_scan_count_limit(
          options.table_cache_remove_scan_count_limit),
      wal_ttl_seconds(options.wal_ttl_seconds),
      wal_size_limit_mb(options.wal_size_limit_mb),
      manifest_preallocation_size(options.manifest_preallocation_size),
      allow_os_buffer(options.allow_os_buffer),
      allow_mmap_reads(options.allow_mmap_reads),
      allow_mmap_writes(options.allow_mmap_writes),
      is_fd_close_on_exec(options.is_fd_close_on_exec),
      skip_log_error_on_recovery(options.skip_log_error_on_recovery),
      stats_dump_period_sec(options.stats_dump_period_sec),
      advise_random_on_open(options.advise_random_on_open),
      access_hint_on_compaction_start(options.access_hint_on_compaction_start),
      use_adaptive_mutex(options.use_adaptive_mutex),
      allow_thread_local(options.allow_thread_local),
      bytes_per_sync(options.bytes_per_sync) {}

static const char* const access_hints[] = {
  "none", "normal", "sequential", "willneed"
};

void dboptions::dump(logger* log) const {
    log(log,"         options.error_if_exists: %d", error_if_exists);
    log(log,"       options.create_if_missing: %d", create_if_missing);
    log(log,"         options.paranoid_checks: %d", paranoid_checks);
    log(log,"                     options.env: %p", env);
    log(log,"                options.info_log: %p", info_log.get());
    log(log,"          options.max_open_files: %d", max_open_files);
    log(log,"      options.max_total_wal_size: %" priu64, max_total_wal_size);
    log(log, "       options.disabledatasync: %d", disabledatasync);
    log(log, "             options.use_fsync: %d", use_fsync);
    log(log, "     options.max_log_file_size: %zu", max_log_file_size);
    log(log, "options.max_manifest_file_size: %lu",
        (unsigned long)max_manifest_file_size);
    log(log, "     options.log_file_time_to_roll: %zu", log_file_time_to_roll);
    log(log, "     options.keep_log_file_num: %zu", keep_log_file_num);
    log(log, "       options.allow_os_buffer: %d", allow_os_buffer);
    log(log, "      options.allow_mmap_reads: %d", allow_mmap_reads);
    log(log, "     options.allow_mmap_writes: %d", allow_mmap_writes);
    log(log, "         options.create_missing_column_families: %d",
        create_missing_column_families);
    log(log, "                             options.db_log_dir: %s",
        db_log_dir.c_str());
    log(log, "                                options.wal_dir: %s",
        wal_dir.c_str());
    log(log, "               options.table_cache_numshardbits: %d",
        table_cache_numshardbits);
    log(log, "    options.table_cache_remove_scan_count_limit: %d",
        table_cache_remove_scan_count_limit);
    log(log, "    options.delete_obsolete_files_period_micros: %lu",
        (unsigned long)delete_obsolete_files_period_micros);
    log(log, "             options.max_background_compactions: %d",
        max_background_compactions);
    log(log, "                 options.max_background_flushes: %d",
        max_background_flushes);
    log(log, "                        options.wal_ttl_seconds: %lu",
        (unsigned long)wal_ttl_seconds);
    log(log, "                      options.wal_size_limit_mb: %lu",
        (unsigned long)wal_size_limit_mb);
    log(log, "            options.manifest_preallocation_size: %zu",
        manifest_preallocation_size);
    log(log, "                         options.allow_os_buffer: %d",
        allow_os_buffer);
    log(log, "                        options.allow_mmap_reads: %d",
        allow_mmap_reads);
    log(log, "                       options.allow_mmap_writes: %d",
        allow_mmap_writes);
    log(log, "                     options.is_fd_close_on_exec: %d",
        is_fd_close_on_exec);
    log(log, "              options.skip_log_error_on_recovery: %d",
        skip_log_error_on_recovery);
    log(log, "                   options.stats_dump_period_sec: %u",
        stats_dump_period_sec);
    log(log, "                   options.advise_random_on_open: %d",
        advise_random_on_open);
    log(log, "         options.access_hint_on_compaction_start: %s",
        access_hints[access_hint_on_compaction_start]);
    log(log, "                      options.use_adaptive_mutex: %d",
        use_adaptive_mutex);
    log(log, "                            options.rate_limiter: %p",
        rate_limiter.get());
    log(log, "                          options.bytes_per_sync: %lu",
        (unsigned long)bytes_per_sync);
}  // dboptions::dump

void columnfamilyoptions::dump(logger* log) const {
  log(log, "              options.comparator: %s", comparator->name());
  log(log, "          options.merge_operator: %s",
      merge_operator ? merge_operator->name() : "none");
  log(log, "       options.compaction_filter: %s",
      compaction_filter ? compaction_filter->name() : "none");
  log(log, "       options.compaction_filter_factory: %s",
      compaction_filter_factory->name());
  log(log, "       options.compaction_filter_factory_v2: %s",
      compaction_filter_factory_v2->name());
  log(log, "        options.memtable_factory: %s", memtable_factory->name());
  log(log, "           options.table_factory: %s", table_factory->name());
  log(log, "           table_factory options: %s",
      table_factory->getprintabletableoptions().c_str());
  log(log, "       options.write_buffer_size: %zd", write_buffer_size);
  log(log, " options.max_write_buffer_number: %d", max_write_buffer_number);
    if (!compression_per_level.empty()) {
      for (unsigned int i = 0; i < compression_per_level.size(); i++) {
          log(log,"       options.compression[%d]: %d",
              i, compression_per_level[i]);
       }
    } else {
      log(log,"         options.compression: %d", compression);
    }
    log(log,"      options.prefix_extractor: %s",
        prefix_extractor == nullptr ? "nullptr" : prefix_extractor->name());
    log(log,"            options.num_levels: %d", num_levels);
    log(log,"       options.min_write_buffer_number_to_merge: %d",
        min_write_buffer_number_to_merge);
    log(log,"        options.purge_redundant_kvs_while_flush: %d",
         purge_redundant_kvs_while_flush);
    log(log,"           options.compression_opts.window_bits: %d",
        compression_opts.window_bits);
    log(log,"                 options.compression_opts.level: %d",
        compression_opts.level);
    log(log,"              options.compression_opts.strategy: %d",
        compression_opts.strategy);
    log(log,"     options.level0_file_num_compaction_trigger: %d",
        level0_file_num_compaction_trigger);
    log(log,"         options.level0_slowdown_writes_trigger: %d",
        level0_slowdown_writes_trigger);
    log(log,"             options.level0_stop_writes_trigger: %d",
        level0_stop_writes_trigger);
    log(log,"               options.max_mem_compaction_level: %d",
        max_mem_compaction_level);
    log(log,"                  options.target_file_size_base: %d",
        target_file_size_base);
    log(log,"            options.target_file_size_multiplier: %d",
        target_file_size_multiplier);
    log(log,"               options.max_bytes_for_level_base: %lu",
        (unsigned long)max_bytes_for_level_base);
    log(log,"         options.max_bytes_for_level_multiplier: %d",
        max_bytes_for_level_multiplier);
    for (int i = 0; i < num_levels; i++) {
      log(log,"options.max_bytes_for_level_multiplier_addtl[%d]: %d",
          i, max_bytes_for_level_multiplier_additional[i]);
    }
    log(log,"      options.max_sequential_skip_in_iterations: %lu",
        (unsigned long)max_sequential_skip_in_iterations);
    log(log,"             options.expanded_compaction_factor: %d",
        expanded_compaction_factor);
    log(log,"               options.source_compaction_factor: %d",
        source_compaction_factor);
    log(log,"         options.max_grandparent_overlap_factor: %d",
        max_grandparent_overlap_factor);
    log(log,"                       options.arena_block_size: %zu",
        arena_block_size);
    log(log,"                      options.soft_rate_limit: %.2f",
        soft_rate_limit);
    log(log,"                      options.hard_rate_limit: %.2f",
        hard_rate_limit);
    log(log,"      options.rate_limit_delay_max_milliseconds: %u",
        rate_limit_delay_max_milliseconds);
    log(log,"               options.disable_auto_compactions: %d",
        disable_auto_compactions);
    log(log,"         options.purge_redundant_kvs_while_flush: %d",
        purge_redundant_kvs_while_flush);
    log(log,"                          options.filter_deletes: %d",
        filter_deletes);
    log(log, "          options.verify_checksums_in_compaction: %d",
        verify_checksums_in_compaction);
    log(log,"                        options.compaction_style: %d",
        compaction_style);
    log(log," options.compaction_options_universal.size_ratio: %u",
        compaction_options_universal.size_ratio);
    log(log,"options.compaction_options_universal.min_merge_width: %u",
        compaction_options_universal.min_merge_width);
    log(log,"options.compaction_options_universal.max_merge_width: %u",
        compaction_options_universal.max_merge_width);
    log(log,"options.compaction_options_universal."
            "max_size_amplification_percent: %u",
        compaction_options_universal.max_size_amplification_percent);
    log(log,
        "options.compaction_options_universal.compression_size_percent: %u",
        compaction_options_universal.compression_size_percent);
    log(log, "options.compaction_options_fifo.max_table_files_size: %" priu64,
        compaction_options_fifo.max_table_files_size);
    std::string collector_names;
    for (const auto& collector_factory : table_properties_collector_factories) {
      collector_names.append(collector_factory->name());
      collector_names.append("; ");
    }
    log(log, "                  options.table_properties_collectors: %s",
        collector_names.c_str());
    log(log, "                  options.inplace_update_support: %d",
        inplace_update_support);
    log(log, "                options.inplace_update_num_locks: %zd",
        inplace_update_num_locks);
    log(log, "              options.min_partial_merge_operands: %u",
        min_partial_merge_operands);
    // todo: easier config for bloom (maybe based on avg key/value size)
    log(log, "              options.memtable_prefix_bloom_bits: %d",
        memtable_prefix_bloom_bits);
    log(log, "            options.memtable_prefix_bloom_probes: %d",
        memtable_prefix_bloom_probes);
    log(log, "  options.memtable_prefix_bloom_huge_page_tlb_size: %zu",
        memtable_prefix_bloom_huge_page_tlb_size);
    log(log, "                          options.bloom_locality: %d",
        bloom_locality);
    log(log, "                   options.max_successive_merges: %zd",
        max_successive_merges);
}  // columnfamilyoptions::dump

void options::dump(logger* log) const {
  dboptions::dump(log);
  columnfamilyoptions::dump(log);
}   // options::dump

//
// the goal of this method is to create a configuration that
// allows an application to write all files into l0 and
// then do a single compaction to output all files into l1.
options*
options::prepareforbulkload()
{
  // never slowdown ingest.
  level0_file_num_compaction_trigger = (1<<30);
  level0_slowdown_writes_trigger = (1<<30);
  level0_stop_writes_trigger = (1<<30);

  // no auto compactions please. the application should issue a
  // manual compaction after all data is loaded into l0.
  disable_auto_compactions = true;
  disabledatasync = true;

  // a manual compaction run should pick all files in l0 in
  // a single compaction run.
  source_compaction_factor = (1<<30);

  // it is better to have only 2 levels, otherwise a manual
  // compaction would compact at every possible level, thereby
  // increasing the total time needed for compactions.
  num_levels = 2;

  // prevent a memtable flush to automatically promote files
  // to l1. this is helpful so that all files that are
  // input to the manual compaction are all at l0.
  max_background_compactions = 2;

  // the compaction would create large files in l1.
  target_file_size_base = 256 * 1024 * 1024;
  return this;
}

// optimization functions
columnfamilyoptions* columnfamilyoptions::optimizeforpointlookup(
    uint64_t block_cache_size_mb) {
  prefix_extractor.reset(newnooptransform());
  blockbasedtableoptions block_based_options;
  block_based_options.index_type = blockbasedtableoptions::khashsearch;
  block_based_options.filter_policy.reset(newbloomfilterpolicy(10));
  block_based_options.block_cache =
    newlrucache(block_cache_size_mb * 1024 * 1024);
  table_factory.reset(new blockbasedtablefactory(block_based_options));
#ifndef rocksdb_lite
  memtable_factory.reset(newhashlinklistrepfactory());
#endif
  return this;
}

columnfamilyoptions* columnfamilyoptions::optimizelevelstylecompaction(
    uint64_t memtable_memory_budget) {
  write_buffer_size = memtable_memory_budget / 4;
  // merge two memtables when flushing to l0
  min_write_buffer_number_to_merge = 2;
  // this means we'll use 50% extra memory in the worst case, but will reduce
  // write stalls.
  max_write_buffer_number = 6;
  // start flushing l0->l1 as soon as possible. each file on level0 is
  // (memtable_memory_budget / 2). this will flush level 0 when it's bigger than
  // memtable_memory_budget.
  level0_file_num_compaction_trigger = 2;
  // doesn't really matter much, but we don't want to create too many files
  target_file_size_base = memtable_memory_budget / 8;
  // make level1 size equal to level0 size, so that l0->l1 compactions are fast
  max_bytes_for_level_base = memtable_memory_budget;

  // level style compaction
  compaction_style = kcompactionstylelevel;

  // only compress levels >= 2
  compression_per_level.resize(num_levels);
  for (int i = 0; i < num_levels; ++i) {
    if (i < 2) {
      compression_per_level[i] = knocompression;
    } else {
      compression_per_level[i] = ksnappycompression;
    }
  }
  return this;
}

columnfamilyoptions* columnfamilyoptions::optimizeuniversalstylecompaction(
    uint64_t memtable_memory_budget) {
  write_buffer_size = memtable_memory_budget / 4;
  // merge two memtables when flushing to l0
  min_write_buffer_number_to_merge = 2;
  // this means we'll use 50% extra memory in the worst case, but will reduce
  // write stalls.
  max_write_buffer_number = 6;
  // universal style compaction
  compaction_style = kcompactionstyleuniversal;
  compaction_options_universal.compression_size_percent = 80;
  return this;
}

dboptions* dboptions::increaseparallelism(int total_threads) {
  max_background_compactions = total_threads - 1;
  max_background_flushes = 1;
  env->setbackgroundthreads(total_threads, env::low);
  env->setbackgroundthreads(1, env::high);
  return this;
}

}  // namespace rocksdb
