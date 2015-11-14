// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.test;

import org.rocksdb.*;

public class statscallbackmock implements statisticscollectorcallback {
  public int tickercallbackcount = 0;
  public int histcallbackcount = 0;

  public void tickercallback(tickertype tickertype, long tickercount) {
    tickercallbackcount++;
  }

  public void histogramcallback(histogramtype histtype,
      histogramdata histdata) {
    histcallbackcount++;
  }
}
