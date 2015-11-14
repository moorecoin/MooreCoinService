// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb;

import java.util.list;
import java.util.concurrent.arrayblockingqueue;
import java.util.concurrent.executors;
import java.util.concurrent.executorservice;
import java.util.concurrent.timeunit;
import java.util.concurrent.atomic.atomicboolean;

/**
 * helper class to collect db statistics periodically at a period specified in
 * constructor. callback function (provided in constructor) is called with
 * every statistics collection.
 *
 * caller should call start() to start statistics collection. shutdown() should
 * be called to stop stats collection and should be called before statistics (
 * provided in constructor) reference has been disposed.
 */
public class statisticscollector {
  private final list<statscollectorinput> _statscollectorinputlist;
  private final executorservice _executorservice;
  private final int _statscollectioninterval;
  private volatile boolean _isrunning = true;

  /**
   * constructor for statistics collector.
   * 
   * @param statscollectorinputlist list of statistics collector input.
   * @param statscollectionintervalinmilliseconds statistics collection time 
   *        period (specified in milliseconds).
   */
  public statisticscollector(list<statscollectorinput> statscollectorinputlist,
      int statscollectionintervalinmilliseconds) {
    _statscollectorinputlist = statscollectorinputlist;
    _statscollectioninterval = statscollectionintervalinmilliseconds;

    _executorservice = executors.newsinglethreadexecutor();
  }

  public void start() {
    _executorservice.submit(collectstatistics());
  }

  /**
   * shuts down statistics collector.
   * 
   * @param shutdowntimeout time in milli-seconds to wait for shutdown before
   *        killing the collection process.
   */
  public void shutdown(int shutdowntimeout) throws interruptedexception {
    _isrunning = false;

    _executorservice.shutdownnow();
    // wait for collectstatistics runnable to finish so that disposal of
    // statistics does not cause any exceptions to be thrown.
    _executorservice.awaittermination(shutdowntimeout, timeunit.milliseconds);
  }

  private runnable collectstatistics() {
    return new runnable() {

      @override
      public void run() {
        while (_isrunning) {
          try {
            if(thread.currentthread().isinterrupted()) {
              break;
            }  
            for(statscollectorinput statscollectorinput :
                _statscollectorinputlist) {
              statistics statistics = statscollectorinput.getstatistics();
              statisticscollectorcallback statscallback =
                  statscollectorinput.getcallback();
              
                // collect ticker data
              for(tickertype ticker : tickertype.values()) {
                long tickervalue = statistics.gettickercount(ticker);
                statscallback.tickercallback(ticker, tickervalue);
              }

              // collect histogram data
              for(histogramtype histogramtype : histogramtype.values()) {
                histogramdata histogramdata =
                    statistics.gehistogramdata(histogramtype);
                statscallback.histogramcallback(histogramtype, histogramdata);
              }

              thread.sleep(_statscollectioninterval);
            }
          }
          catch (interruptedexception e) {
            thread.currentthread().interrupt();
            break;
          }
          catch (exception e) {
            throw new runtimeexception("error while calculating statistics", e);
          }
        }
      }
    };
  }
}
