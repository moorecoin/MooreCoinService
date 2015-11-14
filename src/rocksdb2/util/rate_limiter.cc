//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/rate_limiter.h"
#include "rocksdb/env.h"

namespace rocksdb {


// pending request
struct genericratelimiter::req {
  explicit req(int64_t bytes, port::mutex* mu) :
    bytes(bytes), cv(mu), granted(false) {}
  int64_t bytes;
  port::condvar cv;
  bool granted;
};


genericratelimiter::genericratelimiter(
    int64_t rate_bytes_per_sec,
    int64_t refill_period_us,
    int32_t fairness)
  : refill_period_us_(refill_period_us),
    refill_bytes_per_period_(rate_bytes_per_sec * refill_period_us / 1000000.0),
    env_(env::default()),
    stop_(false),
    exit_cv_(&request_mutex_),
    requests_to_wait_(0),
    total_requests_{0, 0},
    total_bytes_through_{0, 0},
    available_bytes_(0),
    next_refill_us_(env_->nowmicros()),
    fairness_(fairness > 100 ? 100 : fairness),
    rnd_((uint32_t)time(nullptr)),
    leader_(nullptr) {
  total_bytes_through_[0] = 0;
  total_bytes_through_[1] = 0;
}

genericratelimiter::~genericratelimiter() {
  mutexlock g(&request_mutex_);
  stop_ = true;
  requests_to_wait_ = queue_[env::io_low].size() + queue_[env::io_high].size();
  for (auto& r : queue_[env::io_high]) {
    r->cv.signal();
  }
  for (auto& r : queue_[env::io_low]) {
    r->cv.signal();
  }
  while (requests_to_wait_ > 0) {
    exit_cv_.wait();
  }
}

void genericratelimiter::request(int64_t bytes, const env::iopriority pri) {
  assert(bytes < refill_bytes_per_period_);

  mutexlock g(&request_mutex_);
  if (stop_) {
    return;
  }

  ++total_requests_[pri];

  if (available_bytes_ >= bytes) {
    // refill thread assigns quota and notifies requests waiting on
    // the queue under mutex. so if we get here, that means nobody
    // is waiting?
    available_bytes_ -= bytes;
    total_bytes_through_[pri] += bytes;
    return;
  }

  // request cannot be satisfied at this moment, enqueue
  req r(bytes, &request_mutex_);
  queue_[pri].push_back(&r);

  do {
    bool timedout = false;
    // leader election, candidates can be:
    // (1) a new incoming request,
    // (2) a previous leader, whose quota has not been not assigned yet due
    //     to lower priority
    // (3) a previous waiter at the front of queue, who got notified by
    //     previous leader
    if (leader_ == nullptr &&
        ((!queue_[env::io_high].empty() &&
            &r == queue_[env::io_high].front()) ||
         (!queue_[env::io_low].empty() &&
            &r == queue_[env::io_low].front()))) {
      leader_ = &r;
      timedout = r.cv.timedwait(next_refill_us_);
    } else {
      // not at the front of queue or an leader has already been elected
      r.cv.wait();
    }

    // request_mutex_ is held from now on
    if (stop_) {
      --requests_to_wait_;
      exit_cv_.signal();
      return;
    }

    // make sure the waken up request is always the header of its queue
    assert(r.granted ||
           (!queue_[env::io_high].empty() &&
            &r == queue_[env::io_high].front()) ||
           (!queue_[env::io_low].empty() &&
            &r == queue_[env::io_low].front()));
    assert(leader_ == nullptr ||
           (!queue_[env::io_high].empty() &&
            leader_ == queue_[env::io_high].front()) ||
           (!queue_[env::io_low].empty() &&
            leader_ == queue_[env::io_low].front()));

    if (leader_ == &r) {
      // waken up from timedwait()
      if (timedout) {
        // time to do refill!
        refill();

        // re-elect a new leader regardless. this is to simplify the
        // election handling.
        leader_ = nullptr;

        // notify the header of queue if current leader is going away
        if (r.granted) {
          // current leader already got granted with quota. notify header
          // of waiting queue to participate next round of election.
          assert((queue_[env::io_high].empty() ||
                    &r != queue_[env::io_high].front()) &&
                 (queue_[env::io_low].empty() ||
                    &r != queue_[env::io_low].front()));
          if (!queue_[env::io_high].empty()) {
            queue_[env::io_high].front()->cv.signal();
          } else if (!queue_[env::io_low].empty()) {
            queue_[env::io_low].front()->cv.signal();
          }
          // done
          break;
        }
      } else {
        // spontaneous wake up, need to continue to wait
        assert(!r.granted);
        leader_ = nullptr;
      }
    } else {
      // waken up by previous leader:
      // (1) if requested quota is granted, it is done.
      // (2) if requested quota is not granted, this means current thread
      // was picked as a new leader candidate (previous leader got quota).
      // it needs to participate leader election because a new request may
      // come in before this thread gets waken up. so it may actually need
      // to do wait() again.
      assert(!timedout);
    }
  } while (!r.granted);
}

void genericratelimiter::refill() {
  next_refill_us_ = env_->nowmicros() + refill_period_us_;
  // carry over the left over quota from the last period
  if (available_bytes_ < refill_bytes_per_period_) {
    available_bytes_ += refill_bytes_per_period_;
  }

  int use_low_pri_first = rnd_.onein(fairness_) ? 0 : 1;
  for (int q = 0; q < 2; ++q) {
    auto use_pri = (use_low_pri_first == q) ? env::io_low : env::io_high;
    auto* queue = &queue_[use_pri];
    while (!queue->empty()) {
      auto* next_req = queue->front();
      if (available_bytes_ < next_req->bytes) {
        break;
      }
      available_bytes_ -= next_req->bytes;
      total_bytes_through_[use_pri] += next_req->bytes;
      queue->pop_front();

      next_req->granted = true;
      if (next_req != leader_) {
        // quota granted, signal the thread
        next_req->cv.signal();
      }
    }
  }
}

ratelimiter* newgenericratelimiter(
    int64_t rate_bytes_per_sec, int64_t refill_period_us, int32_t fairness) {
  return new genericratelimiter(
      rate_bytes_per_sec, refill_period_us, fairness);
}

}  // namespace rocksdb
