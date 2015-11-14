// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * contains all information necessary to collect statistics from one instance
 * of db statistics.
 */
public class statscollectorinput {
  private final statistics _statistics;
  private final statisticscollectorcallback _statscallback;
      
  /**
   * constructor for statscollectorinput.
   * 
   * @param statistics reference of db statistics.
   * @param statscallback reference of statistics callback interface.
   */
  public statscollectorinput(statistics statistics,
      statisticscollectorcallback statscallback) {
    _statistics = statistics;
    _statscallback = statscallback;
  }
  
  public statistics getstatistics() {
    return _statistics;
  }
  
  public statisticscollectorcallback getcallback() {
    return _statscallback;
  }
}
