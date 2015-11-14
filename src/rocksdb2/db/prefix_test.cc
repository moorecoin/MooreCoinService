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

#include <algorithm>
#include <iostream>
#include <vector>

#include <gflags/gflags.h>
#include "rocksdb/comparator.h"
#include "rocksdb/db.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/memtablerep.h"
#include "util/histogram.h"
#include "util/stop_watch.h"
#include "util/testharness.h"

using gflags::parsecommandlineflags;

define_bool(trigger_deadlock, false,
            "issue delete in range scan to trigger prefixhashmap deadlock");
define_uint64(bucket_count, 100000, "number of buckets");
define_uint64(num_locks, 10001, "number of locks");
define_bool(random_prefix, false, "randomize prefix");
define_uint64(total_prefixes, 100000, "total number of prefixes");
define_uint64(items_per_prefix, 1, "total number of values per prefix");
define_int64(write_buffer_size, 33554432, "");
define_int64(max_write_buffer_number, 2, "");
define_int64(min_write_buffer_number_to_merge, 1, "");
define_int32(skiplist_height, 4, "");
define_int32(memtable_prefix_bloom_bits, 10000000, "");
define_int32(memtable_prefix_bloom_probes, 10, "");
define_int32(memtable_prefix_bloom_huge_page_tlb_size, 2 * 1024 * 1024, "");
define_int32(value_size, 40, "");

// path to the database on file system
const std::string kdbname = rocksdb::test::tmpdir() + "/prefix_test";

namespace rocksdb {

struct testkey {
  uint64_t prefix;
  uint64_t sorted;

  testkey(uint64_t prefix, uint64_t sorted) : prefix(prefix), sorted(sorted) {}
};

// return a slice backed by test_key
inline slice testkeytoslice(const testkey& test_key) {
  return slice((const char*)&test_key, sizeof(test_key));
}

inline const testkey* slicetotestkey(const slice& slice) {
  return (const testkey*)slice.data();
}

class testkeycomparator : public comparator {
 public:

  // compare needs to be aware of the possibility of a and/or b is
  // prefix only
  virtual int compare(const slice& a, const slice& b) const {
    const testkey* key_a = slicetotestkey(a);
    const testkey* key_b = slicetotestkey(b);
    if (key_a->prefix != key_b->prefix) {
      if (key_a->prefix < key_b->prefix) return -1;
      if (key_a->prefix > key_b->prefix) return 1;
    } else {
      assert_true(key_a->prefix == key_b->prefix);
      // note, both a and b could be prefix only
      if (a.size() != b.size()) {
        // one of them is prefix
        assert_true(
          (a.size() == sizeof(uint64_t) && b.size() == sizeof(testkey)) ||
          (b.size() == sizeof(uint64_t) && a.size() == sizeof(testkey)));
        if (a.size() < b.size()) return -1;
        if (a.size() > b.size()) return 1;
      } else {
        // both a and b are prefix
        if (a.size() == sizeof(uint64_t)) {
          return 0;
        }

        // both a and b are whole key
        assert_true(a.size() == sizeof(testkey) && b.size() == sizeof(testkey));
        if (key_a->sorted < key_b->sorted) return -1;
        if (key_a->sorted > key_b->sorted) return 1;
        if (key_a->sorted == key_b->sorted) return 0;
      }
    }
    return 0;
  }

  virtual const char* name() const override {
    return "testkeycomparator";
  }

  virtual void findshortestseparator(
      std::string* start,
      const slice& limit) const {
  }

  virtual void findshortsuccessor(std::string* key) const {}

};

namespace {
void putkey(db* db, writeoptions write_options, uint64_t prefix,
            uint64_t suffix, const slice& value) {
  testkey test_key(prefix, suffix);
  slice key = testkeytoslice(test_key);
  assert_ok(db->put(write_options, key, value));
}

void seekiterator(iterator* iter, uint64_t prefix, uint64_t suffix) {
  testkey test_key(prefix, suffix);
  slice key = testkeytoslice(test_key);
  iter->seek(key);
}

const std::string knotfoundresult = "not_found";

std::string get(db* db, const readoptions& read_options, uint64_t prefix,
                uint64_t suffix) {
  testkey test_key(prefix, suffix);
  slice key = testkeytoslice(test_key);

  std::string result;
  status s = db->get(read_options, key, &result);
  if (s.isnotfound()) {
    result = knotfoundresult;
  } else if (!s.ok()) {
    result = s.tostring();
  }
  return result;
}
}  // namespace

class prefixtest {
 public:
  std::shared_ptr<db> opendb() {
    db* db;

    options.create_if_missing = true;
    options.write_buffer_size = flags_write_buffer_size;
    options.max_write_buffer_number = flags_max_write_buffer_number;
    options.min_write_buffer_number_to_merge =
      flags_min_write_buffer_number_to_merge;

    options.memtable_prefix_bloom_bits = flags_memtable_prefix_bloom_bits;
    options.memtable_prefix_bloom_probes = flags_memtable_prefix_bloom_probes;
    options.memtable_prefix_bloom_huge_page_tlb_size =
        flags_memtable_prefix_bloom_huge_page_tlb_size;

    status s = db::open(options, kdbname,  &db);
    assert_ok(s);
    return std::shared_ptr<db>(db);
  }

