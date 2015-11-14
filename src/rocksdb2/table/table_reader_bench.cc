//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#ifndef gflags
#include <cstdio>
int main() {
  fprintf(stderr, "please install gflags to run rocksdb tools\n");
  return 1;
}
#else

#include <gflags/gflags.h>

#include "rocksdb/db.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/table.h"
#include "db/db_impl.h"
#include "db/dbformat.h"
#include "port/atomic_pointer.h"
#include "table/block_based_table_factory.h"
#include "table/plain_table_factory.h"
#include "table/table_builder.h"
#include "util/histogram.h"
#include "util/testharness.h"
#include "util/testutil.h"

using gflags::parsecommandlineflags;
using gflags::setusagemessage;

namespace rocksdb {

namespace {
// make a key that i determines the first 4 characters and j determines the
// last 4 characters.
static std::string makekey(int i, int j, bool through_db) {
  char buf[100];
  snprintf(buf, sizeof(buf), "%04d__key___%04d", i, j);
  if (through_db) {
    return std::string(buf);
  }
  // if we directly query table, which operates on internal keys
  // instead of user keys, we need to add 8 bytes of internal
  // information (row type etc) to user key to make an internal
  // key.
  internalkey key(std::string(buf), 0, valuetype::ktypevalue);
  return key.encode().tostring();
}

static bool dummysavevalue(void* arg, const parsedinternalkey& ikey,
                           const slice& v) {
  return false;
}

uint64_t now(env* env, bool measured_by_nanosecond) {
  return measured_by_nanosecond ? env->nownanos() : env->nowmicros();
}
}  // namespace

// a very simple benchmark that.
// create a table with roughly numkey1 * numkey2 keys,
// where there are numkey1 prefixes of the key, each has numkey2 number of
// distinguished key, differing in the suffix part.
// if if_query_empty_keys = false, query the existing keys numkey1 * numkey2
// times randomly.
// if if_query_empty_keys = true, query numkey1 * numkey2 random empty keys.
// print out the total time.
// if through_db=true, a full db will be created and queries will be against
// it. otherwise, operations will be directly through table level.
//
// if for_terator=true, instead of just query one key each time, it queries
// a range sharing the same prefix.
namespace {
void tablereaderbenchmark(options& opts, envoptions& env_options,
                          readoptions& read_options, int num_keys1,
                          int num_keys2, int num_iter, int prefix_len,
                          bool if_query_empty_keys, bool for_iterator,
                          bool through_db, bool measured_by_nanosecond) {
  rocksdb::internalkeycomparator ikc(opts.comparator);

  std::string file_name = test::tmpdir()
      + "/rocksdb_table_reader_benchmark";
  std::string dbname = test::tmpdir() + "/rocksdb_table_reader_bench_db";
  writeoptions wo;
  unique_ptr<writablefile> file;
  env* env = env::default();
  tablebuilder* tb = nullptr;
  db* db = nullptr;
  status s;
  if (!through_db) {
    env->newwritablefile(file_name, &file, env_options);
    tb = opts.table_factory->newtablebuilder(opts, ikc, file.get(),
                                             compressiontype::knocompression);
  } else {
    s = db::open(opts, dbname, &db);
    assert_ok(s);
    assert_true(db != nullptr);
  }
  // populate slightly more than 1m keys
  for (int i = 0; i < num_keys1; i++) {
    for (int j = 0; j < num_keys2; j++) {
      std::string key = makekey(i * 2, j, through_db);
      if (!through_db) {
        tb->add(key, key);
      } else {
        db->put(wo, key, key);
      }
    }
  }
  if (!through_db) {
    tb->finish();
    file->close();
  } else {
    db->flush(flushoptions());
  }

  unique_ptr<tablereader> table_reader;
  unique_ptr<randomaccessfile> raf;
  if (!through_db) {
    status s = env->newrandomaccessfile(file_name, &raf, env_options);
    uint64_t file_size;
    env->getfilesize(file_name, &file_size);
    s = opts.table_factory->newtablereader(
        opts, env_options, ikc, std::move(raf), file_size, &table_reader);
  }

  random rnd(301);
  std::string result;
  histogramimpl hist;

  void* arg = nullptr;
  for (int it = 0; it < num_iter; it++) {
    for (int i = 0; i < num_keys1; i++) {
      for (int j = 0; j < num_keys2; j++) {
        int r1 = rnd.uniform(num_keys1) * 2;
        int r2 = rnd.uniform(num_keys2);
        if (if_query_empty_keys) {
          r1++;
          r2 = num_keys2 * 2 - r2;
        }

        if (!for_iterator) {
          // query one existing key;
          std::string key = makekey(r1, r2, through_db);
          uint64_t start_time = now(env, measured_by_nanosecond);
          if (!through_db) {
            s = table_reader->get(read_options, key, arg, dummysavevalue,
                                  nullptr);
          } else {
            s = db->get(read_options, key, &result);
          }
          hist.add(now(env, measured_by_nanosecond) - start_time);
        } else {
          int r2_len;
          if (if_query_empty_keys) {
            r2_len = 0;
          } else {
            r2_len = rnd.uniform(num_keys2) + 1;
            if (r2_len + r2 > num_keys2) {
              r2_len = num_keys2 - r2;
            }
          }
          std::string start_key = makekey(r1, r2, through_db);
          std::string end_key = makekey(r1, r2 + r2_len, through_db);
          uint64_t total_time = 0;
          uint64_t start_time = now(env, measured_by_nanosecond);
          iterator* iter;
          if (!through_db) {
            iter = table_reader->newiterator(read_options);
          } else {
            iter = db->newiterator(read_options);
          }
          int count = 0;
          for(iter->seek(start_key); iter->valid(); iter->next()) {
            if (if_query_empty_keys) {
              break;
            }
            // verify key;
            total_time += now(env, measured_by_nanosecond) - start_time;
            assert(slice(makekey(r1, r2 + count, through_db)) == iter->key());
            start_time = now(env, measured_by_nanosecond);
            if (++count >= r2_len) {
              break;
            }
          }
          if (count != r2_len) {
            fprintf(
                stderr, "iterator cannot iterate expected number of entries. "
                "expected %d but got %d\n", r2_len, count);
            assert(false);
          }
          delete iter;
          total_time += now(env, measured_by_nanosecond) - start_time;
          hist.add(total_time);
        }
      }
    }
  }

  fprintf(
      stderr,
      "==================================================="
      "====================================================\n"
      "inmemorytablesimplebenchmark: %20s   num_key1:  %5d   "
      "num_key2: %5d  %10s\n"
      "==================================================="
      "===================================================="
      "\nhistogram (unit: %s): \n%s",
      opts.table_factory->name(), num_keys1, num_keys2,
      for_iterator ? "iterator" : (if_query_empty_keys ? "empty" : "non_empty"),
      measured_by_nanosecond ? "nanosecond" : "microsecond",
      hist.tostring().c_str());
  if (!through_db) {
    env->deletefile(file_name);
  } else {
    delete db;
    db = nullptr;
    destroydb(dbname, opts);
  }
}
}  // namespace
}  // namespace rocksdb

