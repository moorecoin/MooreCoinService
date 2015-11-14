//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.

#include "util/sync_point.h"

#ifndef ndebug
namespace rocksdb {

syncpoint* syncpoint::getinstance() {
  static syncpoint sync_point;
  return &sync_point;
}

void syncpoint::loaddependency(const std::vector<dependency>& dependencies) {
  successors_.clear();
  predecessors_.clear();
  cleared_points_.clear();
  for (const auto& dependency : dependencies) {
    successors_[dependency.predecessor].push_back(dependency.successor);
    predecessors_[dependency.successor].push_back(dependency.predecessor);
  }
}

bool syncpoint::predecessorsallcleared(const std::string& point) {
  for (const auto& pred : predecessors_[point]) {
    if (cleared_points_.count(pred) == 0) {
      return false;
    }
  }
  return true;
}

void syncpoint::enableprocessing() {
  std::unique_lock<std::mutex> lock(mutex_);
  enabled_ = true;
}

void syncpoint::disableprocessing() {
  std::unique_lock<std::mutex> lock(mutex_);
  enabled_ = false;
}

void syncpoint::cleartrace() {
  std::unique_lock<std::mutex> lock(mutex_);
  cleared_points_.clear();
}

void syncpoint::process(const std::string& point) {
  std::unique_lock<std::mutex> lock(mutex_);

  if (!enabled_) return;

  while (!predecessorsallcleared(point)) {
    cv_.wait(lock);
  }

  cleared_points_.insert(point);
  cv_.notify_all();
}

}  // namespace rocksdb
#endif  // ndebug
