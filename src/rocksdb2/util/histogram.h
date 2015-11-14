//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#pragma once
#include "rocksdb/statistics.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>

namespace rocksdb {

class histogrambucketmapper {
 public:

  histogrambucketmapper();

  // converts a value to the bucket index.
  const size_t indexforvalue(const uint64_t value) const;
  // number of buckets required.

  const size_t bucketcount() const {
    return bucketvalues_.size();
  }

  uint64_t lastvalue() const {
    return maxbucketvalue_;
  }

  uint64_t firstvalue() const {
    return minbucketvalue_;
  }

  uint64_t bucketlimit(const uint64_t bucketnumber) const {
    assert(bucketnumber < bucketcount());
    return bucketvalues_[bucketnumber];
  }

 private:
  const std::vector<uint64_t> bucketvalues_;
  const uint64_t maxbucketvalue_;
  const uint64_t minbucketvalue_;
  std::map<uint64_t, uint64_t> valueindexmap_;
};

class histogramimpl {
 public:
  virtual void clear();
  virtual bool empty();
  virtual void add(uint64_t value);
  void merge(const histogramimpl& other);

  virtual std::string tostring() const;

  virtual double median() const;
  virtual double percentile(double p) const;
  virtual double average() const;
  virtual double standarddeviation() const;
  virtual void data(histogramdata * const data) const;

 private:
  // to be able to use histogramimpl as thread local variable, its constructor
  // has to be static. that's why we're using manually values from bucketmapper
  double min_ = 1000000000;  // this is bucketmapper:lastvalue()
  double max_ = 0;
  double num_ = 0;
  double sum_ = 0;
  double sum_squares_ = 0;
  uint64_t buckets_[138] = {0};  // this is bucketmapper::bucketcount()
};

}  // namespace rocksdb
