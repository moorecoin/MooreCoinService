//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#define __stdc_format_macros
#include <inttypes.h>
#include <limits>
#include "util/testharness.h"
#include "util/rate_limiter.h"
#include "util/random.h"
#include "rocksdb/env.h"

namespace rocksdb {

class ratelimitertest {
};

test(ratelimitertest, startstop) {
  std::unique_ptr<ratelimiter> limiter(new genericratelimiter(100, 100, 10));
}

test(ratelimitertest, rate) {
  auto* env = env::default();
  struct arg {
    arg(int64_t target_rate, int burst)
      : limiter(new genericratelimiter(target_rate, 100 * 1000, 10)),
        request_size(target_rate / 10),
        burst(burst) {}
    std::unique_ptr<ratelimiter> limiter;
    int64_t request_size;
    int burst;
  };

  auto writer = [](void* p) {
    auto* env = env::default();
    auto* arg = static_cast<arg*>(p);
    // test for 2 seconds
    auto until = env->nowmicros() + 2 * 1000000;
    random r((uint32_t)(env->nownanos() %
          std::numeric_limits<uint32_t>::max()));
    while (env->nowmicros() < until) {
      for (int i = 0; i < static_cast<int>(r.skewed(arg->burst) + 1); ++i) {
        arg->limiter->request(r.uniform(arg->request_size - 1) + 1,
                              env::io_high);
      }
      arg->limiter->request(r.uniform(arg->request_size - 1) + 1,
                            env::io_low);
    }
  };

  for (int i = 1; i <= 16; i*=2) {
    int64_t target = i * 1024 * 10;
    arg arg(target, i / 4 + 1);
    auto start = env->nowmicros();
    for (int t = 0; t < i; ++t) {
      env->startthread(writer, &arg);
    }
    env->waitforjoin();

    auto elapsed = env->nowmicros() - start;
    double rate = arg.limiter->gettotalbytesthrough()
                  * 1000000.0 / elapsed;
    fprintf(stderr, "request size [1 - %" prii64 "], limit %" prii64
                    " kb/sec, actual rate: %lf kb/sec, elapsed %.2lf seconds\n",
            arg.request_size - 1, target / 1024, rate / 1024,
            elapsed / 1000000.0);

    assert_ge(rate / target, 0.95);
    assert_le(rate / target, 1.05);
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
