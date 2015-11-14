//  copyright (c) 2014, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
#pragma once

#include <condition_variable>
#include <mutex>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#ifdef ndebug
#define test_sync_point(x)
#else

namespace rocksdb {

// this class provides facility to reproduce race conditions deterministically
// in unit tests.
// developer could specify sync points in the codebase via test_sync_point.
// each sync point represents a position in the execution stream of a thread.
// in the unit test, 'happens after' relationship among sync points could be
// setup via syncpoint::loaddependency, to reproduce a desired interleave of
// threads execution.
// refer to (dbtest,transactionlogiteratorrace), for an exmaple use case.

class syncpoint {
 public:
  static syncpoint* getinstance();

  struct dependency {
    std::string predecessor;
    std::string successor;
  };
  // call once at the beginning of a test to setup the dependency between
  // sync points
  void loaddependency(const std::vector<dependency>& dependencies);

  // enable sync point processing (disabled on startup)
  void enableprocessing();

  // disable sync point processing
  void disableprocessing();

  // remove the execution trace of all sync points
  void cleartrace();

  // triggered by test_sync_point, blocking execution until all predecessors
  // are executed.
  void process(const std::string& point);

  // todo: it might be useful to provide a function that blocks until all
  // sync points are cleared.

 private:
  bool predecessorsallcleared(const std::string& point);

  // successor/predecessor map loaded from loaddependency
  std::unordered_map<std::string, std::vector<std::string>> successors_;
  std::unordered_map<std::string, std::vector<std::string>> predecessors_;

  std::mutex mutex_;
  std::condition_variable cv_;
  // sync points that have been passed through
  std::unordered_set<std::string> cleared_points_;
  bool enabled_ = false;
};

}  // namespace rocksdb

// use test_sync_point to specify sync points inside code base.
// sync points can have happens-after depedency on other sync points,
// configured at runtime via syncpoint::loaddependency. this could be
// utilized to re-produce race conditions between threads.
// see transactionlogiteratorrace in db_test.cc for an example use case.
// test_sync_point is no op in release build.
#define test_sync_point(x) rocksdb::syncpoint::getinstance()->process(x)
#endif  // ndebug