define_bool(query_empty, false, "query non-existing keys instead of existing "
            "ones.");
define_int32(num_keys1, 4096, "number of distinguish prefix of keys");
define_int32(num_keys2, 512, "number of distinguish keys for each prefix");
define_int32(iter, 3, "query non-existing keys instead of existing ones");
define_int32(prefix_len, 16, "prefix length used for iterators and indexes");
define_bool(iterator, false, "for test iterator");
define_bool(through_db, false, "if enable, a db instance will be created and "
            "the query will be against db. otherwise, will be directly against "
            "a table reader.");
define_string(table_factory, "block_based",
              "table factory to use: `block_based` (default), `plain_table` or "
              "`cuckoo_hash`.");
define_string(time_unit, "microsecond",
              "the time unit used for measuring performance. user can specify "
              "`microsecond` (default) or `nanosecond`");

int main(int argc, char** argv) {
  setusagemessage(std::string("\nusage:\n") + std::string(argv[0]) +
                  " [options]...");
  parsecommandlineflags(&argc, &argv, true);

  std::shared_ptr<rocksdb::tablefactory> tf;
  rocksdb::options options;
  if (flags_prefix_len < 16) {
    options.prefix_extractor.reset(rocksdb::newfixedprefixtransform(
        flags_prefix_len));
  }
  rocksdb::readoptions ro;
  rocksdb::envoptions env_options;
  options.create_if_missing = true;
  options.compression = rocksdb::compressiontype::knocompression;

  if (flags_table_factory == "cuckoo_hash") {
    options.allow_mmap_reads = true;
    env_options.use_mmap_reads = true;

    tf.reset(rocksdb::newcuckootablefactory(0.75));
  } else if (flags_table_factory == "plain_table") {
    options.allow_mmap_reads = true;
    env_options.use_mmap_reads = true;

    rocksdb::plaintableoptions plain_table_options;
    plain_table_options.user_key_len = 16;
    plain_table_options.bloom_bits_per_key = (flags_prefix_len == 16) ? 0 : 8;
    plain_table_options.hash_table_ratio = 0.75;

    tf.reset(new rocksdb::plaintablefactory(plain_table_options));
    options.prefix_extractor.reset(rocksdb::newfixedprefixtransform(
        flags_prefix_len));
  } else if (flags_table_factory == "block_based") {
    tf.reset(new rocksdb::blockbasedtablefactory());
  } else {
    fprintf(stderr, "invalid table type %s\n", flags_table_factory.c_str());
  }

  if (tf) {
    // if user provides invalid options, just fall back to microsecond.
    bool measured_by_nanosecond = flags_time_unit == "nanosecond";

    options.table_factory = tf;
    rocksdb::tablereaderbenchmark(options, env_options, ro, flags_num_keys1,
                                  flags_num_keys2, flags_iter, flags_prefix_len,
                                  flags_query_empty, flags_iterator,
                                  flags_through_db, measured_by_nanosecond);
  } else {
    return 1;
  }

  return 0;
}

#endif  // gflags
