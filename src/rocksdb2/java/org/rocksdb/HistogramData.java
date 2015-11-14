// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

public class histogramdata {
  private final double median_;
  private final double percentile95_;
  private final double percentile99_;
  private final double average_;
  private final double standarddeviation_;

  public histogramdata(double median, double percentile95,
      double percentile99, double average, double standarddeviation) {
    median_ = median;
    percentile95_ = percentile95;
    percentile99_ = percentile99;
    average_ = average;
    standarddeviation_ = standarddeviation;
  }

  public double getmedian() {
    return median_;
  }

  public double getpercentile95() {
    return percentile95_;
  }

  public double getpercentile99() {
    return percentile99_;
  }

  public double getaverage() {
    return average_;
  }

  public double getstandarddeviation() {
    return standarddeviation_;
  }
}
