//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include <deque>
#include "port/port_posix.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "rocksdb/env.h"
#include "rocksdb/rate_limiter.h"

namespace rocksdb {

class genericratelimiter : public ratelimiter {
 public:
  genericratelimiter(int64_t refill_bytes,
      int64_t refill_period_us, int32_t fairness);

  virtual ~genericratelimiter();

  // request for token to write bytes. if this request can not be satisfied,
  // the call is blocked. caller is responsible to make sure
  // bytes < getsingleburstbytes()
  virtual void request(const int64_t bytes, const env::iopriority pri) override;

  virtual int64_t getsingleburstbytes() const override {
    // const var
    return refill_bytes_per_period_;
  }

  virtual int64_t gettotalbytesthrough(
      const env::iopriority pri = env::io_total) const override {
    mutexlock g(&request_mutex_);
    if (pri == env::io_total) {
      return total_bytes_through_[env::io_low] +
             total_bytes_through_[env::io_high];
    }
    return total_bytes_through_[pri];
  }

  virtual int64_t gettotalrequests(
      const env::iopriority pri = env::io_total) const override {
    mutexlock g(&request_mutex_);
    if (pri == env::io_total) {
      return total_requests_[env::io_low] + total_requests_[env::io_high];
    }
    return total_requests_[pri];
  }

 private:
  void refill();

  // this mutex guard all internal states
  mutable port::mutex request_mutex_;

  const int64_t refill_period_us_;
  const int64_t refill_bytes_per_period_;
  env* const env_;

  bool stop_;
  port::condvar exit_cv_;
  int32_t requests_to_wait_;

  int64_t total_requests_[env::io_total];
  int64_t total_bytes_through_[env::io_total];
  int64_t available_bytes_;
  int64_t next_refill_us_;

  int32_t fairness_;
  random rnd_;

  struct req;
  req* leader_;
  std::deque<req*> queue_[env::io_total];
};

}  // namespace rocksdb
