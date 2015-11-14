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

#define __stdc_format_macros
#include <inttypes.h>
#include <algorithm>
#include <gflags/gflags.h>

#include "dynamic_bloom.h"
#include "port/port.h"
#include "util/logging.h"
#include "util/testharness.h"
#include "util/testutil.h"
#include "util/stop_watch.h"

using gflags::parsecommandlineflags;

define_int32(bits_per_key, 10, "");
define_int32(num_probes, 6, "");
define_bool(enable_perf, false, "");

namespace rocksdb {

static slice key(uint64_t i, char* buffer) {
  memcpy(buffer, &i, sizeof(i));
  return slice(buffer, sizeof(i));
}

class dynamicbloomtest {
};

test(dynamicbloomtest, emptyfilter) {
  arena arena;
  dynamicbloom bloom1(&arena, 100, 0, 2);
  assert_true(!bloom1.maycontain("hello"));
  assert_true(!bloom1.maycontain("world"));

  dynamicbloom bloom2(&arena, cache_line_size * 8 * 2 - 1, 1, 2);
  assert_true(!bloom2.maycontain("hello"));
  assert_true(!bloom2.maycontain("world"));
}

test(dynamicbloomtest, small) {
  arena arena;
  dynamicbloom bloom1(&arena, 100, 0, 2);
  bloom1.add("hello");
  bloom1.add("world");
  assert_true(bloom1.maycontain("hello"));
  assert_true(bloom1.maycontain("world"));
  assert_true(!bloom1.maycontain("x"));
  assert_true(!bloom1.maycontain("foo"));

  dynamicbloom bloom2(&arena, cache_line_size * 8 * 2 - 1, 1, 2);
  bloom2.add("hello");
  bloom2.add("world");
  assert_true(bloom2.maycontain("hello"));
  assert_true(bloom2.maycontain("world"));
  assert_true(!bloom2.maycontain("x"));
  assert_true(!bloom2.maycontain("foo"));
}

static uint32_t nextnum(uint32_t num) {
  if (num < 10) {
    num += 1;
  } else if (num < 100) {
    num += 10;
  } else if (num < 1000) {
    num += 100;
  } else {
    num += 1000;
  }
  return num;
}

test(dynamicbloomtest, varyinglengths) {
  char buffer[sizeof(uint64_t)];

  // count number of filters that significantly exceed the false positive rate
  int mediocre_filters = 0;
  int good_filters = 0;
  uint32_t num_probes = static_cast<uint32_t>(flags_num_probes);

  fprintf(stderr, "bits_per_key: %d  num_probes: %d\n",
          flags_bits_per_key, num_probes);

  for (uint32_t enable_locality = 0; enable_locality < 2; ++enable_locality) {
    for (uint32_t num = 1; num <= 10000; num = nextnum(num)) {
      uint32_t bloom_bits = 0;
      arena arena;
      if (enable_locality == 0) {
        bloom_bits = std::max(num * flags_bits_per_key, 64u);
      } else {
        bloom_bits = std::max(num * flags_bits_per_key,
                              enable_locality * cache_line_size * 8);
      }
      dynamicbloom bloom(&arena, bloom_bits, enable_locality, num_probes);
      for (uint64_t i = 0; i < num; i++) {
        bloom.add(key(i, buffer));
        assert_true(bloom.maycontain(key(i, buffer)));
      }

      // all added keys must match
      for (uint64_t i = 0; i < num; i++) {
        assert_true(bloom.maycontain(key(i, buffer)))
          << "num " << num << "; key " << i;
      }

      // check false positive rate

      int result = 0;
      for (uint64_t i = 0; i < 10000; i++) {
        if (bloom.maycontain(key(i + 1000000000, buffer))) {
          result++;
        }
      }
      double rate = result / 10000.0;

      fprintf(stderr,
              "false positives: %5.2f%% @ num = %6u, bloom_bits = %6u, "
              "enable locality?%u\n",
              rate * 100.0, num, bloom_bits, enable_locality);

      if (rate > 0.0125)
        mediocre_filters++;  // allowed, but not too often
      else
        good_filters++;
    }

    fprintf(stderr, "filters: %d good, %d mediocre\n",
            good_filters, mediocre_filters);
    assert_le(mediocre_filters, good_filters/5);
  }
}

test(dynamicbloomtest, perf) {
  stopwatchnano timer(env::default());
  uint32_t num_probes = static_cast<uint32_t>(flags_num_probes);

  if (!flags_enable_perf) {
    return;
  }

  for (uint64_t m = 1; m <= 8; ++m) {
    arena arena;
    const uint64_t num_keys = m * 8 * 1024 * 1024;
    fprintf(stderr, "testing %" priu64 "m keys\n", m * 8);

    dynamicbloom std_bloom(&arena, num_keys * 10, 0, num_probes);

    timer.start();
    for (uint64_t i = 1; i <= num_keys; ++i) {
      std_bloom.add(slice(reinterpret_cast<const char*>(&i), 8));
    }

    uint64_t elapsed = timer.elapsednanos();
    fprintf(stderr, "standard bloom, avg add latency %" priu64 "\n",
            elapsed / num_keys);

    uint64_t count = 0;
    timer.start();
    for (uint64_t i = 1; i <= num_keys; ++i) {
      if (std_bloom.maycontain(slice(reinterpret_cast<const char*>(&i), 8))) {
        ++count;
      }
    }
    elapsed = timer.elapsednanos();
    fprintf(stderr, "standard bloom, avg query latency %" priu64 "\n",
            elapsed / count);
    assert_true(count == num_keys);

    // locality enabled version
    dynamicbloom blocked_bloom(&arena, num_keys * 10, 1, num_probes);

      timer.start();
      for (uint64_t i = 1; i <= num_keys; ++i) {
        blocked_bloom.add(slice(reinterpret_cast<const char*>(&i), 8));
      }

      elapsed = timer.elapsednanos();
      fprintf(stderr,
              "blocked bloom(enable locality), avg add latency %" priu64 "\n",
              elapsed / num_keys);

      count = 0;
      timer.start();
      for (uint64_t i = 1; i <= num_keys; ++i) {
        if (blocked_bloom.maycontain(
              slice(reinterpret_cast<const char*>(&i), 8))) {
          ++count;
        }
      }

      elapsed = timer.elapsednanos();
      fprintf(stderr,
              "blocked bloom(enable locality), avg query latency %" priu64 "\n",
              elapsed / count);
      assert_true(count == num_keys);
    }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  parsecommandlineflags(&argc, &argv, true);

  return rocksdb::test::runalltests();
}

#endif  // gflags
