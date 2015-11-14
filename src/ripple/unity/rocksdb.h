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

#ifndef ripple_rocksdb_h_included
#define ripple_rocksdb_h_included

#include <beast/config.h>

#ifndef ripple_rocksdb_available
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
# if beast_win32
#  define ripple_rocksdb_available 0
# else
#  if __cplusplus >= 201103l
#   define ripple_rocksdb_available 1
#  else
#   define ripple_rocksdb_available 0
#  endif
# endif
#endif

#if ripple_rocksdb_available
#define snappy
//#include <rocksdb2/port/port_posix.h>
#include <rocksdb2/include/rocksdb/cache.h>
#include <rocksdb2/include/rocksdb/compaction_filter.h>
#include <rocksdb2/include/rocksdb/comparator.h>
#include <rocksdb2/include/rocksdb/db.h>
#include <rocksdb2/include/rocksdb/env.h>
#include <rocksdb2/include/rocksdb/filter_policy.h>
#include <rocksdb2/include/rocksdb/flush_block_policy.h>
#include <rocksdb2/include/rocksdb/iterator.h>
#include <rocksdb2/include/rocksdb/memtablerep.h>
#include <rocksdb2/include/rocksdb/merge_operator.h>
#include <rocksdb2/include/rocksdb/options.h>
#include <rocksdb2/include/rocksdb/perf_context.h>
#include <rocksdb2/include/rocksdb/slice.h>
#include <rocksdb2/include/rocksdb/slice_transform.h>
#include <rocksdb2/include/rocksdb/statistics.h>
#include <rocksdb2/include/rocksdb/status.h>
#include <rocksdb2/include/rocksdb/table.h>
#include <rocksdb2/include/rocksdb/table_properties.h>
#include <rocksdb2/include/rocksdb/transaction_log.h>
#include <rocksdb2/include/rocksdb/types.h>
#include <rocksdb2/include/rocksdb/universal_compaction.h>
#include <rocksdb2/include/rocksdb/write_batch.h>

#endif

#endif