  void firstoption() {
    option_config_ = kbegin;
  }

  bool nextoptions(int bucket_count) {
    // skip some options
    option_config_++;
    if (option_config_ < kend) {
      options.prefix_extractor.reset(newfixedprefixtransform(8));
      switch(option_config_) {
        case khashskiplist:
          options.memtable_factory.reset(
              newhashskiplistrepfactory(bucket_count, flags_skiplist_height));
          return true;
        case khashlinklist:
          options.memtable_factory.reset(
              newhashlinklistrepfactory(bucket_count));
          return true;
        case khashlinklisthugepagetlb:
          options.memtable_factory.reset(
              newhashlinklistrepfactory(bucket_count, 2 * 1024 * 1024));
          return true;
        case khashlinklisttriggerskiplist:
          options.memtable_factory.reset(
              newhashlinklistrepfactory(bucket_count, 0, 3));
          return true;
        default:
          return false;
      }
    }
    return false;
  }

  prefixtest() : option_config_(kbegin) {
    options.comparator = new testkeycomparator();
  }
  ~prefixtest() {
    delete options.comparator;
  }
 protected:
  enum optionconfig {
    kbegin,
    khashskiplist,
    khashlinklist,
    khashlinklisthugepagetlb,
    khashlinklisttriggerskiplist,
    kend
  };
  int option_config_;
  options options;
};

test(prefixtest, testresult) {
  for (int num_buckets = 1; num_buckets <= 2; num_buckets++) {
    firstoption();
    while (nextoptions(num_buckets)) {
      std::cout << "*** mem table: " << options.memtable_factory->name()
                << " number of buckets: " << num_buckets
                << std::endl;
      destroydb(kdbname, options());
      auto db = opendb();
      writeoptions write_options;
      readoptions read_options;

      // 1. insert one row.
      slice v16("v16");
      putkey(db.get(), write_options, 1, 6, v16);
      std::unique_ptr<iterator> iter(db->newiterator(read_options));
      seekiterator(iter.get(), 1, 6);
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      iter->next();
      assert_true(!iter->valid());

      seekiterator(iter.get(), 2, 0);
      assert_true(!iter->valid());

      assert_eq(v16.tostring(), get(db.get(), read_options, 1, 6));
      assert_eq(knotfoundresult, get(db.get(), read_options, 1, 5));
      assert_eq(knotfoundresult, get(db.get(), read_options, 1, 7));
      assert_eq(knotfoundresult, get(db.get(), read_options, 0, 6));
      assert_eq(knotfoundresult, get(db.get(), read_options, 2, 6));

      // 2. insert an entry for the same prefix as the last entry in the bucket.
      slice v17("v17");
      putkey(db.get(), write_options, 1, 7, v17);
      iter.reset(db->newiterator(read_options));
      seekiterator(iter.get(), 1, 7);
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      seekiterator(iter.get(), 1, 6);
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v17 == iter->value());
      iter->next();
      assert_true(!iter->valid());

      seekiterator(iter.get(), 2, 0);
      assert_true(!iter->valid());

      // 3. insert an entry for the same prefix as the head of the bucket.
      slice v15("v15");
      putkey(db.get(), write_options, 1, 5, v15);
      iter.reset(db->newiterator(read_options));

      seekiterator(iter.get(), 1, 7);
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v15 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v15 == iter->value());

      assert_eq(v15.tostring(), get(db.get(), read_options, 1, 5));
      assert_eq(v16.tostring(), get(db.get(), read_options, 1, 6));
      assert_eq(v17.tostring(), get(db.get(), read_options, 1, 7));

      // 4. insert an entry with a larger prefix
      slice v22("v22");
      putkey(db.get(), write_options, 2, 2, v22);
      iter.reset(db->newiterator(read_options));

      seekiterator(iter.get(), 2, 2);
      assert_true(iter->valid());
      assert_true(v22 == iter->value());
      seekiterator(iter.get(), 2, 0);
      assert_true(iter->valid());
      assert_true(v22 == iter->value());

      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v15 == iter->value());

      seekiterator(iter.get(), 1, 7);
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      // 5. insert an entry with a smaller prefix
      slice v02("v02");
      putkey(db.get(), write_options, 0, 2, v02);
      iter.reset(db->newiterator(read_options));

      seekiterator(iter.get(), 0, 2);
      assert_true(iter->valid());
      assert_true(v02 == iter->value());
      seekiterator(iter.get(), 0, 0);
      assert_true(iter->valid());
      assert_true(v02 == iter->value());

      seekiterator(iter.get(), 2, 0);
      assert_true(iter->valid());
      assert_true(v22 == iter->value());

      seekiterator(iter.get(), 1, 5);
      assert_true(iter->valid());
      assert_true(v15 == iter->value());

