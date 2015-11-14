## copyright (c) 2013
## all rights reserved.
##
## redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are met:
##
##     * redistributions of source code must retain the above copyright notice,
##       this list of conditions and the following disclaimer.
##     * redistributions in binary form must reproduce the above copyright
##       notice, this list of conditions and the following disclaimer in the
##       documentation and/or other materials provided with the distribution.
##     * neither the name of nb nor the names of its contributors may be used to
##		 endorse or promote products derived from this software without specific
##		 prior written permission.
##
## this software is provided by the copyright holders and contributors "as is"
## and any express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular purpose
## are disclaimed. in no event shall the copyright owner or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or business
## interruption) however caused and on any theory of liability, whether in
## contract, strict liability, or tort (including negligence or otherwise)
## arising in any way out of the use of this software, even if advised of the
## possibility of such damage.

aclocal_amflags = -i m4 ${aclocal_flags}
am_cflags = -dleveldb_platform_posix $(snappy_flags) -fno-builtin-memcmp -fno-builtin-memmove
am_cxxflags = -dleveldb_platform_posix $(snappy_flags) -fno-builtin-memcmp -fno-builtin-memmove
am_makeflags = --no-print-directory

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_data = libhyperleveldb.pc

extra_dist =
extra_dist += authors
extra_dist += doc/benchmark.html
extra_dist += doc/doc.css
extra_dist += doc/impl.html
extra_dist += doc/index.html
extra_dist += doc/log_format.txt
extra_dist += doc/table_format.txt
extra_dist += helpers/memenv/memenv.cc
extra_dist += helpers/memenv/memenv_test.cc
extra_dist += license
extra_dist += news
extra_dist += port/readme
extra_dist += readme
extra_dist += todo

nobase_include_headers =
nobase_include_headers += hyperleveldb/cache.h
nobase_include_headers += hyperleveldb/c.h
nobase_include_headers += hyperleveldb/comparator.h
nobase_include_headers += hyperleveldb/db.h
nobase_include_headers += hyperleveldb/env.h
nobase_include_headers += hyperleveldb/filter_policy.h
nobase_include_headers += hyperleveldb/iterator.h
nobase_include_headers += hyperleveldb/options.h
nobase_include_headers += hyperleveldb/slice.h
nobase_include_headers += hyperleveldb/replay_iterator.h
nobase_include_headers += hyperleveldb/status.h
nobase_include_headers += hyperleveldb/table_builder.h
nobase_include_headers += hyperleveldb/table.h
nobase_include_headers += hyperleveldb/write_batch.h
noinst_headers =
noinst_headers += db/builder.h
noinst_headers += db/dbformat.h
noinst_headers += db/db_impl.h
noinst_headers += db/db_iter.h
noinst_headers += db/filename.h
noinst_headers += db/log_format.h
noinst_headers += db/log_reader.h
noinst_headers += db/log_writer.h
noinst_headers += db/memtable.h
noinst_headers += db/skiplist.h
noinst_headers += db/replay_iterator.h
noinst_headers += db/snapshot.h
noinst_headers += db/table_cache.h
noinst_headers += db/version_edit.h
noinst_headers += db/version_set.h
noinst_headers += db/write_batch_internal.h
noinst_headers += helpers/memenv/memenv.h
noinst_headers += port/atomic_pointer.h
noinst_headers += port/port_example.h
noinst_headers += port/port.h
noinst_headers += port/port_posix.h
noinst_headers += port/thread_annotations.h
noinst_headers += port/win/stdint.h
noinst_headers += table/block_builder.h
noinst_headers += table/block.h
noinst_headers += table/filter_block.h
noinst_headers += table/format.h
noinst_headers += table/iterator_wrapper.h
noinst_headers += table/merger.h
noinst_headers += table/two_level_iterator.h
noinst_headers += util/arena.h
noinst_headers += util/coding.h
noinst_headers += util/crc32c.h
noinst_headers += util/hash.h
noinst_headers += util/histogram.h
noinst_headers += util/logging.h
noinst_headers += util/mutexlock.h
noinst_headers += util/posix_logger.h
noinst_headers += util/random.h
noinst_headers += util/testharness.h
noinst_headers += util/testutil.h

lib_ltlibraries = libhyperleveldb.la

libhyperleveldb_la_sources =
libhyperleveldb_la_sources += db/builder.cc
libhyperleveldb_la_sources += db/c.cc
libhyperleveldb_la_sources += db/dbformat.cc
libhyperleveldb_la_sources += db/db_impl.cc
libhyperleveldb_la_sources += db/db_iter.cc
libhyperleveldb_la_sources += db/filename.cc
libhyperleveldb_la_sources += db/log_reader.cc
libhyperleveldb_la_sources += db/log_writer.cc
libhyperleveldb_la_sources += db/memtable.cc
libhyperleveldb_la_sources += db/repair.cc
libhyperleveldb_la_sources += db/replay_iterator.cc
libhyperleveldb_la_sources += db/table_cache.cc
libhyperleveldb_la_sources += db/version_edit.cc
libhyperleveldb_la_sources += db/version_set.cc
libhyperleveldb_la_sources += db/write_batch.cc
libhyperleveldb_la_sources += table/block_builder.cc
libhyperleveldb_la_sources += table/block.cc
libhyperleveldb_la_sources += table/filter_block.cc
libhyperleveldb_la_sources += table/format.cc
libhyperleveldb_la_sources += table/iterator.cc
libhyperleveldb_la_sources += table/merger.cc
libhyperleveldb_la_sources += table/table_builder.cc
libhyperleveldb_la_sources += table/table.cc
libhyperleveldb_la_sources += table/two_level_iterator.cc
libhyperleveldb_la_sources += util/arena.cc
libhyperleveldb_la_sources += util/bloom.cc
libhyperleveldb_la_sources += util/cache.cc
libhyperleveldb_la_sources += util/coding.cc
libhyperleveldb_la_sources += util/comparator.cc
libhyperleveldb_la_sources += util/crc32c.cc
libhyperleveldb_la_sources += util/env.cc
libhyperleveldb_la_sources += util/env_posix.cc
libhyperleveldb_la_sources += util/filter_policy.cc
libhyperleveldb_la_sources += util/hash.cc
libhyperleveldb_la_sources += util/histogram.cc
libhyperleveldb_la_sources += util/logging.cc
libhyperleveldb_la_sources += util/options.cc
libhyperleveldb_la_sources += util/status.cc
libhyperleveldb_la_sources += port/port_posix.cc
libhyperleveldb_la_libadd = $(snappy_libs) -lpthread
libhyperleveldb_la_ldflags = -pthread

