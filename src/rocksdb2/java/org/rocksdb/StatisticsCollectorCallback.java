// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

/**
 * callback interface provided to statisticscollector.
 * 
 * thread safety:
 * statisticscollector doesn't make any guarantees about thread safety. 
 * if the same reference of statisticscollectorcallback is passed to multiple
 * statisticscollector references, then its the responsibility of the 
 * user to make statisticscollectorcallback's implementation thread-safe.
 * 
 * @param tickertype
 * @param tickercount
*/
public interface statisticscollectorcallback {
  /**
   * callback function to get ticker values.
   * @param tickertype ticker type.
   * @param tickercount value of ticker type.
  */
  void tickercallback(tickertype tickertype, long tickercount);

  /**
   * callback function to get histogram values.
   * @param histtype histogram type.
   * @param histdata histogram data.
  */
  void histogramcallback(histogramtype histtype, histogramdata histdata);
}
