// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * statistics to analyze the performance of a db. pointer for statistics object
 * is managed by options class.
 */
public class statistics {

  private final long statshandle_;

  public statistics(long statshandle) {
    statshandle_ = statshandle;
  }

  public long gettickercount(tickertype tickertype) {
    assert(isinitialized());
    return gettickercount0(tickertype.getvalue(), statshandle_);
  }

  public histogramdata gehistogramdata(histogramtype histogramtype) {
    assert(isinitialized());
    histogramdata hist = gehistogramdata0(
        histogramtype.getvalue(), statshandle_);
    return hist;
  }

  private boolean isinitialized() {
    return (statshandle_ != 0);
  }

  private native long gettickercount0(int tickertype, long handle);
  private native histogramdata gehistogramdata0(int histogramtype, long handle);
}
