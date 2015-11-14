//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include "rocksdb/env.h"
#include "util/statistics.h"

namespace rocksdb {
// auto-scoped.
// records the measure time into the corresponding histogram if statistics
// is not nullptr. it is also saved into *elapsed if the pointer is not nullptr.
class stopwatch {
 public:
  stopwatch(env * const env, statistics* statistics,
            const uint32_t hist_type,
            uint64_t* elapsed = nullptr)
    : env_(env),
      statistics_(statistics),
      hist_type_(hist_type),
      elapsed_(elapsed),
      stats_enabled_(statistics && statistics->histenabledfortype(hist_type)),
      start_time_((stats_enabled_ || elapsed != nullptr) ?
                  env->nowmicros() : 0) {
  }


  ~stopwatch() {
    if (elapsed_) {
      *elapsed_ = env_->nowmicros() - start_time_;
    }
    if (stats_enabled_) {
      statistics_->measuretime(hist_type_,
          (elapsed_ != nullptr) ? *elapsed_ :
                                  (env_->nowmicros() - start_time_));
    }
  }

 private:
  env* const env_;
  statistics* statistics_;
  const uint32_t hist_type_;
  uint64_t* elapsed_;
  bool stats_enabled_;
  const uint64_t start_time_;
};

// a nano second precision stopwatch
class stopwatchnano {
 public:
  explicit stopwatchnano(env* const env, bool auto_start = false)
      : env_(env), start_(0) {
    if (auto_start) {
      start();
    }
  }

  void start() { start_ = env_->nownanos(); }

  uint64_t elapsednanos(bool reset = false) {
    auto now = env_->nownanos();
    auto elapsed = now - start_;
    if (reset) {
      start_ = now;
    }
    return elapsed;
  }

 private:
  env* const env_;
  uint64_t start_;
};

} // namespace rocksdb
