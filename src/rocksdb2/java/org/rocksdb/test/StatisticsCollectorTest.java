// copyright (c) 2014, moorecoin, inc.  all rights reserved.
// this source code is licensed under the bsd-style license found in the
// license file in the root directory of this source tree. an additional grant
// of patent rights can be found in the patents file in the same directory.

package org.rocksdb.test;

import java.util.collections;
import org.rocksdb.*;

public class statisticscollectortest {
  static final string db_path = "/tmp/backupablejni_db";
  static {
    rocksdb.loadlibrary();
  }

  public static void main(string[] args) 
      throws interruptedexception, rocksdbexception {
    options opt = new options().createstatistics().setcreateifmissing(true);
    statistics stats = opt.statisticsptr();

    rocksdb db = rocksdb.open(db_path);

    statscallbackmock callback = new statscallbackmock();
    statscollectorinput statsinput = new statscollectorinput(stats, callback);
    
    statisticscollector statscollector = new statisticscollector(
        collections.singletonlist(statsinput), 100);
    statscollector.start();

    thread.sleep(1000);

    assert(callback.tickercallbackcount > 0);
    assert(callback.histcallbackcount > 0);

    statscollector.shutdown(1000);

    db.close();
    opt.dispose();

    system.out.println("stats collector test passed.!");
  }
}
