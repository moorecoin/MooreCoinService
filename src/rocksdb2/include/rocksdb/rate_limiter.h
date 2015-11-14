//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once

#include "rocksdb/env.h"

namespace rocksdb {

class ratelimiter {
 public:
  virtual ~ratelimiter() {}

  // request for token to write bytes. if this request can not be satisfied,
  // the call is blocked. caller is responsible to make sure
  // bytes < getsingleburstbytes()
  virtual void request(const int64_t bytes, const env::iopriority pri) = 0;

  // max bytes can be granted in a single burst
  virtual int64_t getsingleburstbytes() const = 0;

  // total bytes that go though rate limiter
  virtual int64_t gettotalbytesthrough(
      const env::iopriority pri = env::io_total) const = 0;

  // total # of requests that go though rate limiter
  virtual int64_t gettotalrequests(
      const env::iopriority pri = env::io_total) const = 0;
};

// create a ratelimiter object, which can be shared among rocksdb instances to
// control write rate of flush and compaction.
// @rate_bytes_per_sec: this is the only parameter you want to set most of the
// time. it controls the total write rate of compaction and flush in bytes per
// second. currently, rocksdb does not enforce rate limit for anything other
// than flush and compaction, e.g. write to wal.
// @refill_period_us: this controls how often tokens are refilled. for example,
// when rate_bytes_per_sec is set to 10mb/s and refill_period_us is set to
// 100ms, then 1mb is refilled every 100ms internally. larger value can lead to
// burstier writes while smaller value introduces more cpu overhead.
// the default should work for most cases.
// @fairness: ratelimiter accepts high-pri requests and low-pri requests.
// a low-pri request is usually blocked in favor of hi-pri request. currently,
// rocksdb assigns low-pri to request from compaciton and high-pri to request
// from flush. low-pri requests can get blocked if flush requests come in
// continuouly. this fairness parameter grants low-pri requests permission by
// 1/fairness chance even though high-pri requests exist to avoid starvation.
// you should be good by leaving it at default 10.
extern ratelimiter* newgenericratelimiter(
    int64_t rate_bytes_per_sec,
    int64_t refill_period_us = 100 * 1000,
    int32_t fairness = 10);

}  // namespace rocksdb
