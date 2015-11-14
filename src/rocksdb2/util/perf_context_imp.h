//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include "rocksdb/perf_context.h"
#include "util/stop_watch.h"

namespace rocksdb {

#if defined(nperf_context) || defined(ios_cross_compile)

#define perf_timer_guard(metric)
#define perf_timer_measure(metric)
#define perf_timer_stop(metric)
#define perf_timer_start(metric)
#define perf_counter_add(metric, value)

#else

extern __thread perflevel perf_level;

class perfsteptimer {
 public:
  perfsteptimer(uint64_t* metric)
    : enabled_(perf_level >= perflevel::kenabletime),
      env_(enabled_ ? env::default() : nullptr),
      start_(0),
      metric_(metric) {
  }

  ~perfsteptimer() {
    stop();
  }

  void start() {
    if (enabled_) {
      start_ = env_->nownanos();
    }
  }

  void measure() {
    if (start_) {
      uint64_t now = env_->nownanos();
      *metric_ += now - start_;
      start_ = now;
    }
  }

  void stop() {
    if (start_) {
      *metric_ += env_->nownanos() - start_;
      start_ = 0;
    }
  }

 private:
  const bool enabled_;
  env* const env_;
  uint64_t start_;
  uint64_t* metric_;
};

// stop the timer and update the metric
#define perf_timer_stop(metric)          \
  perf_step_timer_ ## metric.stop();

#define perf_timer_start(metric)          \
  perf_step_timer_ ## metric.start();

// declare and set start time of the timer
#define perf_timer_guard(metric)           \
  perfsteptimer perf_step_timer_ ## metric(&(perf_context.metric));          \
  perf_step_timer_ ## metric.start();

// update metric with time elapsed since last start. start time is reset
// to current timestamp.
#define perf_timer_measure(metric)        \
  perf_step_timer_ ## metric.measure();

// increase metric value
#define perf_counter_add(metric, value)     \
  perf_context.metric += value;

#endif

}
