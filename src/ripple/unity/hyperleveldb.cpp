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

// unity build file for hyperleveldb

#include <beastconfig.h>

#include <ripple/unity/hyperleveldb.h>

#if ripple_hyperleveldb_available

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

#if beast_gcc
#pragma gcc diagnostic push
#pragma gcc diagnostic ignored "-wreorder"
#pragma gcc diagnostic ignored "-wunused-variable"
#pragma gcc diagnostic ignored "-wunused-but-set-variable"
#endif

#if beast_clang
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-wreorder"
#pragma clang diagnostic ignored "-wunused-private-field"
#pragma clang diagnostic ignored "-wunused-variable"
#pragma clang diagnostic ignored "-wunused-function"
#endif

// compile hyperleveldb without debugging unless specifically requested
#if !defined (ndebug) && !defined (ripple_debug_hyperleveldb)
#define ndebug
#endif

#include <hyperleveldb/db/builder.cc>
#include <hyperleveldb/db/db_impl.cc>
#include <hyperleveldb/db/db_iter.cc>
#include <hyperleveldb/db/dbformat.cc>
#include <hyperleveldb/db/filename.cc>
#include <hyperleveldb/db/log_reader.cc>
#include <hyperleveldb/db/log_writer.cc>
#include <hyperleveldb/db/memtable.cc>
#include <hyperleveldb/db/repair.cc>
#include <hyperleveldb/db/replay_iterator.cc>
#include <hyperleveldb/db/table_cache.cc>
#include <hyperleveldb/db/version_edit.cc>
#include <hyperleveldb/db/version_set.cc>
#include <hyperleveldb/db/write_batch.cc>

#include <hyperleveldb/table/block.cc>
#include <hyperleveldb/table/block_builder.cc>
#include <hyperleveldb/table/filter_block.cc>
#include <hyperleveldb/table/format.cc>
#include <hyperleveldb/table/iterator.cc>
#include <hyperleveldb/table/merger.cc>
#include <hyperleveldb/table/table.cc>
#include <hyperleveldb/table/table_builder.cc>
#include <hyperleveldb/table/two_level_iterator.cc>

#include <hyperleveldb/util/arena.cc>
#include <hyperleveldb/util/bloom.cc>
#include <hyperleveldb/util/cache.cc>
#include <hyperleveldb/util/coding.cc>
#include <hyperleveldb/util/comparator.cc>
#include <hyperleveldb/util/crc32c.cc>
#include <hyperleveldb/util/env.cc>
#include <hyperleveldb/util/filter_policy.cc>
#include <hyperleveldb/util/hash.cc>
#include <hyperleveldb/util/histogram.cc>
#include <hyperleveldb/util/logging.cc>
#include <hyperleveldb/util/options.cc>
#include <hyperleveldb/util/status.cc>

// platform specific

#if defined (leveldb_platform_windows)
#include <hyperleveldb/util/env_win.cc>
#include <hyperleveldb/port/port_win.cc>

#elif defined (leveldb_platform_posix)
#include <hyperleveldb/util/env_posix.cc>
#include <hyperleveldb/port/port_posix.cc>

#elif defined (leveldb_platform_android)
# error missing android port!

#endif

#if beast_gcc
#pragma gcc diagnostic pop
#endif

#if beast_clang
#pragma clang diagnostic pop
#endif

#endif
