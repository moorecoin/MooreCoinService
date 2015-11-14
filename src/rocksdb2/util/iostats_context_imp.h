//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once
#include "rocksdb/iostats_context.h"

#ifndef ios_cross_compile

// increment a specific counter by the specified value
#define iostats_add(metric, value)     \
  (iostats_context.metric += value)

// increase metric value only when it is positive
#define iostats_add_if_positive(metric, value)   \
  if (value > 0) { iostats_add(metric, value); }

// reset a specific counter to zero
#define iostats_reset(metric)          \
  (iostats_context.metric = 0)

// reset all counters to zero
#define iostats_reset_all()                        \
  (iostats_context.reset())

#define iostats_set_thread_pool_id(value)      \
  (iostats_context.thread_pool_id = value)

#define iostats_thread_pool_id()               \
  (iostats_context.thread_pool_id)

#define iostats(metric)                        \
  (iostats_context.metric)

#else  // ios_cross_compile

#define iostats_add(metric, value)
#define iostats_add_if_positive(metric, value)
#define iostats_reset(metric)
#define iostats_reset_all()
#define iostats_set_thread_pool_id(value)
#define iostats_thread_pool_id()
#define iostats(metric) 0

#endif  // ios_cross_compile