      seekiterator(iter.get(), 1, 7);
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      // 6. insert to the beginning and the end of the first prefix
      slice v13("v13");
      slice v18("v18");
      putkey(db.get(), write_options, 1, 3, v13);
      putkey(db.get(), write_options, 1, 8, v18);
      iter.reset(db->newiterator(read_options));
      seekiterator(iter.get(), 1, 7);
      assert_true(iter->valid());
      assert_true(v17 == iter->value());

      seekiterator(iter.get(), 1, 3);
      assert_true(iter->valid());
      assert_true(v13 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v15 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v16 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v17 == iter->value());
      iter->next();
      assert_true(iter->valid());
      assert_true(v18 == iter->value());

      seekiterator(iter.get(), 0, 0);
      assert_true(iter->valid());
      assert_true(v02 == iter->value());

      seekiterator(iter.get(), 2, 0);
      assert_true(iter->valid());
      assert_true(v22 == iter->value());

      assert_eq(v22.tostring(), get(db.get(), read_options, 2, 2));
      assert_eq(v02.tostring(), get(db.get(), read_options, 0, 2));
      assert_eq(v13.tostring(), get(db.get(), read_options, 1, 3));
      assert_eq(v15.tostring(), get(db.get(), read_options, 1, 5));
      assert_eq(v16.tostring(), get(db.get(), read_options, 1, 6));
      assert_eq(v17.tostring(), get(db.get(), read_options, 1, 7));
      assert_eq(v18.tostring(), get(db.get(), read_options, 1, 8));
    }
  }
}

test(prefixtest, dynamicprefixiterator) {
  while (nextoptions(flags_bucket_count)) {
    std::cout << "*** mem table: " << options.memtable_factory->name()
        << std::endl;
    destroydb(kdbname, options());
    auto db = opendb();
    writeoptions write_options;
    readoptions read_options;

    std::vector<uint64_t> prefixes;
    for (uint64_t i = 0; i < flags_total_prefixes; ++i) {
      prefixes.push_back(i);
    }

    if (flags_random_prefix) {
      std::random_shuffle(prefixes.begin(), prefixes.end());
    }

    histogramimpl hist_put_time;
    histogramimpl hist_put_comparison;

    // insert x random prefix, each with y continuous element.
    for (auto prefix : prefixes) {
       for (uint64_t sorted = 0; sorted < flags_items_per_prefix; sorted++) {
        testkey test_key(prefix, sorted);

        slice key = testkeytoslice(test_key);
        std::string value(flags_value_size, 0);

        perf_context.reset();
        stopwatchnano timer(env::default(), true);
        assert_ok(db->put(write_options, key, value));
        hist_put_time.add(timer.elapsednanos());
        hist_put_comparison.add(perf_context.user_key_comparison_count);
      }
    }

    std::cout << "put key comparison: \n" << hist_put_comparison.tostring()
              << "put time: \n" << hist_put_time.tostring();

    // test seek existing keys
    histogramimpl hist_seek_time;
    histogramimpl hist_seek_comparison;

    std::unique_ptr<iterator> iter(db->newiterator(read_options));

    for (auto prefix : prefixes) {
      testkey test_key(prefix, flags_items_per_prefix / 2);
      slice key = testkeytoslice(test_key);
      std::string value = "v" + std::to_string(0);

      perf_context.reset();
      stopwatchnano timer(env::default(), true);
      auto key_prefix = options.prefix_extractor->transform(key);
      uint64_t total_keys = 0;
      for (iter->seek(key);
           iter->valid() && iter->key().starts_with(key_prefix);
           iter->next()) {
        if (flags_trigger_deadlock) {
          std::cout << "behold the deadlock!\n";
          db->delete(write_options, iter->key());
        }
        total_keys++;
      }
      hist_seek_time.add(timer.elapsednanos());
      hist_seek_comparison.add(perf_context.user_key_comparison_count);
      assert_eq(total_keys, flags_items_per_prefix - flags_items_per_prefix/2);
    }

    std::cout << "seek key comparison: \n"
              << hist_seek_comparison.tostring()
              << "seek time: \n"
              << hist_seek_time.tostring();

    // test non-existing keys
    histogramimpl hist_no_seek_time;
    histogramimpl hist_no_seek_comparison;

    for (auto prefix = flags_total_prefixes;
         prefix < flags_total_prefixes + 10000;
         prefix++) {
      testkey test_key(prefix, 0);
      slice key = testkeytoslice(test_key);

      perf_context.reset();
      stopwatchnano timer(env::default(), true);
      iter->seek(key);
      hist_no_seek_time.add(timer.elapsednanos());
      hist_no_seek_comparison.add(perf_context.user_key_comparison_count);
      assert_true(!iter->valid());
    }

    std::cout << "non-existing seek key comparison: \n"
              << hist_no_seek_comparison.tostring()
              << "non-existing seek time: \n"
              << hist_no_seek_time.tostring();
  }
}

}

int main(int argc, char** argv) {
  parsecommandlineflags(&argc, &argv, true);
  std::cout << kdbname << "\n";

  rocksdb::test::runalltests();
  return 0;
}

#endif  // gflags
