//------------------------------------------------------------------------------
/*
    this file is part of rippled: https://github.com/ripple/rippled
    copyright (c) 2012, 2013 ripple labs inc.

    permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    the  software is provided "as is" and the author disclaims all warranties
    with  regard  to  this  software  including  all  implied  warranties  of
    merchantability  and  fitness. in no event shall the author be liable for
    any  special ,  direct, indirect, or consequential damages or any damages
    whatsoever  resulting  from  loss  of use, data or profits, whether in an
    action  of  contract, negligence or other tortious action, arising out of
    or in connection with the use or performance of this software.
*/
//==============================================================================

#include <beastconfig.h>

#include <ripple/unity/rocksdb.h>

#if ripple_rocksdb_available

#if beast_win32
# define rocksdb_platform_windows
#else
# define rocksdb_platform_posix
# if beast_mac || beast_ios
#  define os_macosx 1
# elif beast_bsd
#  define os_freebsd
# else
#  define os_linux
# endif
#endif

#if beast_gcc
# pragma gcc diagnostic push
# pragma gcc diagnostic ignored "-wreorder"
# pragma gcc diagnostic ignored "-wunused-variable"
# pragma gcc diagnostic ignored "-wunused-but-set-variable"
#endif

// compile rocksdb without debugging unless specifically requested
#if !defined (ndebug) && !defined (ripple_debug_rocksdb)
#define ndebug
#endif