testutil = util/testutil.cc
testharness = util/testharness.cc $(testutil)

noinst_programs =
noinst_programs += db_bench
noinst_programs += leveldbutil

extra_programs =
extra_programs += benchmark
extra_programs += db_bench_sqlite3
extra_programs += db_bench_tree_db

check_programs =
check_programs += autocompact_test
check_programs += arena_test
check_programs += bloom_test
check_programs += c_test
check_programs += cache_test
check_programs += coding_test
check_programs += corruption_test
check_programs += crc32c_test
check_programs += db_test
check_programs += dbformat_test
check_programs += env_test
check_programs += filename_test
check_programs += filter_block_test
check_programs += log_test
check_programs += skiplist_test
check_programs += table_test
check_programs += version_edit_test
check_programs += version_set_test
check_programs += write_batch_test
check_programs += issue178_test
check_programs += issue200_test

tests = $(check_programs)

benchmark_sources = benchmark.cc
benchmark_ldadd = libhyperleveldb.la -lpthread -le -lpopt -larmnod -lnumbers
benchmark_ldflags = -no-install

db_bench_sources = db/db_bench.cc $(testutil)
db_bench_ldadd = libhyperleveldb.la -lpthread

db_bench_sqlite3_sources = doc/bench/db_bench_sqlite3.cc $(testutil)
db_bench_sqlite3_ldadd = -lsqlite3

db_bench_tree_db_sources = doc/bench/db_bench_tree_db.cc $(testutil)
db_bench_tree_db_ldadd = -lkyotocabinet

leveldbutil_sources = db/leveldb_main.cc
leveldbutil_ldadd = libhyperleveldb.la -lpthread

autocompact_test_sources = db/autocompact_test.cc $(testharness)
autocompact_test_ldadd = libhyperleveldb.la -lpthread

arena_test_sources = util/arena_test.cc $(testharness)
arena_test_ldadd = libhyperleveldb.la -lpthread

bloom_test_sources = util/bloom_test.cc $(testharness)
bloom_test_ldadd = libhyperleveldb.la -lpthread

c_test_sources = db/c_test.c $(testharness)
c_test_ldadd = libhyperleveldb.la -lpthread

cache_test_sources = util/cache_test.cc $(testharness)
cache_test_ldadd = libhyperleveldb.la -lpthread

coding_test_sources = util/coding_test.cc $(testharness)
coding_test_ldadd = libhyperleveldb.la -lpthread

corruption_test_sources = db/corruption_test.cc $(testharness)
corruption_test_ldadd = libhyperleveldb.la -lpthread

crc32c_test_sources = util/crc32c_test.cc $(testharness)
crc32c_test_ldadd = libhyperleveldb.la -lpthread

db_test_sources = db/db_test.cc $(testharness)
db_test_ldadd = libhyperleveldb.la -lpthread

dbformat_test_sources = db/dbformat_test.cc $(testharness)
dbformat_test_ldadd = libhyperleveldb.la -lpthread

env_test_sources = util/env_test.cc $(testharness)
env_test_ldadd = libhyperleveldb.la -lpthread

filename_test_sources = db/filename_test.cc $(testharness)
filename_test_ldadd = libhyperleveldb.la -lpthread

filter_block_test_sources = table/filter_block_test.cc $(testharness)
filter_block_test_ldadd = libhyperleveldb.la -lpthread

log_test_sources = db/log_test.cc $(testharness)
log_test_ldadd = libhyperleveldb.la -lpthread

table_test_sources = table/table_test.cc $(testharness)
table_test_ldadd = libhyperleveldb.la -lpthread

skiplist_test_sources = db/skiplist_test.cc $(testharness)
skiplist_test_ldadd = libhyperleveldb.la -lpthread

version_edit_test_sources = db/version_edit_test.cc $(testharness)
version_edit_test_ldadd = libhyperleveldb.la -lpthread

version_set_test_sources = db/version_set_test.cc $(testharness)
version_set_test_ldadd = libhyperleveldb.la -lpthread

write_batch_test_sources = db/write_batch_test.cc $(testharness)
write_batch_test_ldadd = libhyperleveldb.la -lpthread

issue178_test_sources = issues/issue178_test.cc $(testharness)
issue178_test_ldadd = libhyperleveldb.la -lpthread

issue200_test_sources = issues/issue200_test.cc $(testharness)
issue200_test_ldadd = libhyperleveldb.la -lpthread
