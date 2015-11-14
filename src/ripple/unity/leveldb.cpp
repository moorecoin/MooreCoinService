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

// unity build file for leveldb

#include <beastconfig.h>

#include <ripple/unity/leveldb.h>

#include <beast/config.h>

// set the appropriate leveldb platform macro based on our platform.
//
#if beast_win32
 #define leveldb_platform_windows

#else
 #define leveldb_platform_posix

 #if beast_mac || beast_ios
  #define os_macosx

 // vfalco todo distinguish between beast_bsd and beast_freebsd
 #elif beast_bsd
  #define os_freebsd

 #endif

#endif

#if beast_clang
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-wunused-function"
#endif

#include <leveldb/db/builder.cc>
#include <leveldb/db/db_impl.cc>
#include <leveldb/db/db_iter.cc>
#include <leveldb/db/dbformat.cc>
#include <leveldb/db/filename.cc>
#include <leveldb/db/log_reader.cc>
#include <leveldb/db/log_writer.cc>
#include <leveldb/db/memtable.cc>
#include <leveldb/db/repair.cc>
#include <leveldb/db/table_cache.cc>
#include <leveldb/db/version_edit.cc>
#include <leveldb/db/version_set.cc>
#include <leveldb/db/write_batch.cc>

#include <leveldb/table/block.cc>
#include <leveldb/table/block_builder.cc>
#include <leveldb/table/filter_block.cc>
#include <leveldb/table/format.cc>
#include <leveldb/table/iterator.cc>
#include <leveldb/table/merger.cc>
#include <leveldb/table/table.cc>
#include <leveldb/table/table_builder.cc>
#include <leveldb/table/two_level_iterator.cc>

#include <leveldb/util/arena.cc>
#include <leveldb/util/bloom.cc>
#include <leveldb/util/cache.cc>
#include <leveldb/util/coding.cc>
#include <leveldb/util/comparator.cc>
#include <leveldb/util/crc32c.cc>
#include <leveldb/util/env.cc>
#include <leveldb/util/filter_policy.cc>
#include <leveldb/util/hash.cc>
#include <leveldb/util/histogram.cc>
#include <leveldb/util/logging.cc>
#include <leveldb/util/options.cc>
#include <leveldb/util/status.cc>

// platform specific

#if defined (leveldb_platform_windows)
#include <leveldb/util/env_win.cc>
#include <leveldb/port/port_win.cc>

#elif defined (leveldb_platform_posix)
#include <leveldb/util/env_posix.cc>
#include <leveldb/port/port_posix.cc>

#elif defined (leveldb_platform_android)
# error missing android port!

#endif

#if beast_clang
#pragma clang diagnostic pop
#endif
