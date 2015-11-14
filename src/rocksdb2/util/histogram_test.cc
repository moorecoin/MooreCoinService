//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#include "util/histogram.h"

#include "util/testharness.h"

namespace rocksdb {

class histogramtest { };

test(histogramtest, basicoperation) {

  histogramimpl histogram;
  for (uint64_t i = 1; i <= 100; i++) {
    histogram.add(i);
  }

  {
    double median = histogram.median();
    // assert_le(median, 50);
    assert_gt(median, 0);
  }

  {
    double percentile100 = histogram.percentile(100.0);
    assert_le(percentile100, 100.0);
    assert_gt(percentile100, 0.0);
    double percentile99 = histogram.percentile(99.0);
    double percentile85 = histogram.percentile(85.0);
    assert_le(percentile99, 99.0);
    assert_true(percentile99 >= percentile85);
  }

  assert_eq(histogram.average(), 50.5); // avg is acurately caluclated.
}

test(histogramtest, emptyhistogram) {
  histogramimpl histogram;
  assert_eq(histogram.median(), 0.0);
  assert_eq(histogram.percentile(85.0), 0.0);
  assert_eq(histogram.average(), 0.0);
}

test(histogramtest, clearhistogram) {
  histogramimpl histogram;
  for (uint64_t i = 1; i <= 100; i++) {
    histogram.add(i);
  }
  histogram.clear();
  assert_eq(histogram.median(), 0);
  assert_eq(histogram.percentile(85.0), 0);
  assert_eq(histogram.average(), 0);
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  return rocksdb::test::runalltests();
}
