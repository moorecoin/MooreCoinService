//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
// copyright (c) 2011 the leveldb authors. all rights reserved.
// use of this source code is governed by a bsd-style license that can be
// found in the license file. see the authors file for names of contributors.

#include "util/histogram.h"

#include <cassert>
#include <math.h>
#include <stdio.h>
#include "port/port.h"

namespace rocksdb {

histogrambucketmapper::histogrambucketmapper()
    :
      // add newer bucket index here.
      // should be alwyas added in sorted order.
      // if you change this, you also need to change
      // size of array buckets_ in histogramimpl
      bucketvalues_(
          {1,         2,         3,         4,         5,         6,
           7,         8,         9,         10,        12,        14,
           16,        18,        20,        25,        30,        35,
           40,        45,        50,        60,        70,        80,
           90,        100,       120,       140,       160,       180,
           200,       250,       300,       350,       400,       450,
           500,       600,       700,       800,       900,       1000,
           1200,      1400,      1600,      1800,      2000,      2500,
           3000,      3500,      4000,      4500,      5000,      6000,
           7000,      8000,      9000,      10000,     12000,     14000,
           16000,     18000,     20000,     25000,     30000,     35000,
           40000,     45000,     50000,     60000,     70000,     80000,
           90000,     100000,    120000,    140000,    160000,    180000,
           200000,    250000,    300000,    350000,    400000,    450000,
           500000,    600000,    700000,    800000,    900000,    1000000,
           1200000,   1400000,   1600000,   1800000,   2000000,   2500000,
           3000000,   3500000,   4000000,   4500000,   5000000,   6000000,
           7000000,   8000000,   9000000,   10000000,  12000000,  14000000,
           16000000,  18000000,  20000000,  25000000,  30000000,  35000000,
           40000000,  45000000,  50000000,  60000000,  70000000,  80000000,
           90000000,  100000000, 120000000, 140000000, 160000000, 180000000,
           200000000, 250000000, 300000000, 350000000, 400000000, 450000000,
           500000000, 600000000, 700000000, 800000000, 900000000, 1000000000}),
      maxbucketvalue_(bucketvalues_.back()),
      minbucketvalue_(bucketvalues_.front()) {
  for (size_t i =0; i < bucketvalues_.size(); ++i) {
    valueindexmap_[bucketvalues_[i]] = i;
  }
}

const size_t histogrambucketmapper::indexforvalue(const uint64_t value) const {
  if (value >= maxbucketvalue_) {
    return bucketvalues_.size() - 1;
  } else if ( value >= minbucketvalue_ ) {
    std::map<uint64_t, uint64_t>::const_iterator lowerbound =
      valueindexmap_.lower_bound(value);
    if (lowerbound != valueindexmap_.end()) {
      return lowerbound->second;
    } else {
      return 0;
    }
  } else {
    return 0;
  }
}

namespace {
  const histogrambucketmapper bucketmapper;
}

void histogramimpl::clear() {
  min_ = bucketmapper.lastvalue();
  max_ = 0;
  num_ = 0;
  sum_ = 0;
  sum_squares_ = 0;
  memset(buckets_, 0, sizeof buckets_);
}

bool histogramimpl::empty() { return sum_squares_ == 0; }

void histogramimpl::add(uint64_t value) {
  const size_t index = bucketmapper.indexforvalue(value);
  buckets_[index] += 1;
  if (min_ > value) min_ = value;
  if (max_ < value) max_ = value;
  num_++;
  sum_ += value;
  sum_squares_ += (value * value);
}

void histogramimpl::merge(const histogramimpl& other) {
  if (other.min_ < min_) min_ = other.min_;
  if (other.max_ > max_) max_ = other.max_;
  num_ += other.num_;
  sum_ += other.sum_;
  sum_squares_ += other.sum_squares_;
  for (unsigned int b = 0; b < bucketmapper.bucketcount(); b++) {
    buckets_[b] += other.buckets_[b];
  }
}

double histogramimpl::median() const {
  return percentile(50.0);
}

double histogramimpl::percentile(double p) const {
  double threshold = num_ * (p / 100.0);
  double sum = 0;
  for (unsigned int b = 0; b < bucketmapper.bucketcount(); b++) {
    sum += buckets_[b];
    if (sum >= threshold) {
      // scale linearly within this bucket
      double left_point = (b == 0) ? 0 : bucketmapper.bucketlimit(b-1);
      double right_point = bucketmapper.bucketlimit(b);
      double left_sum = sum - buckets_[b];
      double right_sum = sum;
      double pos = 0;
      double right_left_diff = right_sum - left_sum;
      if (right_left_diff != 0) {
       pos = (threshold - left_sum) / (right_sum - left_sum);
      }
      double r = left_point + (right_point - left_point) * pos;
      if (r < min_) r = min_;
      if (r > max_) r = max_;
      return r;
    }
  }
  return max_;
}

double histogramimpl::average() const {
  if (num_ == 0.0) return 0;
  return sum_ / num_;
}

double histogramimpl::standarddeviation() const {
  if (num_ == 0.0) return 0;
  double variance = (sum_squares_ * num_ - sum_ * sum_) / (num_ * num_);
  return sqrt(variance);
}

std::string histogramimpl::tostring() const {
  std::string r;
  char buf[200];
  snprintf(buf, sizeof(buf),
           "count: %.0f  average: %.4f  stddev: %.2f\n",
           num_, average(), standarddeviation());
  r.append(buf);
  snprintf(buf, sizeof(buf),
           "min: %.4f  median: %.4f  max: %.4f\n",
           (num_ == 0.0 ? 0.0 : min_), median(), max_);
  r.append(buf);
  snprintf(buf, sizeof(buf),
           "percentiles: "
           "p50: %.2f p75: %.2f p99: %.2f p99.9: %.2f p99.99: %.2f\n",
           percentile(50), percentile(75), percentile(99), percentile(99.9),
           percentile(99.99));
  r.append(buf);
  r.append("------------------------------------------------------\n");
  const double mult = 100.0 / num_;
  double sum = 0;
  for (unsigned int b = 0; b < bucketmapper.bucketcount(); b++) {
    if (buckets_[b] <= 0.0) continue;
    sum += buckets_[b];
    snprintf(buf, sizeof(buf),
             "[ %7lu, %7lu ) %8lu %7.3f%% %7.3f%% ",
             // left
             (unsigned long)((b == 0) ? 0 : bucketmapper.bucketlimit(b-1)),
             (unsigned long)bucketmapper.bucketlimit(b), // right
             (unsigned long)buckets_[b],                 // count
             (mult * buckets_[b]),        // percentage
             (mult * sum));               // cumulative percentage
    r.append(buf);

    // add hash marks based on percentage; 20 marks for 100%.
    int marks = static_cast<int>(20*(buckets_[b] / num_) + 0.5);
    r.append(marks, '#');
    r.push_back('\n');
  }
  return r;
}

void histogramimpl::data(histogramdata * const data) const {
  assert(data);
  data->median = median();
  data->percentile95 = percentile(95);
  data->percentile99 = percentile(99);
  data->average = average();
  data->standard_deviation = standarddeviation();
}

} // namespace levedb
