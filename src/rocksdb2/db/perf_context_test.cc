//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include <algorithm>
#include <iostream>
#include <vector>
#include "/usr/include/valgrind/callgrind.h"

#include "rocksdb/db.h"
#include "rocksdb/perf_context.h"
#include "rocksdb/slice_transform.h"
#include "rocksdb/memtablerep.h"
#include "util/histogram.h"
#include "util/stop_watch.h"
#include "util/testharness.h"


bool flags_random_key = false;
bool flags_use_set_based_memetable = false;
int flags_total_keys = 100;
int flags_write_buffer_size = 1000000000;
int flags_max_write_buffer_number = 8;
int flags_min_write_buffer_number_to_merge = 7;

// path to the database on file system
const std::string kdbname = rocksdb::test::tmpdir() + "/perf_context_test";

namespace rocksdb {

std::shared_ptr<db> opendb() {
    db* db;
    options options;
    options.create_if_missing = true;
    options.write_buffer_size = flags_write_buffer_size;
    options.max_write_buffer_number = flags_max_write_buffer_number;
    options.min_write_buffer_number_to_merge =
      flags_min_write_buffer_number_to_merge;

    if (flags_use_set_based_memetable) {
      auto prefix_extractor = rocksdb::newfixedprefixtransform(0);
      options.memtable_factory.reset(
          newhashskiplistrepfactory(prefix_extractor));
    }

    status s = db::open(options, kdbname,  &db);
    assert_ok(s);
    return std::shared_ptr<db>(db);
}

class perfcontexttest { };

test(perfcontexttest, seekintodeletion) {
  destroydb(kdbname, options());
  auto db = opendb();
  writeoptions write_options;
  readoptions read_options;

  for (int i = 0; i < flags_total_keys; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string value = "v" + std::to_string(i);

    db->put(write_options, key, value);
  }

  for (int i = 0; i < flags_total_keys -1 ; ++i) {
    std::string key = "k" + std::to_string(i);
    db->delete(write_options, key);
  }

  histogramimpl hist_get;
  histogramimpl hist_get_time;
  for (int i = 0; i < flags_total_keys - 1; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string value;

    perf_context.reset();
    stopwatchnano timer(env::default(), true);
    auto status = db->get(read_options, key, &value);
    auto elapsed_nanos = timer.elapsednanos();
    assert_true(status.isnotfound());
    hist_get.add(perf_context.user_key_comparison_count);
    hist_get_time.add(elapsed_nanos);
  }

  std::cout << "get uesr key comparison: \n" << hist_get.tostring()
            << "get time: \n" << hist_get_time.tostring();

  histogramimpl hist_seek_to_first;
  std::unique_ptr<iterator> iter(db->newiterator(read_options));

  perf_context.reset();
  stopwatchnano timer(env::default(), true);
  iter->seektofirst();
  hist_seek_to_first.add(perf_context.user_key_comparison_count);
  auto elapsed_nanos = timer.elapsednanos();

  std::cout << "seektofirst uesr key comparison: \n" << hist_seek_to_first.tostring()
            << "ikey skipped: " << perf_context.internal_key_skipped_count << "\n"
            << "idelete skipped: " << perf_context.internal_delete_skipped_count << "\n"
            << "elapsed: " << elapsed_nanos << "\n";

  histogramimpl hist_seek;
  for (int i = 0; i < flags_total_keys; ++i) {
    std::unique_ptr<iterator> iter(db->newiterator(read_options));
    std::string key = "k" + std::to_string(i);

    perf_context.reset();
    stopwatchnano timer(env::default(), true);
    iter->seek(key);
    auto elapsed_nanos = timer.elapsednanos();
    hist_seek.add(perf_context.user_key_comparison_count);
    std::cout << "seek cmp: " << perf_context.user_key_comparison_count
              << " ikey skipped " << perf_context.internal_key_skipped_count
              << " idelete skipped " << perf_context.internal_delete_skipped_count
              << " elapsed: " << elapsed_nanos << "ns\n";

    perf_context.reset();
    assert_true(iter->valid());
    stopwatchnano timer2(env::default(), true);
    iter->next();
    auto elapsed_nanos2 = timer2.elapsednanos();
    std::cout << "next cmp: " << perf_context.user_key_comparison_count
              << "elapsed: " << elapsed_nanos2 << "ns\n";
  }

  std::cout << "seek uesr key comparison: \n" << hist_seek.tostring();
}

test(perfcontexttest, stopwatchnanooverhead) {
  // profile the timer cost by itself!
  const int ktotaliterations = 1000000;
  std::vector<uint64_t> timings(ktotaliterations);

  stopwatchnano timer(env::default(), true);
  for (auto& timing : timings) {
    timing = timer.elapsednanos(true /* reset */);
  }

  histogramimpl histogram;
  for (const auto timing : timings) {
    histogram.add(timing);
  }

  std::cout << histogram.tostring();
}

test(perfcontexttest, stopwatchoverhead) {
  // profile the timer cost by itself!
  const int ktotaliterations = 1000000;
  std::vector<uint64_t> timings(ktotaliterations);

  stopwatch timer(env::default());
  for (auto& timing : timings) {
    timing = timer.elapsedmicros();
  }

  histogramimpl histogram;
  uint64_t prev_timing = 0;
  for (const auto timing : timings) {
    histogram.add(timing - prev_timing);
    prev_timing = timing;
  }

  std::cout << histogram.tostring();
}

void profilekeycomparison() {
  destroydb(kdbname, options());    // start this test with a fresh db

  auto db = opendb();

  writeoptions write_options;
  readoptions read_options;

  histogramimpl hist_put;
  histogramimpl hist_get;
  histogramimpl hist_get_snapshot;
  histogramimpl hist_get_memtable;
  histogramimpl hist_get_post_process;
  histogramimpl hist_num_memtable_checked;
  histogramimpl hist_write_pre_post;
  histogramimpl hist_write_wal_time;
  histogramimpl hist_write_memtable_time;

  std::cout << "inserting " << flags_total_keys << " key/value pairs\n...\n";

  std::vector<int> keys;
  for (int i = 0; i < flags_total_keys; ++i) {
    keys.push_back(i);
  }

  if (flags_random_key) {
    std::random_shuffle(keys.begin(), keys.end());
  }

  for (const int i : keys) {
    std::string key = "k" + std::to_string(i);
    std::string value = "v" + std::to_string(i);

    perf_context.reset();
    db->put(write_options, key, value);
    hist_write_pre_post.add(perf_context.write_pre_and_post_process_time);
    hist_write_wal_time.add(perf_context.write_wal_time);
    hist_write_memtable_time.add(perf_context.write_memtable_time);
    hist_put.add(perf_context.user_key_comparison_count);

    perf_context.reset();
    db->get(read_options, key, &value);
    hist_get_snapshot.add(perf_context.get_snapshot_time);
    hist_get_memtable.add(perf_context.get_from_memtable_time);
    hist_num_memtable_checked.add(perf_context.get_from_memtable_count);
    hist_get_post_process.add(perf_context.get_post_process_time);
    hist_get.add(perf_context.user_key_comparison_count);
  }

  std::cout << "put uesr key comparison: \n" << hist_put.tostring()
            << "get uesr key comparison: \n" << hist_get.tostring();
  std::cout << "put(): pre and post process time: \n"
            << hist_write_pre_post.tostring()
            << " writing wal time: \n"
            << hist_write_wal_time.tostring() << "\n"
            << " writing mem table time: \n"
            << hist_write_memtable_time.tostring() << "\n";

  std::cout << "get(): time to get snapshot: \n"
            << hist_get_snapshot.tostring()
            << " time to get value from memtables: \n"
            << hist_get_memtable.tostring() << "\n"
            << " number of memtables checked: \n"
            << hist_num_memtable_checked.tostring() << "\n"
            << " time to post process: \n"
            << hist_get_post_process.tostring() << "\n";
}

test(perfcontexttest, keycomparisoncount) {
  setperflevel(kenablecount);
  profilekeycomparison();

  setperflevel(kdisable);
  profilekeycomparison();

  setperflevel(kenabletime);
  profilekeycomparison();
}

// make perf_context_test
// export rocksdb_tests=perfcontexttest.seekkeycomparison
// for one memtable:
// ./perf_context_test --write_buffer_size=500000 --total_keys=10000
// for two memtables:
// ./perf_context_test --write_buffer_size=250000 --total_keys=10000
// specify --random_key=1 to shuffle the key before insertion
// results show that, for sequential insertion, worst-case seek key comparison
// is close to the total number of keys (linear), when there is only one
// memtable. when there are two memtables, even the avg seek key comparison
// starts to become linear to the input size.

test(perfcontexttest, seekkeycomparison) {
  destroydb(kdbname, options());
  auto db = opendb();
  writeoptions write_options;
  readoptions read_options;

  std::cout << "inserting " << flags_total_keys << " key/value pairs\n...\n";

  std::vector<int> keys;
  for (int i = 0; i < flags_total_keys; ++i) {
    keys.push_back(i);
  }

  if (flags_random_key) {
    std::random_shuffle(keys.begin(), keys.end());
  }

  histogramimpl hist_put_time;
  histogramimpl hist_wal_time;
  histogramimpl hist_time_diff;

  setperflevel(kenabletime);
  stopwatchnano timer(env::default());
  for (const int i : keys) {
    std::string key = "k" + std::to_string(i);
    std::string value = "v" + std::to_string(i);

    perf_context.reset();
    timer.start();
    db->put(write_options, key, value);
    auto put_time = timer.elapsednanos();
    hist_put_time.add(put_time);
    hist_wal_time.add(perf_context.write_wal_time);
    hist_time_diff.add(put_time - perf_context.write_wal_time);
  }

  std::cout << "put time:\n" << hist_put_time.tostring()
            << "wal time:\n" << hist_wal_time.tostring()
            << "time diff:\n" << hist_time_diff.tostring();

  histogramimpl hist_seek;
  histogramimpl hist_next;

  for (int i = 0; i < flags_total_keys; ++i) {
    std::string key = "k" + std::to_string(i);
    std::string value = "v" + std::to_string(i);

    std::unique_ptr<iterator> iter(db->newiterator(read_options));
    perf_context.reset();
    iter->seek(key);
    assert_true(iter->valid());
    assert_eq(iter->value().tostring(), value);
    hist_seek.add(perf_context.user_key_comparison_count);
  }

  std::unique_ptr<iterator> iter(db->newiterator(read_options));
  for (iter->seektofirst(); iter->valid();) {
    perf_context.reset();
    iter->next();
    hist_next.add(perf_context.user_key_comparison_count);
  }

  std::cout << "seek:\n" << hist_seek.tostring()
            << "next:\n" << hist_next.tostring();
}

}

int main(int argc, char** argv) {

  for (int i = 1; i < argc; i++) {
    int n;
    char junk;

    if (sscanf(argv[i], "--write_buffer_size=%d%c", &n, &junk) == 1) {
      flags_write_buffer_size = n;
    }

    if (sscanf(argv[i], "--total_keys=%d%c", &n, &junk) == 1) {
      flags_total_keys = n;
    }

    if (sscanf(argv[i], "--random_key=%d%c", &n, &junk) == 1 &&
        (n == 0 || n == 1)) {
      flags_random_key = n;
    }

    if (sscanf(argv[i], "--use_set_based_memetable=%d%c", &n, &junk) == 1 &&
        (n == 0 || n == 1)) {
      flags_use_set_based_memetable = n;
    }

  }

  std::cout << kdbname << "\n";

  rocksdb::test::runalltests();
  return 0;
}