#include <rocksdb2/db/builder.cc>
#include <rocksdb2/db/c.cc>
#include <rocksdb2/db/column_family.cc>
#include <rocksdb2/db/compaction.cc>
#include <rocksdb2/db/compaction_picker.cc>
#include <rocksdb2/db/db_filesnapshot.cc>
#include <rocksdb2/db/dbformat.cc>
#include <rocksdb2/db/db_impl.cc>
#include <rocksdb2/db/db_impl_debug.cc>
#include <rocksdb2/db/db_impl_readonly.cc>
#include <rocksdb2/db/db_iter.cc>
#include <rocksdb2/db/file_indexer.cc>
#include <rocksdb2/db/filename.cc>
#include <rocksdb2/db/forward_iterator.cc>
#include <rocksdb2/db/internal_stats.cc>
#include <rocksdb2/db/log_reader.cc>
#include <rocksdb2/db/log_writer.cc>
#include <rocksdb2/db/memtable.cc>
#include <rocksdb2/db/memtable_list.cc>
#include <rocksdb2/db/merge_helper.cc>
#include <rocksdb2/db/merge_operator.cc>
#include <rocksdb2/db/repair.cc>
#include <rocksdb2/db/table_cache.cc>
#include <rocksdb2/db/table_properties_collector.cc>
#include <rocksdb2/db/transaction_log_impl.cc>
#include <rocksdb2/db/version_edit.cc>
#include <rocksdb2/db/version_set.cc>
#include <rocksdb2/db/write_batch.cc>
#include <rocksdb2/table/adaptive_table_factory.cc>
#include <rocksdb2/table/block_based_table_builder.cc>
#include <rocksdb2/table/block_based_table_factory.cc>
#include <rocksdb2/table/block_based_table_reader.cc>
#include <rocksdb2/table/block_builder.cc>
#include <rocksdb2/table/block.cc>
#include <rocksdb2/table/block_hash_index.cc>
#include <rocksdb2/table/block_prefix_index.cc>
#include <rocksdb2/table/bloom_block.cc>
#include <rocksdb2/table/cuckoo_table_builder.cc>
#include <rocksdb2/table/cuckoo_table_factory.cc>
#include <rocksdb2/table/cuckoo_table_reader.cc>
#include <rocksdb2/table/filter_block.cc>
#include <rocksdb2/table/flush_block_policy.cc>
#include <rocksdb2/table/format.cc>
#include <rocksdb2/table/iterator.cc>
#include <rocksdb2/table/merger.cc>
#include <rocksdb2/table/meta_blocks.cc>
#include <rocksdb2/table/plain_table_builder.cc>
#include <rocksdb2/table/plain_table_factory.cc>
#include <rocksdb2/table/plain_table_index.cc>
#include <rocksdb2/table/plain_table_key_coding.cc>
#include <rocksdb2/table/plain_table_reader.cc>
#include <rocksdb2/table/table_properties.cc>
#include <rocksdb2/table/two_level_iterator.cc>
#include <rocksdb2/util/arena.cc>
#include <rocksdb2/util/auto_roll_logger.cc>
#include <rocksdb2/util/blob_store.cc>
#include <rocksdb2/util/bloom.cc>
#include <rocksdb2/util/cache.cc>
#include <rocksdb2/util/coding.cc>
#include <rocksdb2/util/comparator.cc>
#include <rocksdb2/util/crc32c.cc>
#include <rocksdb2/util/db_info_dummper.cc>
#include <rocksdb2/util/dynamic_bloom.cc>
#include <rocksdb2/util/env.cc>
#include <rocksdb2/util/env_hdfs.cc>
#include <rocksdb2/util/env_posix.cc>
#include <rocksdb2/util/filter_policy.cc>
#include <rocksdb2/util/hash.cc>
#include <rocksdb2/util/hash_cuckoo_rep.cc>
#include <rocksdb2/util/hash_linklist_rep.cc>
#include <rocksdb2/util/hash_skiplist_rep.cc>
#include <rocksdb2/util/histogram.cc>
#include <rocksdb2/util/iostats_context.cc>
#include <rocksdb2/utilities/backupable/backupable_db.cc>
#include <rocksdb2/utilities/document/document_db.cc>
#include <rocksdb2/utilities/document/json_document.cc>
#include <rocksdb2/utilities/geodb/geodb_impl.cc>
#include <rocksdb2/utilities/merge_operators/put.cc>
#include <rocksdb2/utilities/merge_operators/string_append/stringappend2.cc>
#include <rocksdb2/utilities/merge_operators/string_append/stringappend.cc>
#include <rocksdb2/utilities/merge_operators/uint64add.cc>
#include <rocksdb2/utilities/redis/redis_lists.cc>
#include <rocksdb2/utilities/spatialdb/spatial_db.cc>
#include <rocksdb2/utilities/ttl/db_ttl_impl.cc>
#include <rocksdb2/util/ldb_cmd.cc>
#include <rocksdb2/util/ldb_tool.cc>
#include <rocksdb2/util/log_buffer.cc>
#include <rocksdb2/util/logging.cc>
#include <rocksdb2/util/murmurhash.cc>
#include <rocksdb2/util/options_builder.cc>
#include <rocksdb2/util/options.cc>
#include <rocksdb2/util/perf_context.cc>
#include <rocksdb2/util/rate_limiter.cc>
#include <rocksdb2/util/skiplistrep.cc>
#include <rocksdb2/util/slice.cc>
#include <rocksdb2/util/statistics.cc>
#include <rocksdb2/util/status.cc>
#include <rocksdb2/util/string_util.cc>
#include <rocksdb2/util/sync_point.cc>
#include <rocksdb2/util/thread_local.cc>
#include <rocksdb2/util/vectorrep.cc>
#include <rocksdb2/util/xxhash.cc>
#include <rocksdb2/port/stack_trace.cc>
#include <rocksdb2/port/port_posix.cc>

const char* rocksdb_build_git_sha = "<none>";
const char* rocksdb_build_git_datetime = "<none>";
// don't use __date__ and __time__, otherwise
// builds will be nondeterministic.
const char* rocksdb_build_compile_date = "ripple labs";
const char* rocksdb_build_compile_time = "c++ team";
//const char* rocksdb_build_compile_date = __date__;
//const char* rocksdb_build_compile_time = __time__;

#endif
